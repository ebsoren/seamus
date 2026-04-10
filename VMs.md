### Initialize VM (run once on first boot)

```bash
curl -sL https://raw.githubusercontent.com/hershyz/seamus/main/scripts/vm_setup.sh | bash
```
Installs required packages, and clones monorepo, and creates output dirs.

### Configure System Limits (run prior to launching crawler)
```bash
./scripts/setup_vm.sh
```
Configures system limits for stack sizes and file descriptors, sets machine ID.

### Run Crawler
```bash
./scripts/run_crawler.sh
```
Spawns the crawler with correct resource limits for the VM, and detaches the process from the shell. \
\
To manage it later from any session:
```bash
  - sudo systemctl status crawler — check state
  - sudo journalctl -u crawler -f — tail logs
  - sudo systemctl stop crawler — stop it
```
