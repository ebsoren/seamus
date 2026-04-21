# Query

Parses user query strings into a boolean expression tree, fans the query out across all index machines, and merges the results. Consumed by the `htmlserver` binary to serve search requests.

## Files

- `expressions.h` — the query grammar and parser. Tokens: `"..."` → PHRASE, `-` → NOT, `|` → OR, space → AND, `(...)` grouping. `parse_query(string)` returns a `QueryNode` tree where sibling branches are OR'd and a descent path is AND'd. Also defines the small `UniquePtr` / `ParseError` utilities used by the parser.
- `query_helpers.h` — tree walkers used by consumers of `QueryNode`. `get_unique_words(root)` collects the distinct terms/phrases to look up; `get_word_rarity` is a stub hook for rarity-weighted ranking.
- `index_server.h` — `IndexServer`. RPC listener (`INDEX_SERVER_PORT`) that receives a raw query string from peer machines, hands it to the local `IndexManager` for evaluation against that machine's shard, and returns a `LeanPageResponse`. Backed by a `ThreadPool` so multiple peer requests run concurrently.
- `query_handler.h` — `QueryHandler`. The client-facing entry point. Takes a raw query string, fans it out over RPC to every machine's `IndexServer` (including loopback to the local one), awaits the per-shard `LeanPage` results via futures, and merges them into a single ranked result vector.
- `BUILD` — Bazel target.

## Data flow

```
raw query string
       │
       ▼
 QueryHandler (one per htmlserver)
       │  fan-out to NUM_MACHINES
       ├───► IndexServer @ machine 0 ──► IndexManager ──► shard 0 hits
       ├───► IndexServer @ machine 1 ──► IndexManager ──► shard 1 hits
       │         ...
       └───► IndexServer @ machine N ──► IndexManager ──► shard N hits
                                                            │
       ┌────────────────────────────────────────────────────┘
       ▼
 QueryHandler merges → vector<LeanPage>
```

Each `IndexServer` re-parses the raw query locally (via `expressions.h`) so the tree is reconstructed on the shard side rather than serialized over the wire.

## Usage in `htmlserver`

`htmlserver/main.cpp` owns one `IndexManager`, one `IndexServer`, and one `QueryHandler`:

```cpp
index_server  = new IndexServer(index_manager);
query_handler = new QueryHandler(index_server);
```

For each incoming HTTP search request, `serve_search_results` extracts the query term from the URL and calls:

```cpp
vector<LeanPage> results = query_handler->handle_client_req(term);
```

`QueryHandler` fans out across all shards (its own `IndexServer` handles the local shard; the rest are reached via RPC), and the merged `LeanPage` vector is rendered into the results page. So every htmlserver node acts as both a query coordinator (via `QueryHandler`) and a shard responder (via `IndexServer`) for peers.
