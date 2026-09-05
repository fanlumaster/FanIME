#!/usr/bin/env bash
# Build the pull request release-please just opened, then merge it.
#
# Merging is what carries the release forward: the resulting push runs the release workflow again,
# release-please turns the release commit into the draft release, and the build jobs take over.
#
# Getting a check onto that pull request is the awkward part. GITHUB_TOKEN may not start workflows,
# so the pull_request run is created in action_required and never executes; a push to the branch
# does not produce a run at all. workflow_dispatch is the exception GitHub allows, so dispatch CI on
# the release branch and wait for it. The checks land on the pull request's head commit, which is
# what lets required_status_checks be turned on for this repository (see MSIME-Windows#121). This
# mirrors what MSIME-Apple's release automation already does.
#
# The pull request is found by branch rather than from release-please's payload. It only reports a
# pull request as created once, but it rewrites that branch on every later push to main, and a
# release pull request that could not be merged the first time -- main advancing mid-build is the
# normal way that happens -- would then never be retried.
#
# Requires GH_TOKEN and GH_REPO.
set -euo pipefail

branch=$(gh api "repos/$GH_REPO/git/matching-refs/heads/release-please--" \
    --jq '.[0].ref // empty' | sed 's|^refs/heads/||')
if [[ -z "$branch" ]]; then
    echo "No release branch exists; nothing to build or merge."
    exit 0
fi

number=$(gh pr list --repo "$GH_REPO" --head "$branch" --state open --limit 1 \
    --json number --jq '.[0].number // empty')
if [[ -z "$number" ]]; then
    echo "Release branch $branch has no open pull request; nothing to merge."
    exit 0
fi

pr_json=$(gh pr view "$number" --repo "$GH_REPO" \
    --json state,isDraft,headRefName,headRefOid,baseRefName,baseRefOid)
state=$(jq -er .state <<< "$pr_json")
is_draft=$(jq -r .isDraft <<< "$pr_json")
head_branch=$(jq -er .headRefName <<< "$pr_json")
head_sha=$(jq -er .headRefOid <<< "$pr_json")
base_branch=$(jq -er .baseRefName <<< "$pr_json")
base_sha=$(jq -er .baseRefOid <<< "$pr_json")

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

# main can move while the build runs. Merging then would rebase the release commit onto something
# that was never tested, so leave the pull request for the next release run instead.
current_base_sha=$(gh api "repos/$GH_REPO/git/ref/heads/$base_branch" --jq .object.sha)
if [[ "$current_base_sha" != "$base_sha" ]]; then
    echo "main advanced while release pull request #$number was being tested; leaving it open."
    exit 0
fi

current_head_sha=$(gh pr view "$number" --repo "$GH_REPO" --json headRefOid --jq .headRefOid)
if [[ "$current_head_sha" != "$head_sha" ]]; then
    echo "Release pull request #$number changed after CI completed; refusing to merge it." >&2
    exit 1
fi

echo "Merging release pull request #$number"
gh pr merge "$number" --repo "$GH_REPO" --merge
