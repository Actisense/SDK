#!/usr/bin/env python3
"""Tiny shared test harness for the candirect headless tests.

Keeps the [OK]/[FAIL] output idiom of the n2ksender tests while giving a real
non-zero exit code when something fails, so the suite can run headless in CI.
"""

import os
import sys

# Make the candirect module importable regardless of the current directory.
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


class TestRunner:
    def __init__(self, title: str) -> None:
        self.title = title
        self.passed = 0
        self.failed = 0
        print(title)
        print("=" * 60)

    def check(self, description: str, condition: bool, detail: str = "") -> None:
        if condition:
            self.passed += 1
            print(f"[OK] {description}")
        else:
            self.failed += 1
            print(f"[FAIL] {description}" + (f" - {detail}" if detail else ""))

    def equal(self, description: str, actual, expected) -> None:
        self.check(description, actual == expected, f"expected {expected!r}, got {actual!r}")

    def finish(self) -> None:
        print("-" * 60)
        print(f"{self.title}: {self.passed} passed, {self.failed} failed")
        if self.failed:
            sys.exit(1)


def hexstr(data: bytes) -> str:
    return " ".join(f"{b:02X}" for b in data)
