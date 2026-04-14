#!/bin/bash

set -euo pipefail

SRC_ROOT="/var/seamus"
DST_ROOT="/var/index"

if [ "$(id -u)" -ne 0 ]; then
    echo "Run as root (sudo)." >&2
    exit 1
fi

for sub in parser_output urlstore_output; do
    if [ ! -d "$SRC_ROOT/$sub" ]; then
        echo "Missing source: $SRC_ROOT/$sub" >&2
        exit 1
    fi
done

mkdir -p "$DST_ROOT/parser_output" "$DST_ROOT/urlstore_output"

echo "Source free space:"
df -h "$SRC_ROOT" | tail -n1
echo "Destination free space:"
df -h "$DST_ROOT" | tail -n1

SRC_SIZE_BYTES=$(du -sb "$SRC_ROOT/parser_output" "$SRC_ROOT/urlstore_output" | awk '{s+=$1} END {print s}')
DST_AVAIL_BYTES=$(df --output=avail -B1 "$DST_ROOT" | tail -n1)
echo "Source size: $(numfmt --to=iec "$SRC_SIZE_BYTES")"
echo "Destination available: $(numfmt --to=iec "$DST_AVAIL_BYTES")"

if [ "$DST_AVAIL_BYTES" -lt "$SRC_SIZE_BYTES" ]; then
    echo "Not enough free space at $DST_ROOT. Resize the disk first." >&2
    exit 1
fi

echo "Copying parser_output..."
rsync -a --info=progress2 "$SRC_ROOT/parser_output/" "$DST_ROOT/parser_output/"

echo "Copying urlstore_output..."
rsync -a --info=progress2 "$SRC_ROOT/urlstore_output/" "$DST_ROOT/urlstore_output/"

echo
echo "Done. Sizes:"

du -sh "$DST_ROOT/parser_output" "$DST_ROOT/urlstore_output"
