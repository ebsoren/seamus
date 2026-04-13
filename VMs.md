### Initialize VM (run once on first boot)

```bash
curl -sL https://raw.githubusercontent.com/hershyz/seamus/main/scripts/vm_init.sh | bash
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
sudo systemctl status crawler (check process state)
sudo journalctl -u crawler -f (live tail logs)
sudo systemctl stop crawler  (stop crawler)
sudo systemctl disable crawler (disable crawler systemctl service -- IMPORTANT TO RUN THIS BEFORE SHUTTING DOWN VMs!)
```
