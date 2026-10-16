#!/usr/bin/env bash
# Print a docs/ci-metrics.md row for the latest completed 'ci' run on main
# (or the run id given as $1). Job times are GitHub wall-clock; Pipeline is
# the slowest job. Coverage is not in the API: read it from the run page
# (gh run view <id> --web) or build-coverage/coverage/summary.txt.
set -euo pipefail
id=${1:-$(gh run list --branch main --workflow ci --status completed \
          --limit 1 --json databaseId --jq '.[0].databaseId')}
gh run view "$id" --json jobs --jq '
  def secs: (.completedAt | fromdate) - (.startedAt | fromdate);
  def mmss: "\(. / 60 | floor)m\(. % 60 | tostring | if length < 2 then "0" + . else . end)s";
  (.jobs | map({(.name): secs}) | add) as $j
  | "| \(now | strftime("%Y-%m-%d")) | <change> | \($j | [.[]] | max | mmss) "
    + "| \($j.build | mmss) | \($j.lint | mmss) | \($j.test | mmss) | <lines> | <branches> |"'