#!/usr/bin/env bash
# Re-check right before publishing that the draft is still a draft and still points at the commit
# that was actually built, so a release edited mid-build is never published with these artifacts.
#
# Requires GH_TOKEN, GH_REPO, TAG_NAME, TARGET_SHA.
set -euo pipefail

release_json=$(gh release view "$TAG_NAME" --repo "$GH_REPO" --json isDraft,targetCommitish)
test "$(jq -er .isDraft <<< "$release_json")" = true
test "$(jq -er .targetCommitish <<< "$release_json")" = "$TARGET_SHA"
echo "Draft $TAG_NAME still targets $TARGET_SHA."
