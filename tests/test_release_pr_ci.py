"""Exercise the release merge gate with a fake GitHub CLI and no network or writes."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'scripts/ci/land-release-pr.sh'
SHA = 'a' * 40
FAKE_GH = r'''#!/usr/bin/env python3
import json, os, sys
args = sys.argv[1:]
with open(os.environ['CALL_LOG'], 'a') as log:
    log.write(json.dumps(args) + '\n')
if args[:2] == ['api', 'repos/test/repo/git/matching-refs/heads/release-please--']:
    print('release-please--branches--main')
elif args[:2] == ['pr', 'list']:
    print('12')
elif args[:2] == ['pr', 'view']:
    if 'mergeCommit' in args:
        print('b' * 40)
    else:
        print(json.dumps(dict(state='OPEN', isDraft=False,
            headRefName='release-please--branches--main', headRefOid='a'*40, baseRefName='main')))
elif args[:2] == ['run', 'list']:
    assert args[args.index('--event')+1] == 'pull_request'
    assert 'a'*40 in args[args.index('--jq')+1]
    print('123')
elif args[:2] == ['run', 'watch']:
    assert args[2] == '123' and '--exit-status' in args
    sys.exit(int(os.environ['CI_RESULT']))
elif args[:2] == ['pr', 'merge']:
    assert '--admin' not in args and '--auto' not in args
    assert args[args.index('--match-head-commit')+1] == 'a'*40
elif args[0] == 'api' and '/compare/' in args[1]:
    print('ahead')
else:
    raise AssertionError(args)
'''


class ReleasePRCI(unittest.TestCase):
    def run_gate(self, result):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            gh = directory / 'gh'
            gh.write_text(FAKE_GH)
            gh.chmod(0o755)
            log = directory / 'calls.jsonl'
            env = dict(os.environ, GH_REPO='test/repo', CALL_LOG=str(log), CI_RESULT=str(result))
            env['PATH'] = str(directory) + os.pathsep + env['PATH']
            run = subprocess.run(['bash', str(SCRIPT)], env=env, text=True, capture_output=True)
            calls = [json.loads(line) for line in log.read_text().splitlines()]
            return run, calls

    def test_passing_pr_ci_merges_only_tested_head(self):
        run, calls = self.run_gate(0)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertTrue(any(call[:2] == ['pr', 'merge'] for call in calls))
        self.assertFalse(any(call[:2] == ['workflow', 'run'] for call in calls))

    def test_failed_pr_ci_never_merges(self):
        run, calls = self.run_gate(1)
        self.assertNotEqual(run.returncode, 0)
        self.assertFalse(any(call[:2] == ['pr', 'merge'] for call in calls))


if __name__ == '__main__':
    unittest.main()
