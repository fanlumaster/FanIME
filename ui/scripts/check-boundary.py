#!/usr/bin/env python3
"""Keep the reusable GUI library independent of the IME product host."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
forbidden = re.compile(r'MetasequoiaImeEngine|MSIME-Server|config/ime_config\.h|global/globals\.h|ipc/event_listener\.h|GlobalIme::|\bg_inputSession\b')
failures = []
for directory in ('include', 'src'):
    for path in (ROOT / directory).rglob('*'):
        if path.suffix not in ('.h', '.hpp', '.cpp', '.cc'):
            continue
        for number, line in enumerate(path.read_text(encoding='utf-8-sig').splitlines(), 1):
            if forbidden.search(line):
                failures.append(f'{path.relative_to(ROOT)}:{number}: product-specific dependency in GUI library')
if failures:
    raise SystemExit('\n'.join(failures))
print('GUI library has no Server/Engine product dependencies')
