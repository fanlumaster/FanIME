#!/usr/bin/env bash
# Verify against committed digests, never a mutable upstream checksum file.
set -euo pipefail
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
python "$script_dir/../product_lock.py" fetch-dictionaries --staging-root "$STAGING_ROOT"
