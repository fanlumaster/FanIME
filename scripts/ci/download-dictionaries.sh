#!/usr/bin/env bash
# Fetch the shipping dictionaries from a metasequoiaime/MSIME-Dict release and stage them where
# msime-installer's Prepare-PackageFiles.ps1 reads them from. They are not rebuilt here: MSIME-Dict
# builds and publishes them, and this verifies the checksums it published alongside.
#
# Requires GH_TOKEN, DICTIONARY_TAG (a dict-* tag or "latest"), STAGING_ROOT.
# Appends the resolved tag to GITHUB_ENV.
set -euo pipefail

target="$STAGING_ROOT/MetasequoiaImeDict/out"
mkdir -p "$target"

if [[ "$DICTIONARY_TAG" == "latest" ]]; then
    DICTIONARY_TAG=$(gh release list --repo metasequoiaime/MSIME-Dict --limit 30 --json tagName \
        --jq '[.[] | select(.tagName | startswith("dict-"))] | first | .tagName // ""')
fi

if [[ -z "$DICTIONARY_TAG" ]]; then
    echo "No dict-* release found in metasequoiaime/MSIME-Dict. Run its Build dictionaries workflow with publish enabled first." >&2
    exit 1
fi

echo "Using dictionary release $DICTIONARY_TAG"
gh release download "$DICTIONARY_TAG" --repo metasequoiaime/MSIME-Dict --dir "$target" --clobber
(cd "$target" && sha256sum --check SHA256SUMS.txt)

# Prepare-PackageFiles.ps1 reads the Mozc notice from the path the local build leaves it at and
# copies it into the installer as MOZC_DICTIONARY_LICENSE.txt. The IPAdic / ICOT / Okinawa
# attribution has to ship with dict_japanese.dat, so put it where it is expected.
notice_dir="$STAGING_ROOT/MetasequoiaImeDict/source/mozc_dictionary_oss"
mkdir -p "$notice_dir"
cp "$target/mozc_dictionary_oss_README.txt" "$notice_dir/README.txt"

echo "DICTIONARY_TAG=$DICTIONARY_TAG" >> "$GITHUB_ENV"
