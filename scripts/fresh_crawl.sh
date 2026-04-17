#!/bin/bash
set -e

if [ -z "$1" ]; then
    echo "Usage: curl ... | bash -s -- <machine_id>"
    echo "  e.g. curl -sSL https://raw.githubusercontent.com/hershyz/seamus/main/scripts/fresh_crawl.sh | bash -s -- 9"
    exit 1
fi

MACHINE_ID=$1
USER=$(whoami)

echo "=== Fresh crawl setup for machine_id=$MACHINE_ID, user=$USER ==="

# ---- Stop existing services ----
echo "Stopping existing services..."
sudo systemctl stop crawler.service 2>/dev/null || true
sudo systemctl stop index.service 2>/dev/null || true
sudo systemctl stop htmlserver.service 2>/dev/null || true
sudo systemctl stop queryhandler.service 2>/dev/null || true
sudo systemctl stop indexserver.service 2>/dev/null || true
sudo systemctl stop urlstore.service 2>/dev/null || true

# Kill any stray processes
pkill -f bazel-bin/crawler/crawler 2>/dev/null || true
pkill -f bazel-bin/index/index 2>/dev/null || true

echo "All services stopped."

# ---- Clear all data directories ----
echo "Clearing all data directories..."
sudo rm -rf /var/seamus/parser_output/*
sudo rm -rf /var/seamus/index_output/*
sudo rm -rf /var/seamus/urlstore_output/*
sudo rm -rf /var/index/parser_output/* 2>/dev/null || true
sudo rm -rf /var/index/urlstore_output/* 2>/dev/null || true
echo "Data directories cleared."

# ---- Expand root partition ----
echo "Expanding root partition..."
sudo apt install -y cloud-guest-utils 2>/dev/null
sudo growpart /dev/nvme0n1 1 || true
sudo resize2fs /dev/nvme0n1p1 || true

# ---- Create directories ----
sudo mkdir -p /var/seamus/parser_output /var/seamus/index_output /var/seamus/urlstore_output
sudo chown -R "$USER":"$USER" /var/seamus
sudo chmod -R 755 /var/seamus

# ---- System limits ----
echo "Configuring system limits..."
sudo sysctl -w fs.nr_open=1048576
sudo sysctl -w fs.file-max=1048576
sudo touch /etc/sysctl.conf
grep -q "fs.nr_open" /etc/sysctl.conf || echo "fs.nr_open = 1048576" | sudo tee -a /etc/sysctl.conf > /dev/null
grep -q "fs.file-max" /etc/sysctl.conf || echo "fs.file-max = 1048576" | sudo tee -a /etc/sysctl.conf > /dev/null
grep -q "hard nofile" /etc/security/limits.conf || echo "* hard nofile 1048576" | sudo tee -a /etc/security/limits.conf > /dev/null
grep -q "soft nofile" /etc/security/limits.conf || echo "* soft nofile 1048576" | sudo tee -a /etc/security/limits.conf > /dev/null

# ---- Install dependencies ----
echo "Installing dependencies..."
sudo apt update
sudo apt install -y build-essential libssl-dev git npm iproute2 netcat-openbsd
sudo npm install -g @bazel/bazelisk 2>/dev/null || true

# ---- Clone or update repo ----
cd ~
if [ -d seamus ]; then
    echo "Updating existing repo..."
    cd seamus
    git fetch origin
    git reset --hard origin/main
else
    echo "Cloning repo..."
    git clone https://github.com/hershyz/seamus.git
    cd seamus
fi

# ---- Save machine ID ----
echo "$MACHINE_ID" > .machine_id

# ---- Build crawler ----
echo "Building crawler..."
bazel build -c opt //crawler

# ---- Create and start systemd service ----
BINARY_PATH=$(readlink -f bazel-bin/crawler/crawler)
WORK_DIR=$(pwd)

echo "Creating crawler service..."
sudo tee /etc/systemd/system/crawler.service > /dev/null <<EOF
[Unit]
Description=Search Engine Crawler
After=network.target

[Service]
ExecStart=$BINARY_PATH
WorkingDirectory=$WORK_DIR
Restart=no
RestartSec=5
User=$USER
Environment=MACHINE_ID=$MACHINE_ID
LimitNOFILE=1048575
LimitSTACK=262144

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl start crawler

echo ""
echo "=== Done! Crawler running on machine $MACHINE_ID ==="
echo "  Logs:   sudo journalctl -u crawler -f"
echo "  Status: sudo systemctl status crawler"
