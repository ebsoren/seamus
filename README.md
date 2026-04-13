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

### Build a binary to a specific directory

```bash
bazel build //<package>:<target> && cp bazel-bin/<package>/<target> /path/to/destination/
```

For example, to build the crawler and place it in `~/bin/`:
```bash
bazel build //crawler:crawler && cp bazel-bin/crawler/crawler ~/bin/
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

### Running on a VM
See [VMs.md](VMs.md) for instructions on provisioning a linux VM and running the crawler as a `systemd` service.

### Clean
Remove all bazel caches (build server cache, test results, executables, symlinks, etc...)
```bash
bazel clean --expunge
```
