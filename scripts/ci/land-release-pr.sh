#!/usr/bin/env bash
# Get a passing check onto the pull request release-please just opened, then merge it.
#
# Merging here is what makes releases automatic. The merge writes the version bump and the CHANGELOG onto main, and the release-please invocation that runs after this script in release.yml turns that commit into the draft release the rest of the run builds, signs and publishes. Nothing in the pipeline waits for a human any more.
#
# The merge deliberately uses GITHUB_TOKEN. A push made with that token does not start a workflow run, so merging here does not produce a second Release run racing this one through the concurrency group: the release is created and built inside the same run that merged it. A personal access token would break that property rather than improve it.
#
# Getting a check onto the pull request is still awkward. GITHUB_TOKEN may not start workflows, so the pull_request run is created in action_required and never executes; a push to the branch does not produce a run at all. workflow_dispatch is the exception GitHub allows, so dispatch CI on the release branch and wait for it. The checks land on the pull request's head commit, which is what lets required_status_checks stay on for this repository (MSIME-Windows#121) without leaving the release pull request permanently unmergeable.
#
# The pull request is found by branch rather than from release-please's payload, which only reports a pull request as created once but rewrites that branch on every later push to main.
#
# Requires GH_TOKEN and GH_REPO.
set -euo pipefail

branch=$(gh api "repos/$GH_REPO/git/matching-refs/heads/release-please--" \
    --jq '.[0].ref // empty' | sed 's|^refs/heads/||')
if [[ -z "$branch" ]]; then
    echo "No release branch exists; nothing to land."
    exit 0
fi

number=$(gh pr list --repo "$GH_REPO" --head "$branch" --state open --limit 1 \
    --json number --jq '.[0].number // empty')
if [[ -z "$number" ]]; then
    echo "Release branch $branch has no open pull request; nothing to land."
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

# Merge the commit that CI just proved, not whatever the branch has drifted to. release-please
# rewrites this branch on every push to main, and the concurrency group only serialises Release
# runs, so nothing here rules out the branch moving while CI was running.
echo "Release pull request #$number is green at $head_sha; merging it."
gh pr merge "$number" --repo "$GH_REPO" --merge --match-head-commit "$head_sha"

# release-please reads main through the API, and the merge commit is not reliably visible there the
# moment gh returns. Wait for main to actually carry it, so the invocation after this script sees
# the release commit instead of quietly finding nothing to release.
merge_sha=$(gh pr view "$number" --repo "$GH_REPO" --json mergeCommit --jq '.mergeCommit.oid // empty')
if [[ -z "$merge_sha" ]]; then
    echo "Release pull request #$number reports no merge commit after merging." >&2
    exit 1
fi
for _ in $(seq 1 30); do
    status=$(gh api "repos/$GH_REPO/compare/$merge_sha...main" --jq .status 2>/dev/null || echo unknown)
    if [[ "$status" == identical || "$status" == ahead ]]; then
        echo "Merged release pull request #$number as $merge_sha, now on main."
        exit 0
    fi
    sleep 2
done

echo "Merge commit $merge_sha did not become reachable from main in time." >&2
exit 1
