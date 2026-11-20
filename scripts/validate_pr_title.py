#!/usr/bin/env python3
"""Validate a pull request title against the release-intent convention."""

from __future__ import annotations

import argparse
import sys

from commit_conventions import validate_commit_message


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--title", required=True, help="pull request title to validate")
    args = parser.parse_args()

    errors = validate_commit_message(args.title)
    if not errors:
        return 0

    print("invalid pull request title:", file=sys.stderr)
    for error in errors:
        print(f"- {error}", file=sys.stderr)
    print("examples: PATCH: fix cache, MINOR(ui): add carousel, MAJOR!: remove legacy API", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
