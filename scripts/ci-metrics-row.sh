#!/usr/bin/env bash
# Print a docs/ci-metrics.md row for the latest completed 'ci' run on main
# (or the run id given as $1). Job times are GitHub wall-clock; Pipeline is
# the slowest of the five pull-request jobs (the badge job is excluded: it
# runs only on main and nobody waits for it).
#
# Coverage is not in the Actions API. It is read from the local
# build-coverage/coverage/summary.txt if scripts/ci.sh test has been run on
# the same commit; otherwise the two columns are left as <lines> <branches>
# to fill in from the run page (gh run view <id> --web, "Coverage" summary).
set -euo pipefail
cd "$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"

id=${1:-$(gh run list --branch main --workflow ci --status completed \
          --limit 1 --json databaseId --jq '.[0].databaseId')}

# Exported so the jq program below can read them as env.lines / env.branches.
export lines='<lines>' branches='<branches>'
summary=build-coverage/coverage/summary.txt
if [ -r "$summary" ]; then
    lines=$(sed -n 's/^ *lines\.*: *\([0-9.]*%\).*/\1/p' "$summary")
    branches=$(sed -n 's/^ *branches\.*: *\([0-9.]*%\).*/\1/p' "$summary")
fi

# shellcheck disable=SC2016  # jq syntax, not shell expansion
gh run view "$id" --json jobs --jq '
  def secs: (.completedAt | fromdate) - (.startedAt | fromdate);
  def mmss: "\(. / 60 | floor)m\(. % 60 | tostring | if length < 2 then "0" + . else . end)s";
  def col: if . == null then "-" else mmss end;
  (.jobs | map({(.name): secs}) | add) as $j
  | ([$j.build, $j.lint, $j.test, $j.sil, $j.trace] | map(select(. != null)) | max) as $slowest
  | "| \(now | strftime("%Y-%m-%d")) | <change> | \($slowest | mmss) "
    + "| \($j.build | col) | \($j.lint | col) | \($j.test | col) "
    + "| \($j.sil | col) | \($j.trace | col) | \(env.lines) | \(env.branches) |"'