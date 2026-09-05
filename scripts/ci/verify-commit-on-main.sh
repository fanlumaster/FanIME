#!/usr/bin/env bash
# Refuse to build anything that is not in main's history. Run from inside the checkout being
# validated.
set -euo pipefail

git fetch --no-tags --depth 50 origin main
if ! git merge-base --is-ancestor HEAD FETCH_HEAD; then
    echo "The release commit is not in main's history." >&2
    exit 1
fi
echo "HEAD is in main's history."
