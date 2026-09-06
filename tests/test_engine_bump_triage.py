"""Guard the conditions on the engine-bump auto-merge step.

This exists because of a specific failure: the contract-lock guard was written into the step's HEAD
environment value rather than its `if:`, so it was not a condition at all. Auto-merge ran whether or
not the lock verified, and the comment the step posts printed the commit followed by the literal
text `&& steps.lock.outputs.locked == 'yes'`. Nothing failed, so nothing surfaced it.
"""
from pathlib import Path
import unittest

import yaml

WORKFLOW = Path(__file__).resolve().parents[1] / '.github/workflows/engine-bump-triage.yml'


def steps():
    # `on:` parses as the boolean True in YAML 1.1, which is why the job lookup goes through 'jobs'.
    workflow = yaml.safe_load(WORKFLOW.read_text(encoding='utf-8'))
    return [step for job in workflow['jobs'].values() for step in job.get('steps', [])]


def step_named(name):
    matches = [step for step in steps() if step.get('name') == name]
    if len(matches) != 1:
        raise AssertionError(f'expected exactly one step named {name!r}, found {len(matches)}')
    return matches[0]


class AutoMergeGuards(unittest.TestCase):
    def test_auto_merge_requires_both_the_verdict_and_the_lock(self):
        condition = step_named('Enable auto-merge')['if']
        self.assertIn("steps.classify.outputs.verdict == 'inert'", condition)
        self.assertIn("steps.lock.outputs.locked == 'yes'", condition)

    def test_no_step_condition_leaked_into_an_environment_value(self):
        for step in steps():
            for key, value in (step.get('env') or {}).items():
                self.assertNotIn('steps.', str(value).split('}}')[-1],
                                 f'{step.get("name")}: env {key} carries a condition fragment')

    def test_the_comment_prints_only_the_commit_range(self):
        env = step_named('Enable auto-merge')['env']
        for key in ('BASE', 'HEAD'):
            self.assertRegex(env[key], r'^\$\{\{ steps\.range\.outputs\.\w+ \}\}$')


if __name__ == '__main__':
    unittest.main()
