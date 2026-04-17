#!/bin/bash
set -e

if [ ! -f .machine_id ]; then
    echo "Error: .machine_id not found. Run ./setup_vm.sh <machine_id> first."
    exit 1
fi

MACHINE_ID=$(cat .machine_id)
USER=$(whoami)

echo "Building htmlserver..."
bazel build -c opt //htmlserver

BINARY_PATH=$(readlink -f bazel-bin/htmlserver/htmlserver)
WORK_DIR=$(pwd)

echo "Binary: $BINARY_PATH"
echo "WorkDir: $WORK_DIR"
echo "User: $USER"
echo "MachineID: $MACHINE_ID"

echo "Creating systemd service..."
sudo tee /etc/systemd/system/htmlserver.service > /dev/null <<EOF
[Unit]
Description=Seamus Search Engine HTML Server
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

echo "Starting service..."
sudo systemctl daemon-reload
sudo systemctl disable htmlserver
sudo systemctl start htmlserver

echo "Checking status..."
sudo systemctl status htmlserver
