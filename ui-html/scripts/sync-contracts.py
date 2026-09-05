#!/usr/bin/env python3
"""Materialize the pinned Engine's web contract for bundling and classic WebView pages."""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FILES = ('schema.js', 'runtime.js', 'schema.d.ts', 'runtime.d.ts', 'messages.ts')
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--check', action='store_true')
args = parser.parse_args()
source = ROOT / 'vendor/MetasequoiaImeEngine/contracts/webview'
destination = ROOT / 'webview2/shared'
for name in FILES:
    expected = (source / name).read_bytes()
    target = destination / name
    if args.check:
        if not target.exists() or target.read_bytes() != expected:
            raise SystemExit(f'{target} differs from the pinned Engine contract; run scripts/sync-contracts.py')
    else:
        destination.mkdir(parents=True, exist_ok=True)
        target.write_bytes(expected)
