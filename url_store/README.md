# URL Store

Per-machine registry of every URL this node has seen: seed/domain distance, anchor text, crawl state, title/body lengths, etc. Consumed by the crawler (to decide what's new, to feed the frontier) and the ranker (as a friend class). Exposes an RPC listener so peer machines can forward URLs they discovered that hash to this shard.

## Files

- `url_store.h` — the `UrlStore` class and its supporting types.
  - `UrlShard` — one of `URL_NUM_SHARDS` in-memory partitions. Each shard holds an `unordered_map<string, UrlData>` and its own mutex. Sharding is by `DefaultHash<string>(url) % URL_NUM_SHARDS`, so per-URL work only contends on one shard lock.
  - `UrlAnchorData` — a `(anchor_text*, freq)` view returned by `getUrlAnchorInfo`. The actual anchor strings live in the global `id_to_anchor` table and are referenced by id in each `UrlData::anchor_freqs`, so duplicate anchor text is stored once.
  - Public accessors: `hasUrl`, `hasCrawled`, `markCrawled`, `getUrl`, `getTitle`, `getUrlNumEncountered`, `getUrlSeedDistance`, `getUrlAnchorInfo`. All lowercase the URL first (`lowercase_url`) and take the shard lock.
  - Mutators: `updateUrl` (inserts/merges a URL with anchor text, distances, encounter count, and priority bucket), `updateTitle` / `updateTitleLen` / `updateBodyLen`, `markCrawled`.
  - RPC entry points: `manage_frontier_and_update_url` and `batch_manage_frontier_and_update_url` handle single and batched `URLStoreUpdateRequest`s — they call `updateUrl` and, on a *new* URL, push a `CrawlTarget` into the `DomainCarousel` at the computed priority.
  - Metrics hooks: `record_local_urls` / `record_rpc_urls` accumulate locally and flush every 100 calls via `metric_submit` (wired to `CrawlerInstrumentation`). `MemStats` / `mem_stats()` report per-shard allocated footprint for memory instrumentation.
- `url_store.cpp` — implementation.
  - Constructor: optionally `readFromFile()` for crash recovery (skipped when `URL_FROM_SCRATCH`), then stands up the `RPCListener` on `URL_STORE_PORT` with `URL_STORE_NUM_THREADS` and detaches the listener thread. A periodic `persist_store_thread` exists but is currently disabled.
  - `client_handler` — accepts an RPC, dispatches to single or batched update.
  - `addUrl_unlocked` — the core insert/merge used by `updateUrl` after locks are held. Assigns anchor ids via the global `anchor_to_id` / `id_to_anchor` dictionary (guarded by `global_mtx`).
  - `readFromFile` — per-worker recovery path that reloads shard state from disk.
- `BUILD` — Bazel target.

## Concurrency model

- **Sharded maps, global anchor dict.** Shard locks (`shards[i].mtx`) cover URL reads/writes; `global_mtx` covers the anchor-text dictionary and `anchor_to_id` / `id_to_anchor`. Routines that touch both (e.g. `getUrlAnchorInfo`) take the shard lock first, then `global_mtx`. Keep that order to avoid deadlock.
- **Atomics for counters.** `unique_url_count`, `total_url_count`, and the pending/call counters are `std::atomic` so fast-path bookkeeping doesn't need a lock.
- **RPC thread pool.** The `RPCListener` dispatches inbound requests onto its own threads; `client_handler` is called concurrently.
- **Shutdown.** Destructor sets `running=false`, notifies `shutdown_cv`, stops the listener, joins the listener thread, deletes the listener.

## Data flow

```
local crawler workers ──► record_local_urls / updateUrl ──┐
                                                          │
peer machines ──► RPC (URL_STORE_PORT) ──► client_handler ─┤
                                                          │
                                                          ▼
                              UrlStore (URL_NUM_SHARDS shards, shared anchor dict)
                                                          │
                                                          ├──► DomainCarousel (new URLs only)
                                                          │
                                                          ├──► CrawlerInstrumentation (metric_submit)
                                                          │
                                                          └──► Ranker (friend: reads UrlData directly)
```

URLs are lowercased before hashing and lookup, so casing differences don't create duplicate entries.
