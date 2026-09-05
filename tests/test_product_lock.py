import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("product_lock", ROOT / "scripts/product_lock.py")
lock = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lock)


class ProductLockTests(unittest.TestCase):
    def setUp(self):
        self.data = lock.load(ROOT / "product-lock.json")

    def test_mutable_refs_and_missing_components_are_rejected(self):
        for component in lock.REPOSITORIES:
            for ref in ("main", "latest", "v1.0", "abc123", "a" * 40 + "\n"):
                with self.subTest(component=component, ref=ref):
                    changed = copy.deepcopy(self.data)
                    changed["repositories"][component]["commit"] = ref
                    with self.assertRaises(ValueError):
                        lock.validate(changed)
        del self.data["repositories"]["engine"]
        with self.assertRaises(ValueError):
            lock.validate(self.data)

    def test_data_tag_and_complete_artifact_set_are_required(self):
        for tag in ("latest", "main", "../dict-test", "dict-test\n"):
            changed = copy.deepcopy(self.data)
            changed["dictionary"]["tag"] = tag
            with self.assertRaises(ValueError):
                lock.validate(changed)
        for name in lock.ASSETS:
            changed = copy.deepcopy(self.data)
            del changed["dictionary"]["assets"][name]
            with self.assertRaises(ValueError):
                lock.validate(changed)

    def test_assets_cannot_escape_the_output_directory(self):
        self.data["dictionary"]["assets"]["../notice.txt"] = "a" * 64
        with self.assertRaises(ValueError):
            lock.validate(self.data)

    def test_modern_dictionary_requires_a_compatible_locked_manifest(self):
        self.data['dictionary']['tag'] = 'dict-2026.09.06'
        with self.assertRaises(ValueError):
            lock.validate(self.data)
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.fixture_assets(directory)
            for name in lock._product.DESKTOP_FILES:
                (directory / name).write_bytes(b'MSJPDT1\0fixture' if name == 'dict_japanese.dat' else b'fixture')
            product = {'manifest_version': 1, 'format_version': 1, 'profile': 'desktop',
                       'engine_compatibility': {'dictionary_format': 1, 'japanese_model_magic': 'MSJPDT1'},
                       'files': {name: {'sha256': lock.sha256(directory / name), 'size': (directory / name).stat().st_size}
                                 for name in lock._product.DESKTOP_FILES}}
            manifest_path = directory / lock.PRODUCT_MANIFEST
            manifest_path.write_text(json.dumps(product))
            for name in (*lock.ASSETS, lock.PRODUCT_MANIFEST):
                self.data['dictionary']['assets'][name] = lock.sha256(directory / name)
            lock.validate(self.data)
            lock.verify_assets(directory, self.data)
            # Even a deliberately updated digest cannot declare an unsupported format compatible.
            product['format_version'] = 2
            manifest_path.write_text(json.dumps(product))
            self.data['dictionary']['assets'][lock.PRODUCT_MANIFEST] = lock.sha256(manifest_path)
            with self.assertRaises(ValueError):
                lock.verify_assets(directory, self.data)

    def fixture_assets(self, directory):
        for name in lock.ASSETS:
            value = (name + " fixture").encode()
            (directory / name).write_bytes(value)
            self.data["dictionary"]["assets"][name] = hashlib.sha256(value).hexdigest()

    def test_mutating_both_database_and_upstream_checksums_still_fails(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.fixture_assets(directory)
            lock.verify_assets(directory, self.data)
            (directory / "msime.db").write_bytes(b"replacement database")
            (directory / "SHA256SUMS.txt").write_text(lock.sha256(directory / "msime.db") + "  msime.db\n")
            with self.assertRaises(ValueError):
                lock.verify_assets(directory, self.data)

    def test_failed_download_never_overwrites_previous_usable_data(self):
        with tempfile.TemporaryDirectory() as temporary:
            staging = Path(temporary)
            target = staging / "MetasequoiaImeDict/out"
            target.mkdir(parents=True)
            self.fixture_assets(target)
            before = {path.name: path.read_bytes() for path in target.iterdir()}

            def corrupt_download(command, **kwargs):
                incoming = Path(command[command.index("--dir") + 1])
                for name, value in before.items():
                    (incoming / name).write_bytes(value)
                (incoming / "others.db").write_bytes(b"truncated")

            with mock.patch.object(lock.subprocess, "run", side_effect=corrupt_download):
                with self.assertRaises(ValueError):
                    lock.fetch_dictionaries(staging, self.data)
            self.assertEqual(before, {path.name: path.read_bytes() for path in target.iterdir()})

    def test_helpcode_checkout_must_match_the_reviewed_commit(self):
        with mock.patch.object(lock, "git", return_value=self.data["repositories"]["helpcode"]["commit"]):
            lock.verify_checkout("helpcode", ROOT, self.data)

    def test_wrong_checkout_is_rejected(self):
        with mock.patch.object(lock, "git", return_value="0" * 40):
            with self.assertRaises(ValueError):
                lock.verify_checkout("helpcode", ROOT, self.data)

    def test_a_path_that_is_not_a_submodule_cannot_pass_as_the_engine(self):
        for reply in ("", "100644 blob " + "0" * 40 + "\tvendor/MetasequoiaImeEngine"):
            with self.subTest(reply=reply):
                with mock.patch.object(lock, "git", return_value=reply):
                    with self.assertRaises(ValueError):
                        lock.verify_contracts(ROOT, self.data)

    def test_the_lock_records_the_engine_commit_the_submodule_actually_points_at(self):
        self.assertEqual(lock.engine_gitlink(ROOT), self.data["repositories"]["engine"]["commit"])

    def test_independently_bumped_tsf_contract_is_rejected(self):
        expected = self.data["repositories"]["engine"]["commit"]
        with mock.patch.object(lock, "git", return_value=f"160000 commit {expected}\tvendor/MetasequoiaImeEngine"):
            lock.verify_contracts(ROOT, self.data)
        with mock.patch.object(lock, "git", return_value="160000 commit " + "0" * 40 + "\tvendor/MetasequoiaImeEngine"):
            with self.assertRaises(ValueError):
                lock.verify_contracts(ROOT, self.data)

    def compare_status(self, status):
        def api(endpoint):
            return {"status": status} if "/compare/" in endpoint else {"default_branch": "main"}
        return api

    def test_a_locked_commit_that_never_reached_its_default_branch_is_rejected(self):
        # An ancestor of the default branch compares as behind, or identical when it is the tip.
        for status in ("behind", "identical"):
            with self.subTest(status=status):
                with mock.patch.object(lock, "api", side_effect=self.compare_status(status)):
                    lock.verify_published(self.data)
        # ahead and diverged both mean the commit sits on something nobody merged. This is what
        # locking a commit that only exists on a pull request branch looks like.
        for status in ("ahead", "diverged"):
            with self.subTest(status=status):
                with mock.patch.object(lock, "api", side_effect=self.compare_status(status)):
                    with self.assertRaises(ValueError):
                        lock.verify_published(self.data)

    def test_a_commit_the_component_repository_does_not_have_is_rejected(self):
        def missing(endpoint):
            if "/compare/" in endpoint:
                raise subprocess.CalledProcessError(1, "gh")
            return {"default_branch": "main"}

        with mock.patch.object(lock, "api", side_effect=missing):
            with self.assertRaises(ValueError):
                lock.verify_published(self.data)

    def test_manifest_records_exact_source_and_lock_bytes(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "manifest.json"
            commit = "1" * 40
            subprocess.run(["python3", str(ROOT / "scripts/product_lock.py"), "manifest",
                            "--windows-commit", commit, "--output", str(output)], check=True)
            manifest = json.loads(output.read_text())
            self.assertEqual(manifest["repositories"]["windows"]["commit"], commit)
            self.assertEqual(manifest["dictionary"], self.data["dictionary"])
            self.assertEqual(manifest["lock_sha256"], lock.sha256(ROOT / "product-lock.json"))


if __name__ == "__main__":
    unittest.main()
