#!/usr/bin/env bash
# Merge the pull request release-please just opened.
#
# Merging is what carries the release forward: the resulting push runs the release workflow
# again, release-please turns the release commit into the draft release, and the build jobs take
# over. Left open the pull request stalls, because CI cannot run on it either -- GITHUB_TOKEN is
# not allowed to start workflows, so anything it triggers waits for a human. The pull request
# only rewrites version.txt, CHANGELOG.md and .release-please-manifest.json.
#
# Requires GH_TOKEN, GH_REPO, RELEASE_PR (the JSON release-please emits).
set -euo pipefail

number=$(jq -er .number <<< "$RELEASE_PR")
echo "Merging release pull request #$number"
gh pr merge "$number" --repo "$GH_REPO" --merge
