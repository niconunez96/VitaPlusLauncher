#!/usr/bin/env python3
"""Validate a Git commit message against the required release-intent convention."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from commit_conventions import validate_commit_message


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--file", type=Path, required=True, help="commit message file to validate")
    args = parser.parse_args()

    errors = validate_commit_message(args.file.read_text(encoding="utf-8"))
    if not errors:
        return 0
    print("invalid commit message:", file=sys.stderr)
    for error in errors:
        print(f"- {error}", file=sys.stderr)
    print("examples: PATCH: fix cache, MINOR(ui): add carousel, MAJOR!: remove legacy API", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
