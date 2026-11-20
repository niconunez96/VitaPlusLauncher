#!/usr/bin/env sh
# Activate the repository-managed hooks in the current local checkout.
set -eu
repo_root=$(git rev-parse --show-toplevel)
git config --local core.hooksPath "$repo_root/githooks"
printf 'Installed repository hooks from %s/githooks\n' "$repo_root"
