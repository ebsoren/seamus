#!/bin/bash
set -e

if [ ! -d /var/seamus/parser_output ] || [ -z "$(ls -A /var/seamus/parser_output 2>/dev/null)" ]; then
    echo "Error: /var/seamus/parser_output is empty. Run the crawler first." >&2
    exit 1
fi

USER=$(whoami)

echo "Building index..."
bazel build -c opt //index

BINARY_PATH=$(readlink -f bazel-bin/index/index)
WORK_DIR=$(pwd)

echo "Binary: $BINARY_PATH"
echo "WorkDir: $WORK_DIR"
echo "User: $USER"

echo "Creating systemd service..."
sudo tee /etc/systemd/system/index.service > /dev/null <<EOF
[Unit]
Description=Seamus Index Builder
After=network.target

[Service]
Type=oneshot
ExecStart=$BINARY_PATH
WorkingDirectory=$WORK_DIR
User=$USER
LimitNOFILE=1048575
LimitSTACK=262144
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

echo "Starting service..."
sudo systemctl daemon-reload
sudo systemctl start index.service

echo
echo "Index is running. Tail logs with:"
echo "  sudo journalctl -u index.service -f"
echo "Check status with:"
echo "  sudo systemctl status index.service"
