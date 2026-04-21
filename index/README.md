# Index

Offline indexer binary. Reads parser-output files and writes sorted, on-disk **index chunks** (posting lists + URL table + dictionary TOC) that the online serving side (`index_chunk/`, `index_manager/`) loads at query time.

## Files

- `Index.h` — declares `IndexChunk` and its on-disk record types.
  - `post { doc, loc }` — a single occurrence of a term (document id + location within that document; both 1-indexed so 0 is a sentinel).
  - `postings { posts, n_docs }` — in-memory posting list for one term.
  - `IndexChunk` — builds one chunk at a time in memory (`unordered_map<string, postings> index`, parallel `urls` table), flushes to disk when the chunk fills, then resets and starts the next chunk. Each worker owns its own `IndexChunk` and its own chunk id sequence.
- `Index.cpp` — implementation.
  - `index_file(path)` — streams a parser output file, assigns each document a 1-indexed id, appends `post`s to each term's list, and calls `flush()` when `docs_in_chunk_` or `posts_bytes_` hit their caps.
  - `sort_entries()` — alphabetizes the term table and drops keys whose first byte isn't `[a-z0-9]` (the 36-slot dictionary TOC can't represent them).
  - `persist()` — writes the chunk file in one pass: URL table (id → URL), then the alphabetized dictionary with per-letter/digit offset slots (`DICT_SLOTS = 36`), posting lists, and an optional skip list (currently gated off by `WRITE_SKIP_LIST`).
  - `flush()` / `reset()` — persist the current chunk, bump `chunk`, clear in-memory state for the next one.
- `main.cpp` — driver. Spawns `NUM_INDEXER_THREADS` workers; each worker stripes across parser output files (`parser_{i}_out.txt`) by `worker_id mod NUM_INDEXER_THREADS`, indexes them sequentially into its `IndexChunk`, and does a final `flush()` when the queue drains.
- `BUILD` — Bazel target.

## Chunk file layout

Written by `IndexChunk::persist()`:

```
┌──────────────────────────────────────────────┐
│ <8B urls_bytes>\n                            │  size of the URL table
├──────────────────────────────────────────────┤
│ <4B id> ' ' <url bytes> '\n'   (× N docs)    │  id → URL mapping
├──────────────────────────────────────────────┤
│ '\n'                                         │  separator
├──────────────────────────────────────────────┤
│ dictionary TOC: 36 uint64 offsets            │  a-z + 0-9 → first term offset
├──────────────────────────────────────────────┤
│ alphabetized term entries + posting lists    │  each term's <doc,loc> pairs
│ (optional skip list per term, gated off)     │
└──────────────────────────────────────────────┘
```

Documents and locations are 1-indexed; a `0` byte functions as a "new document" flag in the posting stream.

## Data flow

```
parser/*_out.txt        (one file per crawler worker)
       │
       ▼  striped: worker_i reads files where (file_idx % NUM_INDEXER_THREADS) == i
 Index main (NUM_INDEXER_THREADS threads)
       │
       ▼
 IndexChunk per worker  ──► in-memory term table fills ──► persist() on cap
       │                         │
       │                         └─► index_chunk_{worker}_{chunk}.txt
       ▼
        (chunk++, reset, continue)
```

Crash/restart safety: `IndexChunk`'s constructor scans for existing `index_chunk_{worker}_{chunk}.txt` files and advances `chunk` past them, and `persist()` opens with `"wx"` so a re-run never clobbers a chunk that's already on disk.

## Relationship to the rest of the system

`index/` produces chunks; `index_chunk/` (ISR / ranked-buffer / chunk manager) and `index_manager/` (per-shard query entry point used by `query/IndexServer`) consume them at serve time. Parser output is the input; query-side code never touches the indexer directly.
