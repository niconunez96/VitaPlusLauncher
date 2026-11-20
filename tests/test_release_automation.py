#!/usr/bin/env python3
"""Regression tests for the release-automation helpers."""

from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from commit_conventions import (  # noqa: E402
    bump_for_message,
    highest_bump,
    next_semver,
    validate_commit_message,
)


class CommitConventionTests(unittest.TestCase):
    def test_accepts_each_release_level(self) -> None:
        for level in ("PATCH", "MINOR", "MAJOR"):
            message = f"{level}(release): describe the change\n\nDetails.\n"
            self.assertEqual([], validate_commit_message(message))
            self.assertEqual(level, bump_for_message(message))

    def test_rejects_missing_or_invalid_release_level(self) -> None:
        self.assertTrue(validate_commit_message("feat: add a feature\n"))
        self.assertTrue(validate_commit_message("MINOR add a feature\n"))
        self.assertTrue(validate_commit_message("PATCH: \n"))

    def test_selects_the_highest_bump(self) -> None:
        messages = [
            "PATCH: fix crash\n",
            "MINOR(ui): add new view\n",
            "MAJOR(api)!: remove deprecated endpoint\n",
        ]
        self.assertEqual("MAJOR", highest_bump(messages))


class SemanticVersionTests(unittest.TestCase):
    def test_increments_each_semantic_component(self) -> None:
        self.assertEqual("v2.4.6", next_semver("v2.4.5", "PATCH"))
        self.assertEqual("v2.5.0", next_semver("v2.4.5", "MINOR"))
        self.assertEqual("v3.0.0", next_semver("v2.4.5", "MAJOR"))

    def test_rejects_an_invalid_current_version(self) -> None:
        with self.assertRaises(ValueError):
            next_semver("2.4", "PATCH")


class CommitMsgHookTests(unittest.TestCase):
    def test_hook_script_accepts_and_rejects_messages(self) -> None:
        hook = ROOT / "githooks" / "commit-msg"
        with tempfile.TemporaryDirectory() as temp_dir:
            message_file = Path(temp_dir) / "COMMIT_EDITMSG"
            message_file.write_text("PATCH: correct release tooling\n", encoding="utf-8")
            accepted = subprocess.run([str(hook), str(message_file)], cwd=ROOT, text=True, capture_output=True)
            self.assertEqual(0, accepted.returncode, accepted.stderr)

            message_file.write_text("fix: missing release intent\n", encoding="utf-8")
            rejected = subprocess.run([str(hook), str(message_file)], cwd=ROOT, text=True, capture_output=True)
            self.assertNotEqual(0, rejected.returncode)
            self.assertIn("invalid commit message", rejected.stderr)


class WorkflowContractTests(unittest.TestCase):
    def test_ci_workflow_gates_release_and_uploads_the_vpk(self) -> None:
        workflow = (ROOT / ".github" / "workflows" / "ci.yml").read_text(encoding="utf-8")
        self.assertIn("pull_request:", workflow)
        self.assertIn("make test", workflow)
        self.assertIn("./scripts/build-vpk.sh --clean", workflow)
        self.assertIn("needs: [validate, host-tests, build-vpk]", workflow)
        self.assertIn("gh release create", workflow)
        self.assertIn("--generate-notes", workflow)


if __name__ == "__main__":
    unittest.main()
