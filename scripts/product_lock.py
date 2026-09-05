#!/usr/bin/env python3
"""Maintain and consume the reviewed Windows product dependency lock.

Only `refresh` resolves upstream refs. All build commands are offline except fetching
the exact dictionary assets, whose bytes must match the committed SHA256 values.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
REPOSITORIES = {
    "server": "metasequoiaime/MSIME-Server",
    "ui": "metasequoiaime/MSIME-UiHtml",
    "installer": "metasequoiaime/MSIME-Installer",
    "helpcode": "metasequoiaime/MSIME-HelpCode",
    "engine": "metasequoiaime/MSIME-Engine",
    "gui": "metasequoiaime/MSIME-UI",
}
ROOT_COMPONENTS = ("server", "ui", "installer", "helpcode")
SERVER_GITLINKS = {"engine": "MetasequoiaImeEngine", "gui": "msimeui"}
ASSETS = {
    "msime.db", "english.db", "others.db", "dict_japanese.dat",
    "mozc_dictionary_oss_README.txt", "SHA256SUMS.txt",
}
SHA = re.compile(r"[0-9a-f]{40}\Z")
DIGEST = re.compile(r"[0-9a-f]{64}\Z")
TAG = re.compile(r"dict-[A-Za-z0-9._-]+\Z")


def validate(data: dict) -> dict:
    if data.get("schema_version") != 1:
        raise ValueError("Unsupported product lock schema_version")
    repositories = data.get("repositories", {})
    if set(repositories) != set(REPOSITORIES):
        raise ValueError("Product lock must contain every expected repository")
    for name, repository in REPOSITORIES.items():
        entry = repositories[name]
        if entry.get("repository") != repository or not SHA.fullmatch(entry.get("commit", "")):
            raise ValueError(f"{name}: expected {repository} and a full immutable commit SHA")
    dictionary = data.get("dictionary", {})
    if dictionary.get("repository") != "metasequoiaime/MSIME-Dict":
        raise ValueError("Unexpected dictionary repository")
    if not TAG.fullmatch(dictionary.get("tag", "")):
        raise ValueError("Dictionary tag must be an explicit dict-* release, never latest")
    assets = dictionary.get("assets", {})
    if set(assets) != ASSETS:
        raise ValueError("Dictionary lock must include all databases, model, notice and checksums")
    for name, digest in assets.items():
        if not DIGEST.fullmatch(digest):
            raise ValueError(f"Invalid SHA256 for {name}")
    return data


def load(path: Path) -> dict:
    return validate(json.loads(path.read_text(encoding="utf-8")))


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify_assets(directory: Path, data: dict) -> None:
    for name, expected in data["dictionary"]["assets"].items():
        path = directory / name
        if not path.is_file() or sha256(path) != expected:
            raise ValueError(f"Locked dictionary asset missing or changed: {name}")


def git(directory: Path, *args: str) -> str:
    return subprocess.check_output(["git", "-C", str(directory), *args], text=True).strip()


def verify_checkout(component: str, directory: Path, data: dict) -> None:
    expected = data["repositories"][component]["commit"]
    if git(directory, "rev-parse", "HEAD") != expected:
        raise ValueError(f"{component}: checkout does not match product lock")
    if component == "server":
        for name, path in SERVER_GITLINKS.items():
            fields = git(directory, "ls-tree", "HEAD", path).split()
            if len(fields) != 4 or fields[0] != "160000" or fields[2] != data["repositories"][name]["commit"]:
                raise ValueError(f"Server's {path} gitlink does not match product lock")


def verify_contracts(directory: Path, data: dict) -> None:
    fields = git(directory, "ls-tree", "HEAD", "vendor/MetasequoiaImeEngine").split()
    if len(fields) != 4 or fields[0] != "160000" or fields[2] != data["repositories"]["engine"]["commit"]:
        raise ValueError("TSF and Server must consume the same Engine contract commit")


def github_outputs(data: dict) -> str:
    return "".join(f"{name}_sha={entry['commit']}\n" for name, entry in data["repositories"].items()) + \
        f"dictionary_tag={data['dictionary']['tag']}\n"


def fetch_dictionaries(staging: Path, data: dict) -> None:
    target = staging / "MetasequoiaImeDict" / "out"
    target.parent.mkdir(parents=True, exist_ok=True)
    # Verify the complete set before replacing any usable files in the staging tree.
    with tempfile.TemporaryDirectory(dir=target.parent) as temporary:
        incoming = Path(temporary)
        command = ["gh", "release", "download", data["dictionary"]["tag"],
                   "--repo", data["dictionary"]["repository"], "--dir", str(incoming)]
        for name in sorted(ASSETS):
            command.extend(["--pattern", name])
        subprocess.run(command, check=True)
        verify_assets(incoming, data)
        target.mkdir(exist_ok=True)
        for name in sorted(ASSETS):
            shutil.copyfile(incoming / name, target / name)
    notice = staging / "MetasequoiaImeDict" / "source" / "mozc_dictionary_oss" / "README.txt"
    notice.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(target / "mozc_dictionary_oss_README.txt", notice)


def api(endpoint: str) -> dict:
    return json.loads(subprocess.check_output(["gh", "api", endpoint], text=True))


def refresh(tag: str, refs: list[str]) -> dict:
    if not TAG.fullmatch(tag):
        raise ValueError("refresh requires an explicit dict-* release tag")
    overrides = {}
    for override in refs:
        component, separator, ref = override.partition("=")
        if not separator or component not in ROOT_COMPONENTS or not ref:
            raise ValueError("--ref must be server|ui|installer|helpcode=<commit-or-ref>")
        overrides[component] = ref
    repositories = {}
    for name in ROOT_COMPONENTS:
        repository = REPOSITORIES[name]
        commit = api(f"repos/{repository}/commits/{overrides.get(name, 'main')}")["sha"]
        repositories[name] = {"repository": repository, "commit": commit}
    server = repositories["server"]
    tree = api(f"repos/{server['repository']}/git/trees/{server['commit']}")["tree"]
    for name, path in SERVER_GITLINKS.items():
        entry = next(item for item in tree if item["path"] == path and item["type"] == "commit")
        repositories[name] = {"repository": REPOSITORIES[name], "commit": entry["sha"]}
    release = api(f"repos/metasequoiaime/MSIME-Dict/releases/tags/{tag}")
    if release["draft"]:
        raise ValueError("Cannot lock an unpublished dictionary release")
    assets = {}
    for asset in release["assets"]:
        if asset["name"] in ASSETS:
            digest = asset.get("digest") or ""
            if not digest.startswith("sha256:"):
                raise ValueError(f"Release asset has no SHA256 digest: {asset['name']}")
            assets[asset["name"]] = digest.removeprefix("sha256:")
    return validate({"schema_version": 1, "repositories": repositories,
                     "dictionary": {"repository": "metasequoiaime/MSIME-Dict", "tag": tag, "assets": assets}})


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lock", type=Path, default=ROOT / "product-lock.json")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("validate")
    output = commands.add_parser("outputs")
    output.add_argument("--github-output", type=Path)
    checkout = commands.add_parser("verify-checkout")
    checkout.add_argument("component", choices=REPOSITORIES)
    checkout.add_argument("directory", type=Path)
    contracts = commands.add_parser("verify-contracts")
    contracts.add_argument("directory", type=Path)
    fetch = commands.add_parser("fetch-dictionaries")
    fetch.add_argument("--staging-root", type=Path, required=True)
    verify = commands.add_parser("verify-dictionaries")
    verify.add_argument("directory", type=Path)
    manifest = commands.add_parser("manifest")
    manifest.add_argument("--windows-commit", required=True)
    manifest.add_argument("--output", type=Path, required=True)
    update = commands.add_parser("refresh")
    update.add_argument("--dictionary-tag", required=True)
    update.add_argument("--ref", action="append", default=[])
    args = parser.parse_args()
    if args.command == "refresh":
        write_json(args.lock, refresh(args.dictionary_tag, args.ref))
        return
    data = load(args.lock)
    if args.command == "outputs":
        if args.github_output:
            with args.github_output.open("a", encoding="utf-8") as stream:
                stream.write(github_outputs(data))
        else:
            print(github_outputs(data), end="")
    elif args.command == "verify-checkout":
        verify_checkout(args.component, args.directory, data)
    elif args.command == "verify-contracts":
        verify_contracts(args.directory, data)
    elif args.command == "fetch-dictionaries":
        fetch_dictionaries(args.staging_root, data)
    elif args.command == "verify-dictionaries":
        verify_assets(args.directory, data)
    elif args.command == "manifest":
        if not SHA.fullmatch(args.windows_commit):
            raise ValueError("Windows commit must be a full SHA")
        data["repositories"]["windows"] = {"repository": "metasequoiaime/MSIME-Windows",
                                           "commit": args.windows_commit}
        data["lock_sha256"] = sha256(args.lock)
        write_json(args.output, data)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error)) from error
