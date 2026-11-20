#!/usr/bin/env python3
"""Calculate a semantic release tag from release-intent commits in a Git range."""

from __future__ import annotations

import argparse
import subprocess

from commit_conventions import highest_bump, next_semver


def messages_in_range(revision_range: str) -> list[str]:
    result = subprocess.run(
        ["git", "log", "--no-merges", "--format=%B%x00", revision_range],
        check=True,
        capture_output=True,
        text=True,
    )
    return [message for message in result.stdout.split("\0") if message.strip()]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--current", required=True, help="current v-prefixed semantic tag")
    parser.add_argument("--range", dest="revision_range", required=True, help="Git revision range")
    args = parser.parse_args()

    bump = highest_bump(messages_in_range(args.revision_range))
    print(next_semver(args.current, bump))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
