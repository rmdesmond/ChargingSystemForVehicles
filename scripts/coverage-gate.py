#!/usr/bin/env python3
"""Fail when coverage is below the committed threshold.

    scripts/coverage-gate.py build-coverage/coverage/summary.txt [--badge FILE]

Reads the two percentages from lcov's summary and compares each with the
number in .coverage-threshold at the repository root:

    lines 91
    branches 77

Exit status 1 if either is below its threshold, so the test step fails. The
thresholds only ever go up: raise them when the numbers have grown, never lower
them to make a pull request pass (README, "Coverage rule").

--badge writes a JSON file in the shields.io endpoint format; CI publishes it
and the README badge reads it.
"""
import json
from os import path
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
THRESHOLD_FILE = ROOT / ".coverage-threshold"
# lcov --summary prints e.g. " lines......: 96.8% (421 of 435 lines)"
SUMMARY_LINE = re.compile(r"^\s*(lines|branches)\.+:\s*([0-9.]+)%\s*\((\d+) of (\d+)")

def read_summary(path: Path) -> dict[str, tuple[float, int, int]]:
    """{'lines': (96.8, 421, 435), 'branches': (...)} from lcov's summary."""
    found = {}
    for line in path.read_text().splitlines():
        match = SUMMARY_LINE.match(line)
        if match:
            kind, percent, hit, total = match.groups()
            found[kind] = (float(percent), int(hit), int(total))
    missing = {"lines", "branches"} - found.keys()
    if missing:
        sys.exit(f"{path}: no {', '.join(sorted(missing))} total in the lcov summary")
    return found

def read_thresholds(path: Path) -> dict[str, float]:
    thresholds = {}
    for line in path.read_text().splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            kind, value = line.split()
            thresholds[kind] = float(value)
    missing = {"lines", "branches"} - thresholds.keys()
    if missing:
        sys.exit(f"{path}: no threshold for {', '.join(sorted(missing))}")
    return thresholds

def badge(lines_percent: float) -> dict:
    # Green from 90, yellow from 80, red below: the usual reading of a badge.
    color = "brightgreen" if lines_percent >= 90 else "yellow" if lines_percent >= 80 else "red"
    return {"schemaVersion": 1, "label": "coverage", "message": f"{lines_percent:.1f}%","color": color}

def main(argv: list[str]) -> int:
    if len(argv) not in (2, 4) or (len(argv) == 4 and argv[2] != "--badge"):
        sys.exit(__doc__.strip().splitlines()[2].strip())
    summary = read_summary(Path(argv[1]))
    thresholds = read_thresholds(THRESHOLD_FILE)

    failed = False
    for kind in ("lines", "branches"):
        percent, hit, total = summary[kind]
        threshold = thresholds[kind]
        verdict = "ok" if percent >= threshold else "BELOW THRESHOLD"
        if percent < threshold:
            failed = True
        print(f"coverage {kind:8s} {percent:5.1f}% ({hit} of {total})"
            f" threshold {threshold:5.1f}% {verdict}")
    
    if len(argv) == 4:
        Path(argv[3]).write_text(json.dumps(badge(summary["lines"][0])) + "\n")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))