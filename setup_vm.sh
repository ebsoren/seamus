#!/bin/bash
set -e

if [ -z "$1" ]; then
    echo "Usage: ./setup_vm.sh <machine_id>"
    echo "  e.g. ./setup_vm.sh 0"
    exit 1
fi

MACHINE_ID=$1
USER=$(whoami)

echo "Setting up VM for user=$USER, machine_id=$MACHINE_ID"

echo "$MACHINE_ID" > .machine_id

sudo mkdir -p /var/seamus/parser_output /var/seamus/index_output /var/seamus/urlstore_output
sudo chown -R $(whoami):$(whoami) /var/seamus
sudo chmod -R 755 /var/seamus

echo "Clearing output directories..."
rm -rf /var/seamus/parser_output/*
rm -rf /var/seamus/index_output/*
rm -rf /var/seamus/urlstore_output/*

echo "Done. Run ./run_crawler.sh to start the crawler."
