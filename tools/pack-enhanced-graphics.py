#!/usr/bin/env python3
"""Validate indexed PNG masters and pack the Enhanced Amiga runtime banks."""

from __future__ import annotations

import argparse
import sys

from amiga_graphics_format import ASSETS, build_runtime_banks


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="validate masters and require runtime banks to already match",
    )
    args = parser.parse_args()

    try:
        banks = build_runtime_banks()
        changed = []
        for path, packed in banks.items():
            current = path.read_bytes() if path.is_file() else None
            if current == packed:
                continue
            if args.check:
                raise ValueError(f"Runtime bank is stale or missing: {path}")
            path.write_bytes(packed)
            changed.append(path.name)
    except (OSError, ValueError) as error:
        print(f"Enhanced graphics error: {error}", file=sys.stderr)
        return 1

    dimensions = ", ".join(
        f"{asset.filename}={asset.width}x{asset.height}" for asset in ASSETS
    )
    if changed:
        print(f"Enhanced graphics packed: {', '.join(changed)}")
    else:
        print("Enhanced graphics banks are current")
    print(f"Validated indexed OCS masters: {dimensions}; transparency=0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
