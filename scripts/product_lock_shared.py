"""Shared primitives for platform product locks.

Platform locks keep their repository-specific constants and command-line interface, but vendor this
module for the security-sensitive digest, manifest, download and tag-resolution operations. A
consumer must compare its vendored copy byte-for-byte with this file in CI.
"""

from __future__ import annotations

import hashlib
import os
import re
import subprocess
import tempfile
import urllib.error
import urllib.request
from pathlib import Path, PurePosixPath
from typing import Callable, Mapping

SHA = re.compile(r"[0-9a-f]{40}\Z")
TAG = re.compile(r"dict-[A-Za-z0-9._-]+\Z")


def _asset_path(directory: Path, name: str) -> Path:
    path = PurePosixPath(name)
    if path.is_absolute() or len(path.parts) != 1 or path.parts[0] in ("", ".", "..") or "\\" in name:
        raise ValueError(f"Invalid product asset name: {name}")
    return directory / path.parts[0]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify_digests(directory: Path, expected: Mapping[str, str]) -> None:
    """Verify every locked asset without trusting a checksum file shipped beside it."""
    directory = Path(directory)
    for name, digest in expected.items():
        path = _asset_path(directory, name)
        if not path.is_file():
            raise ValueError(f"Locked product asset is missing: {name}")
        actual = sha256(path)
        if actual != digest:
            raise ValueError(f"{name} does not match the product lock: expected {digest}, got {actual}")


def verify_manifest_provenance(
    directory: Path,
    manifest_name: str,
    verify_product: Callable[..., dict],
    required_files: set[str] | tuple[str, ...],
    repository: str,
    commit: str,
) -> dict:
    """Validate a dictionary manifest and bind its source to the product lock."""
    manifest = verify_product(directory, "desktop", required_files)
    source = manifest.get("source", {})
    if (source.get("repository") != repository or source.get("commit") != commit or
            source.get("dirty") is not False):
        raise ValueError(f"{manifest_name} provenance does not match the product lock")
    return manifest


def published_checksums(path: Path) -> dict[str, str]:
    """Read the conventional two-space-separated SHA256SUMS.txt format."""
    checksums = {}
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        digest, separator, name = line.partition("  ")
        if separator and name.strip():
            name = name.strip()
            if name in checksums:
                raise ValueError(f"Duplicate published checksum entry: {name}")
            checksums[name] = digest.strip()
    return checksums


def download_with_retries(url: str, target: Path, *, attempts: int = 3, timeout: int = 120) -> None:
    """Download one asset atomically, retrying transient URL and filesystem failures."""
    if attempts < 1 or timeout <= 0:
        raise ValueError("Download attempts and timeout must be positive")
    target = Path(target)
    target.parent.mkdir(parents=True, exist_ok=True)
    last_error: Exception | None = None
    for _ in range(attempts):
        temporary_name: str | None = None
        try:
            with urllib.request.urlopen(url, timeout=timeout) as response:
                with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as temporary:
                    temporary_name = temporary.name
                    for chunk in iter(lambda: response.read(1 << 20), b""):
                        temporary.write(chunk)
            os.replace(temporary_name, target)
            return
        except (urllib.error.URLError, TimeoutError, OSError) as error:
            last_error = error
            if temporary_name:
                try:
                    Path(temporary_name).unlink()
                except OSError:
                    pass
    raise ValueError(f"Could not download {url}: {last_error}") from last_error


def resolve_tag_commit(repository_url: str, tag: str) -> str:
    """Resolve an explicit tag to a commit using git's unauthenticated read-only protocol."""
    if not TAG.fullmatch(tag):
        raise ValueError("Tag must be an explicit dict-* release")
    output = subprocess.check_output(
        ["git", "ls-remote", repository_url, f"refs/tags/{tag}", f"refs/tags/{tag}^{{}}"], text=True
    )
    references = {}
    for line in output.splitlines():
        commit, _, reference = line.partition("\t")
        references[reference.strip()] = commit.strip()
    commit = references.get(f"refs/tags/{tag}^{{}}") or references.get(f"refs/tags/{tag}")
    if not commit or not SHA.fullmatch(commit):
        raise ValueError(f"{tag} does not resolve to a commit in {repository_url}")
    return commit
