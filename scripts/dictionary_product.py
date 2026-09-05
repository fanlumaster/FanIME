"""Public build-time dictionary product validation (Python standard library only).

Consumers may vendor this file from their pinned Engine contract. CI must compare
vendored bytes with that gitlink. Lock-file digests remain the trust anchor; this
validator additionally checks format/profile compatibility and selected payloads.
"""
import hashlib
import json
from pathlib import Path
import re

MANIFEST_NAME = 'dictionary-manifest.json'
SUPPORTED_FORMATS = (1,)
DESKTOP_FILES = frozenset({'msime.db', 'english.db', 'others.db', 'dict_japanese.dat', 'mozc_dictionary_oss_README.txt'})
MOBILE_FILES = frozenset({'msime.db', 'msime.db.sha256'})


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b''):
            digest.update(chunk)
    return digest.hexdigest()


def verify_product(directory, profile='desktop', required_files=None):
    directory = Path(directory)
    manifest = json.loads((directory / MANIFEST_NAME).read_text(encoding='utf-8'))
    version = manifest.get('format_version')
    if (manifest.get('manifest_version') != 1 or type(version) is not int or
            version not in SUPPORTED_FORMATS or
            manifest.get('engine_compatibility', {}).get('dictionary_format') != version):
        raise ValueError('Unsupported or inconsistent dictionary product format')
    if profile not in ('desktop', 'mobile') or manifest.get('profile') != profile:
        raise ValueError('Dictionary product profile mismatch')
    expected = DESKTOP_FILES if profile == 'desktop' else MOBILE_FILES
    files = manifest.get('files', {})
    if set(files) != expected:
        raise ValueError('Dictionary artifact set does not match its profile')
    selected = expected if required_files is None else frozenset(required_files)
    if not selected <= expected:
        raise ValueError('Requested files are not part of this dictionary product')
    for name, entry in files.items():
        if (Path(name).name != name or '/' in name or '\\' in name or
                not re.fullmatch('[0-9a-f]{64}', entry.get('sha256', '')) or
                type(entry.get('size')) is not int or entry['size'] < 0):
            raise ValueError(f'Invalid dictionary artifact metadata: {name}')
        if name in selected:
            path = directory / name
            if not path.is_file() or path.stat().st_size != entry['size'] or sha256(path) != entry['sha256']:
                raise ValueError(f'Dictionary artifact changed: {name}')
    if profile == 'desktop':
        if manifest.get('engine_compatibility', {}).get('japanese_model_magic') != 'MSJPDT1':
            raise ValueError('Unsupported Japanese model format')
        if 'dict_japanese.dat' in selected:
            with (directory / 'dict_japanese.dat').open('rb') as model:
                if model.read(8) != b'MSJPDT1\0':
                    raise ValueError('Japanese model header does not match its declared format')
    return manifest
