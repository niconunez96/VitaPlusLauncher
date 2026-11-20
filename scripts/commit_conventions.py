#!/usr/bin/env python3
"""Shared parser for VitaShelf Launcher's release-intent commits."""

from __future__ import annotations

import re
from collections.abc import Iterable

LEVELS = ("PATCH", "MINOR", "MAJOR")
LEVEL_WEIGHT = {level: index for index, level in enumerate(LEVELS)}
HEADER = re.compile(
    r"^(?P<level>PATCH|MINOR|MAJOR)(?:\([a-z0-9][a-z0-9._/-]*\))?!?: (?P<summary>\S.*)$"
)
SEMVER = re.compile(r"^v?(?P<major>0|[1-9]\d*)\.(?P<minor>0|[1-9]\d*)\.(?P<patch>0|[1-9]\d*)$")


def subject(message: str) -> str:
    """Return the first non-empty line, as Git does for a commit subject."""
    return next((line.strip() for line in message.splitlines() if line.strip()), "")


def validate_commit_message(message: str) -> list[str]:
    """Return human-readable policy violations for a commit message."""
    commit_subject = subject(message)
    if not commit_subject:
        return ["commit subject is required"]
    if not HEADER.fullmatch(commit_subject):
        return [
            "subject must be '<PATCH|MINOR|MAJOR>(optional-scope): summary'",
            f"received: {commit_subject!r}",
        ]
    return []


def bump_for_message(message: str) -> str:
    """Extract the release level after validating the conventional subject."""
    errors = validate_commit_message(message)
    if errors:
        raise ValueError("; ".join(errors))
    match = HEADER.fullmatch(subject(message))
    assert match is not None
    return match.group("level")


def highest_bump(messages: Iterable[str]) -> str:
    """Return the greatest semantic impact in a non-empty commit collection."""
    levels = [bump_for_message(message) for message in messages]
    if not levels:
        raise ValueError("at least one commit is required to select a release level")
    return max(levels, key=LEVEL_WEIGHT.__getitem__)


def next_semver(current: str, bump: str) -> str:
    """Increment a v-prefixed semantic version for the given release level."""
    match = SEMVER.fullmatch(current)
    if not match:
        raise ValueError(f"invalid semantic version: {current!r}")
    if bump not in LEVEL_WEIGHT:
        raise ValueError(f"invalid release level: {bump!r}")

    major, minor, patch = (int(match.group(name)) for name in ("major", "minor", "patch"))
    if bump == "MAJOR":
        major, minor, patch = major + 1, 0, 0
    elif bump == "MINOR":
        minor, patch = minor + 1, 0
    else:
        patch += 1
    return f"v{major}.{minor}.{patch}"
