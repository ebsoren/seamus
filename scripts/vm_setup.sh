#!/bin/bash
set -e

sudo apt update
sudo apt install -y build-essential libssl-dev git npm
sudo npm install -g @bazel/bazelisk

cd ~
git clone https://github.com/hershyz/seamus.git
cd seamus

bazelisk build //crawler:crawler
bazelisk build //index:index

echo "Done. Binaries at ~/seamus/bazel-bin/crawler/crawler and ~/seamus/bazel-bin/index/index"
