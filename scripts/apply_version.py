#!/usr/bin/env python3
"""Stamp the version from version.txt into the TSF DLL version resource.

release-please owns version.txt. The VERSIONINFO block in windows/src/IME/MetasequoiaIME.rc carries the
same number in two shapes, a comma-separated quad and a dotted string, so it cannot be driven by
release-please's generic updater. The release workflow runs this before configuring CMake.

The fourth field of the quad stays 0; only MAJOR.MINOR.PATCH are release-managed.

    python scripts/apply_version.py            # stamp from version.txt
    python scripts/apply_version.py --check    # exit 1 if the resource is out of date
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
VERSION_FILE = REPO_ROOT / "version.txt"
RESOURCE_FILE = REPO_ROOT / "windows" / "src" / "IME" / "MetasequoiaIME.rc"

SEMVER = re.compile(r"^(\d+)\.(\d+)\.(\d+)$")

# Each pattern captures the part before the version and the part after it, so only the digits move.
SUBSTITUTIONS = (
    (re.compile(r"^(\s*FILEVERSION\s+)\d+,\d+,\d+,\d+(\s*)$", re.MULTILINE), "quad"),
    (re.compile(r"^(\s*PRODUCTVERSION\s+)\d+,\d+,\d+,\d+(\s*)$", re.MULTILINE), "quad"),
    (re.compile(r'^(\s*VALUE\s+"FileVersion",\s*")\d+\.\d+\.\d+\.\d+("\s*)$', re.MULTILINE), "dotted"),
    (re.compile(r'^(\s*VALUE\s+"ProductVersion",\s*")\d+\.\d+\.\d+\.\d+("\s*)$', re.MULTILINE), "dotted"),
)


def read_version() -> tuple[str, str, str]:
    if not VERSION_FILE.is_file():
        raise SystemExit(f"error: {VERSION_FILE} is missing")
    raw = VERSION_FILE.read_text(encoding="utf-8").strip()
    match = SEMVER.match(raw)
    if not match:
        raise SystemExit(f"error: version.txt must hold MAJOR.MINOR.PATCH, got {raw!r}")
    return match.groups()


def render(version: tuple[str, str, str]) -> tuple[str, str]:
    major, minor, patch = version
    return f"{major},{minor},{patch},0", f"{major}.{minor}.{patch}.0"


def apply(text: str, quad: str, dotted: str) -> tuple[str, int]:
    replaced = 0
    for pattern, shape in SUBSTITUTIONS:
        value = quad if shape == "quad" else dotted
        text, count = pattern.subn(lambda m: f"{m.group(1)}{value}{m.group(2)}", text)
        if count != 1:
            raise SystemExit(
                f"error: expected exactly one match for {pattern.pattern!r} in {RESOURCE_FILE}, found {count}"
            )
        replaced += count
    return text, replaced


def read_resource() -> str:
    # newline="" keeps the CRLF endings byte for byte, so only the version digits show up in a diff.
    with RESOURCE_FILE.open("r", encoding="utf-8", newline="") as stream:
        return stream.read()


def write_resource(text: str) -> None:
    with RESOURCE_FILE.open("w", encoding="utf-8", newline="") as stream:
        stream.write(text)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="report drift instead of rewriting the resource")
    args = parser.parse_args()

    version = read_version()
    quad, dotted = render(version)

    original = read_resource()
    updated, replaced = apply(original, quad, dotted)

    if args.check:
        if updated != original:
            print(f"{RESOURCE_FILE.relative_to(REPO_ROOT)} does not match version.txt ({dotted})")
            return 1
        print(f"{RESOURCE_FILE.relative_to(REPO_ROOT)} matches version.txt ({dotted})")
        return 0

    if updated == original:
        print(f"Already at {dotted}, nothing to do.")
        return 0

    write_resource(updated)
    print(f"Stamped {dotted} into {RESOURCE_FILE.relative_to(REPO_ROOT)} ({replaced} fields).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
