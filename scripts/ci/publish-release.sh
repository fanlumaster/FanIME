#!/usr/bin/env bash
# Attach the installer to the draft release, append the artifact details to its notes and publish.
#
# GitHub offers three release states and no notion of a channel, so an automatic per-merge build and a release somebody deliberately cut both land in the same Pre-release list that docs/installation.md presents to users as the 内测版 channel. Every releasable merge now produces one of these, so the two are told apart in the title and the notes rather than by where they appear (MSIME-Windows#167).
#
# Requires GH_TOKEN, GH_REPO, TAG_NAME, ASSET_PATH, ASSET_NAME, ASSET_SHA256, SIGNING_ENABLED,
# DICTIONARY_TAG, PRODUCT_MANIFEST, RELEASE_TRIGGER.
set -euo pipefail

gh release upload "$TAG_NAME" "$ASSET_PATH" "$PRODUCT_MANIFEST" --repo "$GH_REPO" --clobber

if [[ "$RELEASE_TRIGGER" == push ]]; then
    # Braces are required: bash takes the full-width bracket that follows as part of the name otherwise.
    title="${TAG_NAME}（自动构建）"
    banner='本版本由 CI 在合并到 `main` 后自动构建发布，未经人工挑选。想要经过挑选的内测版本，请选择标题不带「自动构建」的发布。'
else
    title="$TAG_NAME"
    banner=''
fi

gh release view "$TAG_NAME" --repo "$GH_REPO" --json body --jq .body > notes.md
{
    if [[ -n "$banner" ]]; then
        printf '\n> %s\n' "$banner"
    fi
    printf '\n---\n\n'
    printf '| Field | Value |\n| --- | --- |\n'
    printf '| Installer | `%s` |\n' "$ASSET_NAME"
    printf '| SHA256 | `%s` |\n' "$ASSET_SHA256"
    printf '| Dictionaries | `%s` |\n' "$DICTIONARY_TAG"
    printf '| Build inputs | `product-manifest.json` (attached and installed) |\n'
    if [[ "$SIGNING_ENABLED" != true ]]; then
        printf '\nThis build is **unsigned**. Windows warns on launch, and `uiAccess` does not take effect, so the candidate window cannot float over elevated applications.\n'
    fi
} >> notes.md

gh release edit "$TAG_NAME" --repo "$GH_REPO" --draft=false --prerelease --title "$title" --notes-file notes.md
echo "Published $TAG_NAME as '$title' with $ASSET_NAME"
