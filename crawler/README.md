# Crawler

Distributed web crawler. `main.cpp` wires the components together; each worker thread pulls crawl targets from the domain carousel, fetches over HTTPS, parses, and persists results.

## Files

- `main.cpp` — driver. Initializes the carousel, bucket manager, URL store, robots manager, parser, listener, instrumentation, then spawns the worker pool.
- `domain_carousel.h` — `DomainCarousel`: in-memory per-domain queues of `CrawlTarget`s. Hash-slotted (`CRAWLER_CAROUSEL_SIZE`) with per-bucket locks so workers can claim disjoint ranges without contention. Enforces per-domain politeness.
- `bucket_manager.h` — `BucketManager`: on-disk priority frontier. Loads seed/persisted `bucket_p{0..N}` files into memory, feeds the carousel, and holds the backoff queue for domains that were temporarily rejected.
- `crawler_worker.h` — `crawler_worker`: the per-thread loop. Monitors a `[carousel_left, carousel_right]` slice of the carousel, fetches HTML, parses, emits URLs to the url store, and updates instrumentation.
- `crawler_listener.h` — `CrawlerListener`: RPC endpoint (`CRAWLER_LISTENER_PORT`). Receives `BatchCrawlTargetRequest`s from peer machines and routes each target into the correct priority bucket.
- `network_util.h` — shared `SSL_CTX` plus `https_get_once` (single HTTPS request, with redirect extraction).
- `crawler_metrics.h` — `MetricType` enum and `MetricUpdate` struct used by workers to report to instrumentation.
- `crawler_instrumentation.h` — `CrawlerInstrumentation`: per-worker metric queues, drained on an interval thread for logging/aggregation.
- `BUILD` — Bazel target.

## Data flow

```
peers ──► CrawlerListener ──┐
                            ├──► BucketManager (disk frontier + backoff)
seed files ─────────────────┘              │
                                           ▼
                                    DomainCarousel
                                           │
                         ┌─────────────────┼─────────────────┐
                         ▼                 ▼                 ▼
                     worker 0          worker 1     ...   worker N
                         │
            https_get_once → HtmlParser → UrlStore (new URLs) + disk (doc)
                         │
                         └──► CrawlerInstrumentation
```
