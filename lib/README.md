# lib

Shared utilities used across every binary: custom STL-style containers, string types, RPC primitives, threading helpers, and project-wide constants/logging.

## Files

| File | Description |
| --- | --- |
| `algorithm.h` | Generic algorithms (`binary_search`, sort/merge helpers) over our container types. |
| `array.h` | Fixed-size `array<T, N>` — a minimal `std::array` analogue. |
| `atomic_vector.h` | Mutex-guarded `vector<T>` wrapper for concurrent push/read. |
| `buffer.h` | Byte buffer with read/write helpers over file descriptors and sockets. |
| `chunk_manager_query.h` | Query-time structs (`NodeInfo`, `DocInfo`) shared between query tree and chunk manager. |
| `consts.h` | Project-wide constants (ports, shard counts, paths, log level, user agent). |
| `deque.h` | Double-ended queue with amortized O(1) push/pop at both ends. |
| `Frontier.h` | Disk-backed priority-bucket frontier used by the crawler's `BucketManager`. |
| `io.h` | Low-level `seamus_read`/`seamus_write` wrappers around `read`/`write` syscalls. |
| `joinable_thread_pool.h` | Thread pool with named waitgroups — submit under an id, `join(id)` waits only on that id. |
| `logger.h` | Level-filtered logger (`logger::debug/info/warn/error/instr`) gated by `LOG_LEVEL`. |
| `priority_queue.h` | Binary-heap priority queue over `vector<T>` with a configurable comparator. |
| `rpc_common.h` | Shared RPC wire primitives: framing, `send_string`/`recv_string`, endianness helpers. |
| `rpc_crawler.h` | `CrawlTarget` + serialization for the crawler↔crawler batch-target RPC. |
| `rpc_listener.h` | `RPCListener`: accept loop with a thread pool that dispatches each fd to a handler. |
| `rpc_query_handler.h` | Wire format for query fan-out: query string in, `LeanPageResponse` out. |
| `rpc_urlstore.h` | `URLStoreUpdateRequest` / batch variant — URL + anchor/distance metadata RPC. |
| `string_old.h` | Previous `string` implementation, kept for reference; no longer the active type. |
| `string.h` | Active `string` / `string_view` with small-string optimization (`MAX_SHORT_LENGTH`). |
| `thread_pool.h` | Plain `ThreadPool` — N workers pulling `std::function<void()>` off a shared queue. |
| `unordered_map.h` | Open-addressed hash map with `DefaultHash<T>`, `capacity()`, iterator-style access. |
| `url_filter.h` | Blocklist of banned keywords used to drop URLs before they reach the frontier. |
| `utf8.h` | UTF-8 / UTF-16 / Unicode codepoint conversion and validation. |
| `utils.h` | Misc helpers: `stem_word`, `file_exists`, `extract_domain`, `my_machine_id`, `get_priority_bucket`, etc. |
| `vector.h` | Dynamic array with explicit capacity/grow semantics, used everywhere in place of `std::vector`. |
