#!/usr/bin/env bash
# Attach the installer to the draft release, append the artifact details to its notes and publish.
#
# GitHub has no notion of a release channel, so the two kinds of release this pipeline produces are carried by the two states it does have (MSIME-Windows#167). An automatic per-merge build is a prerelease; a build somebody deliberately dispatched is an ordinary release, and the newest one carries the Latest badge. Users therefore see two lists rather than one list with two kinds of title in it, and /releases/latest finally resolves.
#
# The prerelease flag means "automatic and uncurated" here, not "beta". The product being 内测 is stated in the notes and in docs/installation.md, which is where a claim about stability belongs; it is not something the flag can express while the flag is also the only thing separating the channels.
#
# Requires GH_TOKEN, GH_REPO, TAG_NAME, ASSET_PATH, ASSET_NAME, ASSET_SHA256, SIGNING_ENABLED,
# DICTIONARY_TAG, PRODUCT_MANIFEST, RELEASE_TRIGGER.
set -euo pipefail

gh release upload "$TAG_NAME" "$ASSET_PATH" "$PRODUCT_MANIFEST" --repo "$GH_REPO" --clobber

if [[ "$RELEASE_TRIGGER" == push ]]; then
    # Braces are required: bash takes the full-width bracket that follows as part of the name otherwise.
    title="${TAG_NAME}（自动构建）"
    banner='本版本由 CI 在合并到 `main` 后自动构建发布。'
    channel=(--prerelease=false --latest)
else
    title="$TAG_NAME"
    banner=''
    channel=(--prerelease=false --latest)
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

gh release edit "$TAG_NAME" --repo "$GH_REPO" --draft=false "${channel[@]}" --title "$title" --notes-file notes.md
echo "Published $TAG_NAME as '$title' (${channel[*]}) with $ASSET_NAME"
