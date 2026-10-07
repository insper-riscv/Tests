#!/usr/bin/env bash
# Clone Core, Memory and TopLevel next to this checkout (the platform configs name them as ../Core,
# ../Memory and ../TopLevel). Each comes from `main`, unless .github/sibling-branches names another
# branch for it: a change that spans repositories lists the branches of its companion pull requests
# there, and the lines go away when those are merged.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
pins="$here/.github/sibling-branches"

for repo in Core Memory TopLevel; do
  branch=main
  if [ -f "$pins" ]; then
    pinned="$(sed -n "s/^$repo=//p" "$pins" | head -n 1)"
    [ -n "$pinned" ] && branch="$pinned"
  fi
  rm -rf "$here/../$repo"
  echo "$repo: $branch"
  git clone --depth 1 --branch "$branch" "https://github.com/insper-riscv/$repo.git" "$here/../$repo"
done
