"""REQ-019: the start-up log says whether this binary is a verified release.

The installer (scripts/install-release.sh) writes a VERIFIED marker next to
the binary only after the signature and checksums passed. The daemon reports
the marker's verdict in its first log lines, so 'journalctl -u charger-daemon'
answers "is this a signed release?" without running anything else.
"""

import os
import subprocess

import pytest

def built_version(daemon_binary: str) -> str:
    """'charger-daemon <version>' from the binary itself (REQ-018)."""
    line = subprocess.run([daemon_binary, "--version"], capture_output=True,
                          text=True, check=True).stdout.strip()
    return line.split(" ", 1)[1]

@pytest.mark.req("REQ-019")
def test_marker_naming_this_version_logs_verified_release(start_daemon, tmp_path):
    binary = os.environ.get("CHARGER_DAEMON", "build-native/app/charger-daemon")
    marker = tmp_path / "VERIFIED"
    marker.write_text(f"version={built_version(binary)}\nverified=2026-11-04T00:00:00Z\n")
    daemon = start_daemon("--release-marker", str(marker))
    assert any(line.startswith("charger-daemon: verified release ") for line in daemon.log_lines())
    assert not any("unverified" in line for line in daemon.log_lines())

@pytest.mark.req("REQ-019")
def test_marker_naming_another_version_logs_unverified(start_daemon, tmp_path):
    marker = tmp_path / "VERIFIED"
    marker.write_text("version=v9.9.9\n")
    daemon = start_daemon("--release-marker", str(marker))
    assert any(line.startswith("charger-daemon: unverified build: marker names v9.9.9")
               for line in daemon.log_lines())

@pytest.mark.req("REQ-019")
def test_missing_marker_logs_unverified(start_daemon, tmp_path):
    daemon = start_daemon("--release-marker", str(tmp_path / "missing"))
    assert any(line.startswith("charger-daemon: unverified build: no release marker")
               for line in daemon.log_lines())
