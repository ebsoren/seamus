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

echo "Expanding root partition to use full disk..."
sudo apt install -y cloud-guest-utils
sudo growpart /dev/nvme0n1 1 || true
sudo resize2fs /dev/nvme0n1p1 || true

sudo mkdir -p /var/seamus/parser_output /var/seamus/index_output /var/seamus/urlstore_output
sudo chown -R $(whoami):$(whoami) /var/seamus
sudo chmod -R 755 /var/seamus

echo "Configuring system limits..."
sudo sysctl -w fs.nr_open=1048576
sudo sysctl -w fs.file-max=1048576
grep -q "fs.nr_open" /etc/sysctl.conf || echo "fs.nr_open = 1048576" | sudo tee -a /etc/sysctl.conf > /dev/null
grep -q "fs.file-max" /etc/sysctl.conf || echo "fs.file-max = 1048576" | sudo tee -a /etc/sysctl.conf > /dev/null
grep -q "hard nofile" /etc/security/limits.conf || echo "* hard nofile 1048576" | sudo tee -a /etc/security/limits.conf > /dev/null
grep -q "soft nofile" /etc/security/limits.conf || echo "* soft nofile 1048576" | sudo tee -a /etc/security/limits.conf > /dev/null

read -p "Clear output directories? (y/N) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Clearing output directories..."
    rm -rf /var/seamus/parser_output/*
    rm -rf /var/seamus/urlstore_output/*
fi

echo "Done. Run ./run_crawler.sh to start the crawler."
