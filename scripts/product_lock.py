#!/usr/bin/env python3
"""Maintain and consume the reviewed Windows product dependency lock.

Only `refresh` resolves upstream refs. All build commands are offline except fetching
the exact dictionary assets, whose bytes must match the committed SHA256 values.
"""

from __future__ import annotations

import argparse
import importlib.util
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
_product_spec = importlib.util.spec_from_file_location("dictionary_product", Path(__file__).with_name("dictionary_product.py"))
_product = importlib.util.module_from_spec(_product_spec)
_product_spec.loader.exec_module(_product)
PRODUCT_MANIFEST = _product.MANIFEST_NAME
LEGACY_DICTIONARY_TAG = "dict-2026.09.05"
DICTIONARY_REPOSITORY = "metasequoiaime/MSIME-Engine"

# The tip, the server, the GUI framework, the pages and the installer are components of this
# repository now, so a commit of this repository already pins them and there is nothing left to
# lock. The helpcodes moved into the engine, so the engine gitlink pins those too. What survives is
# the engine itself and the dictionary release, which is fetched at build time.
REPOSITORIES = {
    "engine": "metasequoiaime/MSIME-Engine",
}
ROOT_COMPONENTS = ()
ENGINE_GITLINK = "vendor/MetasequoiaImeEngine"
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
    # The dictionary source and build entry point moved into the engine. The published MSIME-Dict
    # releases stay valid as immutable historical artefacts, so the old repository is still accepted
    # for the tag that shipped from it, and for nothing else: moving the publishing source is a
    # reviewed change to this file, not something a tag rename can do quietly.
    if dictionary.get("repository") != DICTIONARY_REPOSITORY and not (
        dictionary.get("repository") == "metasequoiaime/MSIME-Dict" and dictionary.get("tag") == LEGACY_DICTIONARY_TAG
    ):
        raise ValueError("Unexpected dictionary repository")
    if not TAG.fullmatch(dictionary.get("tag", "")):
        raise ValueError("Dictionary tag must be an explicit dict-* release, never latest")
    if not SHA.fullmatch(dictionary.get("source_commit", "")):
        raise ValueError("Dictionary source_commit must be a full immutable commit SHA")
    assets = dictionary.get("assets", {})
    expected_assets = ASSETS if dictionary['tag'] == LEGACY_DICTIONARY_TAG else ASSETS | {PRODUCT_MANIFEST}
    if set(assets) != expected_assets:
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

    if PRODUCT_MANIFEST in data["dictionary"]["assets"]:
        manifest = _product.verify_product(directory, "desktop", set(data["dictionary"]["assets"]) - {"SHA256SUMS.txt", PRODUCT_MANIFEST})
        # Matching digests only prove the bytes are the reviewed bytes. The manifest additionally
        # states which commit built them and whether that tree was clean, and a release whose data
        # came from an uncommitted working tree cannot be rebuilt from anything.
        source = manifest.get("source", {})
        if (source.get("repository") != data["dictionary"]["repository"] or
                source.get("commit") != data["dictionary"]["source_commit"] or source.get("dirty") is not False):
            raise ValueError("Dictionary manifest provenance does not match the reviewed source commit")


def git(directory: Path, *args: str) -> str:
    return subprocess.check_output(["git", "-C", str(directory), *args], text=True).strip()


def verify_checkout(component: str, directory: Path, data: dict) -> None:
    expected = data["repositories"][component]["commit"]
    if git(directory, "rev-parse", "HEAD") != expected:
        raise ValueError(f"{component}: checkout does not match product lock")


def engine_gitlink(directory: Path) -> str:
    fields = git(directory, "ls-tree", "HEAD", ENGINE_GITLINK).split()
    if len(fields) != 4 or fields[0] != "160000":
        raise ValueError(f"{ENGINE_GITLINK} is not a submodule of this repository")
    return fields[2]


def verify_contracts(directory: Path, data: dict) -> None:
    if engine_gitlink(directory) != data["repositories"]["engine"]["commit"]:
        raise ValueError("The engine submodule and the product lock name different contract commits")


def verify_published(data: dict) -> None:
    """Every locked commit has to be reachable from its own repository's default branch.

    This is a release gate rather than a validate rule, deliberately. A pull request legitimately locks branch commits while one change lands across several repositories at once, and enforcing this in pull request CI would deadlock the very landing it exists to protect. At release time the situation is the opposite: an input that never reached its default branch is an input nobody merged, and shipping it makes the lock attest to a review that did not happen.
    """
    for name, entry in data["repositories"].items():
        repository, commit = entry["repository"], entry["commit"]
        try:
            default = api(f"repos/{repository}")["default_branch"]
            status = api(f"repos/{repository}/compare/{default}...{commit}")["status"]
        except subprocess.CalledProcessError as error:
            raise ValueError(f"{name}: cannot resolve {commit} in {repository}") from error
        # behind and identical both mean the locked commit is an ancestor of the default branch.
        # ahead and diverged mean it sits on something that was never merged into it.
        if status not in ("behind", "identical"):
            raise ValueError(f"{name}: {commit} is not on {repository}'s {default} ({status})")
        print(f"{name}: {commit[:12]} is on {repository}'s {default}")


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
        for name in sorted(data["dictionary"]["assets"]):
            command.extend(["--pattern", name])
        subprocess.run(command, check=True)
        verify_assets(incoming, data)
        target.mkdir(exist_ok=True)
        for name in sorted(data["dictionary"]["assets"]):
            shutil.copyfile(incoming / name, target / name)
    notice = staging / "MetasequoiaImeDict" / "source" / "mozc_dictionary_oss" / "README.txt"
    notice.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(target / "mozc_dictionary_oss_README.txt", notice)


def api(endpoint: str) -> dict:
    return json.loads(subprocess.check_output(["gh", "api", endpoint], text=True))


def refresh(tag: str, refs: list[str]) -> dict:
    if not TAG.fullmatch(tag):
        raise ValueError("refresh requires an explicit dict-* release tag")
    # Nothing is resolved from a floating ref any more. Every first-party source is either a
    # directory of this repository or the engine submodule, so --ref has nothing left to override.
    if refs:
        raise ValueError("--ref has no effect: the only locked source is the engine submodule")
    # Bumping the submodule is the review; the lock only records which commit that review landed on,
    # so that verify-published can still refuse a release built on a commit nobody merged.
    repositories = {"engine": {"repository": REPOSITORIES["engine"], "commit": engine_gitlink(ROOT)}}
    release = api(f"repos/{DICTIONARY_REPOSITORY}/releases/tags/{tag}")
    if release["draft"]:
        raise ValueError("Cannot lock an unpublished dictionary release")
    assets = {}
    for asset in release["assets"]:
        if asset["name"] in ASSETS | {PRODUCT_MANIFEST}:
            digest = asset.get("digest") or ""
            if not digest.startswith("sha256:"):
                raise ValueError(f"Release asset has no SHA256 digest: {asset['name']}")
            assets[asset["name"]] = digest.removeprefix("sha256:")
    source_commit = api(f"repos/{DICTIONARY_REPOSITORY}/commits/{tag}")["sha"]
    return validate({"schema_version": 1, "repositories": repositories,
                     "dictionary": {"repository": DICTIONARY_REPOSITORY, "tag": tag,
                                    "source_commit": source_commit, "assets": assets}})


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
    commands.add_parser("verify-published")
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
    elif args.command == "verify-published":
        verify_published(data)
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
