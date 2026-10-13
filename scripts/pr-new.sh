#!/usr/bin/env bash
# pr-new.sh - open a GitHub pull request whose description answers four
# questions. Each question becomes a heading in the description.
#
# Usage: scripts/pr-new.sh [base-branch]      (base defaults to main)
#
# Run it on your work branch after committing. It asks for a title and the
# four answers, shows the finished description, and only after you confirm
# does it push the branch and create the pull request. It does not merge.
#
# Needs: git, and gh already logged in (gh auth status).

set -euo pipefail

base=${1:-main}

# The four questions. The same text is used as the prompt and as the heading.
questions=(
    "What: what changed, in one or two lines?"
    "Why: what is the reason, or the requirement it serves?"
    "How verified: exactly what did you run, and what did you see?"
    "Notes for the reviewer: any risks, follow-ups, or anything deliberately left out?"
)

die() { echo "pr-new: $*" >&2; exit 1; }

command -v gh > /dev/null || die "gh is not installed."
gh auth status > /dev/null 2>&1 || die "gh is not logged in; run 'gh auth login'."

branch=$(git rev-parse --abbrev-ref HEAD)
[ "$branch" != "HEAD" ] || die "detached HEAD; switch to a branch first."
[ "$branch" != "$base" ] || die "you are on $base; create a work branch first."

if [ -n "$(git status --porcelain)" ]; then
    echo "Warning: uncommitted changes will NOT be part of this pull request." >&2
fi

# Read a multi-line answer from the terminal; an empty line ends it.
# Prompts go to stderr so that only the answer is captured.
ask() {
    local line answer=""
    printf '\n%s\n  (one or more lines; finish with an empty line)\n' "$1" >&2
    while IFS= read -r -e -p '> ' line && [ -n "$line" ]; do
        answer+="$line"$'\n'
    done
    printf '%s' "$answer"
}

# Title: offer the last commit's subject line, editable in place.
default_title=$(git log -1 --format=%s)
printf 'Branch: %s  ->  %s\n\n' "$branch" "$base"
IFS= read -r -e -i "$default_title" -p 'Title: ' title || true
title=${title:-$default_title}      # an empty answer keeps the commit subject
[ -n "$title" ] || die "a title is required."

body=$(mktemp)
trap 'rm -f "$body"' EXIT

answered=0
for q in "${questions[@]}"; do
    answer=$(ask "$q")
    # A question left empty is omitted from the description.
    if [ -n "$answer" ]; then
        printf '### %s\n\n%s\n\n' "$q" "$answer" >> "$body"
        answered=$((answered + 1))
    fi
done
[ "$answered" -gt 0 ] || die "no answers given; nothing to create."

printf '\n----- pull request preview -----\n'
printf 'Title: %s\n\n' "$title"
cat "$body"
printf '%s\n' '--------------------------------'

reply=""
read -r -p "Push '$branch' and create this pull request? [y/N] " reply || true
case "$reply" in
    y|Y|yes|YES) ;;
    *) echo "Cancelled; nothing was pushed or created."; exit 0 ;;
esac

git push -u origin HEAD
gh pr create --base "$base" --head "$branch" --title "$title" --body-file "$body"

echo
echo "Created. To merge: gh pr merge --squash --delete-branch"
