#!/usr/bin/env python3
"""Reject a vendored validator that differs from the reviewed Engine gitlink."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
source = root / 'vendor/MetasequoiaImeEngine/contracts/dictionary/product.py'
# An engine submodule that was never checked out is not a validator that drifted. Reporting both as
# "differs" sends whoever reads it looking for a content change that is not there.
if not source.is_file():
    raise SystemExit(f'{source.relative_to(root)} is missing; run git submodule update --init')
if source.read_bytes() != (root / 'scripts/dictionary_product.py').read_bytes():
    raise SystemExit('Dictionary validator differs from the pinned Engine contract')
print('Dictionary product validator matches the pinned Engine contract')
