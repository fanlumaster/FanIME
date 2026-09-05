#!/usr/bin/env bash
# Get a passing check onto the pull request release-please just opened, and stop there.
#
# This script used to merge that pull request too, which is what made releases automatic. Releases
# are manual now: the code signing certificate has a limited number of signatures, and an automatic
# release spends one on every merge to main whether or not anybody wanted a build. Merging the
# release pull request is a deliberate human act, and building and signing the resulting draft is a
# second one (workflow_dispatch on release.yml).
#
# Getting a check onto the pull request is still automatic, and still awkward. GITHUB_TOKEN may not
# start workflows, so the pull_request run is created in action_required and never executes; a push
# to the branch does not produce a run at all. workflow_dispatch is the exception GitHub allows, so
# dispatch CI on the release branch and wait for it. The checks land on the pull request's head
# commit, which is what lets required_status_checks stay on for this repository (MSIME-Windows#121)
# without leaving the release pull request permanently unmergeable.
#
# The pull request is found by branch rather than from release-please's payload, which only reports
# a pull request as created once but rewrites that branch on every later push to main.
#
# Requires GH_TOKEN and GH_REPO.
set -euo pipefail

branch=$(gh api "repos/$GH_REPO/git/matching-refs/heads/release-please--" \
    --jq '.[0].ref // empty' | sed 's|^refs/heads/||')
if [[ -z "$branch" ]]; then
    echo "No release branch exists; nothing to test."
    exit 0
fi

number=$(gh pr list --repo "$GH_REPO" --head "$branch" --state open --limit 1 \
    --json number --jq '.[0].number // empty')
if [[ -z "$number" ]]; then
    echo "Release branch $branch has no open pull request; nothing to test."
    exit 0
fi

pr_json=$(gh pr view "$number" --repo "$GH_REPO" \
    --json state,isDraft,headRefName,headRefOid,baseRefName)
state=$(jq -er .state <<< "$pr_json")
is_draft=$(jq -r .isDraft <<< "$pr_json")
head_branch=$(jq -er .headRefName <<< "$pr_json")
head_sha=$(jq -er .headRefOid <<< "$pr_json")
base_branch=$(jq -er .baseRefName <<< "$pr_json")

if [[ "$state" != OPEN || "$is_draft" != false || "$base_branch" != main ]]; then
    echo "Release pull request #$number is not an open, non-draft pull request against main." >&2
    exit 1
fi
if [[ "$head_branch" != release-please--* ]]; then
    echo "Release pull request #$number has an unexpected head branch: $head_branch" >&2
    exit 1
fi

echo "Dispatching CI for release pull request #$number at $head_sha"
gh workflow run ci.yml --repo "$GH_REPO" --ref "$head_branch"

# The run does not appear immediately, and gh has no way to ask for the run a dispatch created, so
# poll for one on this branch at this exact commit.
run_id=""
for _ in $(seq 1 150); do
    run_id=$(gh run list --repo "$GH_REPO" --workflow ci.yml --branch "$head_branch" \
        --event workflow_dispatch --limit 20 --json databaseId,headSha \
        --jq "map(select(.headSha == \"$head_sha\")) | first | .databaseId // empty")
    [[ -n "$run_id" ]] && break
    sleep 2
done
if [[ -z "$run_id" ]]; then
    echo "Timed out waiting for CI to start for release pull request #$number at $head_sha." >&2
    exit 1
fi

gh run watch "$run_id" --repo "$GH_REPO" --exit-status --interval 15

echo "Release pull request #$number is green at $head_sha and ready to merge when you want a release."
