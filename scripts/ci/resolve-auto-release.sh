#!/usr/bin/env bash
# Decide which release an automatic run publishes, out of the two release-please invocations in the prepare job.
#
# FIRST_* comes from the invocation that recovers releases for pull requests already merged when the run started, which is the one that fires when a human merged the release pull request by hand instead of letting land-release-pr.sh do it. SECOND_* comes from the invocation after this run merged the pull request itself. On any given push at most one of them normally reports a release. The invocation between them only refreshes the pull request and creates no release, so it is not consulted here.
#
# When both do report one, the push carried a merged release pull request and further releasable commits at once. The second release is the newer version and supersedes the first, so it is the one that gets built; the first is left as a draft for a manual dispatch to pick up, which is the only case where the automatic path still leaves something behind.
#
# The manual path gets its immutability guarantees from validate-draft-release.sh. Nothing validates release-please's own outputs, so the same two invariants are checked here: the commit is a full SHA rather than a branch name, and the tag agrees with the version.
#
# Requires FIRST_CREATED, FIRST_TAG, FIRST_VERSION, FIRST_SHA and the matching SECOND_* values.
# Writes release_created, tag_name, version and target_sha to GITHUB_OUTPUT.
set -euo pipefail

if [[ "${SECOND_CREATED:-}" == true ]]; then
    tag=$SECOND_TAG
    version=$SECOND_VERSION
    target_sha=$SECOND_SHA
    if [[ "${FIRST_CREATED:-}" == true ]]; then
        echo "::warning::Both release-please invocations created a release. Building $SECOND_TAG; $FIRST_TAG stays a draft until somebody dispatches it."
    fi
elif [[ "${FIRST_CREATED:-}" == true ]]; then
    tag=$FIRST_TAG
    version=$FIRST_VERSION
    target_sha=$FIRST_SHA
else
    printf 'release_created=false\n' >> "$GITHUB_OUTPUT"
    echo "No release was created on this push; nothing to build."
    exit 0
fi

if [[ ! "$target_sha" =~ ^[0-9a-f]{40}$ ]]; then
    echo "Release $tag must be tagged at an immutable full commit SHA, got '$target_sha'." >&2
    exit 1
fi
if [[ "$tag" != "v$version" ]]; then
    echo "Release tag $tag does not match version $version." >&2
    exit 1
fi

{
    printf 'release_created=true\n'
    printf 'tag_name=%s\n' "$tag"
    printf 'version=%s\n' "$version"
    printf 'target_sha=%s\n' "$target_sha"
} >> "$GITHUB_OUTPUT"
echo "Publishing $tag (version $version) at $target_sha"
