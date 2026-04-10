# to make runnable:
# chmod +x run.sh
# then:
# ./run.sh

#!/bin/bash
set -e

# One-shot helper: ensures /var/seamus is writable by the current user, builds the benchmark binary, and runs it.

SEAMUS_DIR="/var/seamus"
SUBDIRS=("index_output" "parser_output")

if [ ! -d "$SEAMUS_DIR" ] || [ "$(stat -f '%u' "$SEAMUS_DIR")" != "$(id -u)" ]; then
    echo "Setting up $SEAMUS_DIR (requires sudo once)..."
    sudo mkdir -p "$SEAMUS_DIR"
    for sub in "${SUBDIRS[@]}"; do
        sudo mkdir -p "$SEAMUS_DIR/$sub"
    done
    sudo chown -R "$(id -u):$(id -g)" "$SEAMUS_DIR"
fi

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

bazel run //index-isr-validation:index_isr_validation "$@"
