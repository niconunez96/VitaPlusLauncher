#!/usr/bin/env python3
"""Validate every commit in an inclusive Git revision range."""

from __future__ import annotations

import argparse
import subprocess
import sys

from commit_conventions import validate_commit_message


def git_messages(revision_range: str) -> list[tuple[str, str]]:
    result = subprocess.run(
        ["git", "log", "--no-merges", "--format=%H%x00%B%x00", revision_range],
        check=True,
        capture_output=True,
        text=True,
    )
    fields = result.stdout.split("\0")
    return [(fields[index], fields[index + 1]) for index in range(0, len(fields) - 1, 2) if fields[index]]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("revision_range", help="Git revision range, for example abc..def")
    args = parser.parse_args()

    messages = git_messages(args.revision_range)
    if not messages:
        print(f"no commits found in {args.revision_range}", file=sys.stderr)
        return 1

    invalid = [(sha, validate_commit_message(message)) for sha, message in messages]
    invalid = [(sha, errors) for sha, errors in invalid if errors]
    if not invalid:
        print(f"validated {len(messages)} commit(s) in {args.revision_range}")
        return 0

    for sha, errors in invalid:
        print(f"{sha[:12]}:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
