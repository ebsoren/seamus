# seamus
Engine (non-hw) code for Seamus the Search Engine (W26)


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

### Clean
Remove all bazel caches (build server cache, test results, executables, symlinks, etc...)
```bash
bazel clean --expunge
```
