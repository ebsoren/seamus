# seamus
Engine (non-hw) code for Seamus the Search Engine (W26)

## Build

```bash
bazel build //...
```

## Tests

Run all tests:
```bash
bazel test //tests/...
```

Run a specific test:
```bash
bazel test //tests:vector_test
```

## Clean
Remove all bazel cache for the repo (tests, executables, symlinks, etc...)
```bash
bazel clean --expunge
```
