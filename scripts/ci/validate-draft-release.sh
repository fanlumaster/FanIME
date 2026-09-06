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

# The target has to be on main. Without this, anyone able to create a draft release could point it
# at an arbitrary commit -- a fork's branch, an abandoned branch, a commit that never passed review --
# and dispatch this workflow to get that code compiled and signed into a published installer.
#
# The check belongs here rather than in the build jobs. This runs in the prepare job, which checks
# out the default branch, so both this script and the answer it gets come from trusted code. The
# equivalent check in the package job runs after the untrusted commit is already checked out, which
# means the script performing it is itself supplied by the commit under test; the TSF job, which
# compiles, had no such check at all. Those remain as defence in depth, not as the gate.
#
# compare/main...<sha> reports "behind" when the commit is an ancestor of main and "identical" when
# it is main's tip. Anything else means it is not in main's history.
comparison=$(gh api "repos/$GH_REPO/compare/main...$target_sha" --jq .status)
if [[ "$comparison" != "behind" && "$comparison" != "identical" ]]; then
    echo "Draft release target $target_sha is not in main's history (compare status: $comparison)." >&2
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
