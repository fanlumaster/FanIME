#!/usr/bin/env bash
# Check that a manually requested tag names an unpublished draft release pinned to an immutable
# commit whose version.txt agrees with the tag, then report the commit and version for the build
# jobs to use. Nothing is built until this passes.
#
# Requires GH_TOKEN, GH_REPO, TAG_NAME. Writes target_sha and version to GITHUB_OUTPUT.
set -euo pipefail

if [[ ! "$TAG_NAME" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Tag must use the vMAJOR.MINOR.PATCH format, got '$TAG_NAME'." >&2
    exit 1
fi

release_json=$(gh release view "$TAG_NAME" --json isDraft,targetCommitish)

if [[ "$(jq -er .isDraft <<< "$release_json")" != true ]]; then
    echo "Release $TAG_NAME is not an unpublished draft." >&2
    exit 1
fi

target_sha=$(jq -er .targetCommitish <<< "$release_json")
if [[ ! "$target_sha" =~ ^[0-9a-f]{40}$ ]]; then
    echo "Draft release target must be an immutable full commit SHA, got '$target_sha'." >&2
    exit 1
fi

version=$(gh api "repos/$GH_REPO/contents/version.txt?ref=$target_sha" --jq .content | base64 --decode | tr -d '[:space:]')
if [[ "$TAG_NAME" != "v$version" ]]; then
    echo "Release $TAG_NAME does not match version.txt ($version)." >&2
    exit 1
fi

printf 'target_sha=%s\n' "$target_sha" >> "$GITHUB_OUTPUT"
printf 'version=%s\n' "$version" >> "$GITHUB_OUTPUT"
echo "Draft $TAG_NAME is valid: version $version at $target_sha"
