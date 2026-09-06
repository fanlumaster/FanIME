#!/usr/bin/env python3
"""Reject a product-lock helper that differs from the reviewed Engine contract."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
engine = root / 'vendor/MetasequoiaImeEngine/contracts/product_lock.py'
vendored = root / 'scripts/product_lock_shared.py'
for path in (engine, vendored):
    if not path.is_file():
        raise SystemExit(f'{path.relative_to(root)} is missing; initialize the Engine submodule')
if engine.read_bytes() != vendored.read_bytes():
    raise SystemExit('Product-lock shared helper differs from the pinned Engine contract')
print('Product-lock shared helper matches the pinned Engine contract')
