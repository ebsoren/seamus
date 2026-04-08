#!/bin/bash
set -e

sudo mkdir -p /var/seamus/parser_output /var/seamus/index_output /var/seamus/urlstore_output
sudo chmod 777 /var/seamus/parser_output /var/seamus/index_output /var/seamus/urlstore_output

sudo apt update
sudo apt install iproute2 netcat-openbsd -y
sudo apt install -y build-essential libssl-dev git npm
sudo npm install -g @bazel/bazelisk

cd ~
git clone https://github.com/hershyz/seamus.git
cd seamus

bazelisk build //crawler:crawler
bazelisk build //index:index

echo "Done. Binaries at ~/seamus/bazel-bin/crawler/crawler and ~/seamus/bazel-bin/index/index"
