# seamus
Engine (non-hw) code for Seamus the Search Engine \
University of Michigan - System Design of a Search Engine (W26) \
🚟🚟🚟🚟🚟🚟🚟🚟🚟🚟🚟🚟🚟🚟🚟🚟🚟🚟🚟🚟🚟🚟🚟🚟🚟🚟🚟🚟🚟🚟


### Bazel Install
https://bazel.build/install \
(simply `brew install bazel` on macOS)


### Build

```bash
bazel build //...
```

### Tests

Run all tests:
```bash
bazel test //tests/...
```

Run a specific test:
```bash
bazel test //tests:vector_test
```

Note that benchmarks look like tests to bazel, to see actual output of anything within `/tests` we can use something like:
```bash
bazel run //tests:thread_pool_benchmark
```

### Clean
Remove all bazel caches (build server cache, test results, executables, symlinks, etc...)
```bash
bazel clean --expunge
```
