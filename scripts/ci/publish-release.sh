#!/usr/bin/env bash
# Attach the installer to the draft release, append the artifact details to its notes and publish.
#
# Requires GH_TOKEN, GH_REPO, TAG_NAME, ASSET_PATH, ASSET_NAME, ASSET_SHA256, SIGNING_ENABLED,
# DICTIONARY_TAG.
set -euo pipefail

gh release upload "$TAG_NAME" "$ASSET_PATH" --repo "$GH_REPO" --clobber

gh release view "$TAG_NAME" --repo "$GH_REPO" --json body --jq .body > notes.md
{
    printf '\n---\n\n'
    printf '| Field | Value |\n| --- | --- |\n'
    printf '| Installer | `%s` |\n' "$ASSET_NAME"
    printf '| SHA256 | `%s` |\n' "$ASSET_SHA256"
    printf '| Dictionaries | `%s` |\n' "$DICTIONARY_TAG"
    if [[ "$SIGNING_ENABLED" != true ]]; then
        printf '\nThis build is **unsigned**. Windows warns on launch, and `uiAccess` does not take effect, so the candidate window cannot float over elevated applications.\n'
    fi
} >> notes.md

gh release edit "$TAG_NAME" --repo "$GH_REPO" --draft=false --prerelease --notes-file notes.md
echo "Published $TAG_NAME with $ASSET_NAME"
