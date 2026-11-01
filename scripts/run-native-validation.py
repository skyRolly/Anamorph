#!/usr/bin/env python3
"""Collect a native evidence packet or archive an existing (even partial) packet."""
import sys
from native_validation import main

if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, RuntimeError, OSError) as error:
        print(f"native validation: {error}", file=sys.stderr)
        sys.exit(2)
