#!/usr/bin/env bash
# Build for the Pi, copy the daemon and its unit file over, restart the
# service and show the first seconds of its log.
#
# scripts/deploy.sh [ssh-host] default host: charger
#
# Run from the aarch64 VM, whose native build is the Pi build. The remote user
# needs passwordless sudo (the default on Raspberry Pi OS).
set -euo pipefail

host="${1:-charger}"
cd "$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"

if [ "$(uname -m)" != "aarch64" ]; then
    echo "deploy.sh: this machine is $(uname -m); the Pi needs an aarch64 binary" >&2
    exit 1
fi

echo "== build"
cmake --preset native > /dev/null
cmake --build --preset native

echo "== copy to $host"
ssh "$host" 'mkdir -p /tmp/charger-deploy'
rsync --archive --compress \
    build-native/app/charger-daemon systemd/charger-daemon.service \
    "$host:/tmp/charger-deploy/"

echo "== install and restart"
# The quoted 'REMOTE' means nothing below is expanded here; it all runs on the Pi.
ssh "$host" 'sudo bash -s' <<'REMOTE'
set -euo pipefail
# A system account with no home directory and no login shell.
id charger > /dev/null 2>&1 || useradd --system --no-create-home --shell /usr/sbin/nologin charger
install -D --mode=0755 /tmp/charger-deploy/charger-daemon /opt/charger/bin/charger-daemon
install -D --mode=0644 /tmp/charger-deploy/charger-daemon.service \
    /etc/systemd/system/charger-daemon.service
systemctl daemon-reload # make systemd re-read unit files
systemctl enable charger-daemon # start at every boot
systemctl restart charger-daemon # returns once the daemon sent READY=1
systemctl --no-pager --lines=0 status charger-daemon
REMOTE

echo "== log (5 s)"
ssh "$host" 'timeout 5 journalctl --unit=charger-daemon --follow --lines=15' || true