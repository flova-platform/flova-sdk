#!/usr/bin/env python3
"""Development-only board acceptance and SDK benchmarks (build-only by default)."""

from board_e2e.runner import main

if __name__ == "__main__":
    raise SystemExit(main())
