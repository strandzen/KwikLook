#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 strandzen
"""Compare what the Dolphin hook relies on (dolphin-api.txt) with Dolphin's headers.

Run weekly in CI against Dolphin's development branch, so a change to a return
type, a parameter or an enum value shows up months before it reaches a Dolphin
release (tst_dolphinsymbols then catches what reaches the release).

usage: check-dolphin-api.py [--ref master] [--headers DIR]
  --ref      Dolphin branch or tag to fetch (default: master)
  --headers  use the headers in DIR instead of fetching them
Exit status: 0 = all found, 1 = something changed, 2 = couldn't fetch or read.
"""

import argparse
import pathlib
import re
import sys
import urllib.request

RAW_URL = "https://invent.kde.org/system/dolphin/-/raw/{ref}/src/kitemviews/{header}"
API_FILE = pathlib.Path(__file__).with_name("dolphin-api.txt")


def normalize(text):
    """Without comments, whitespace collapsed, no space inside brackets or before ; ,"""
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    text = re.sub(r"\s+", " ", text)
    text = re.sub(r"\s*([(){};,])\s*", r"\1", text)
    return text.replace(",}", "}")  # a trailing comma in an enum


def read_api():
    entries = []
    for number, line in enumerate(API_FILE.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.startswith("#"):
            continue
        parts = [p.strip() for p in line.split("|", 2)]
        if len(parts) != 3:
            sys.exit(f"{API_FILE.name}:{number}: expected 'header | name | declaration'")
        entries.append(tuple(parts))
    return entries


def fetch(ref, header):
    request = urllib.request.Request(RAW_URL.format(ref=ref, header=header), headers={"User-Agent": "dolphin-api-check"})
    with urllib.request.urlopen(request, timeout=30) as reply:
        return reply.read().decode("utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ref", default="master")
    parser.add_argument("--headers", type=pathlib.Path)
    args = parser.parse_args()

    entries = read_api()
    headers = {}
    for header in sorted({e[0] for e in entries}):
        try:
            text = (args.headers / header).read_text(encoding="utf-8") if args.headers else fetch(args.ref, header)
        except Exception as error:  # network, missing file: not a finding
            print(f"couldn't read {header}: {error}", file=sys.stderr)
            return 2
        headers[header] = normalize(text)

    changed = []
    for header, name, declaration in entries:
        wanted = normalize(declaration)
        if name == "enum":
            enum_name = re.match(r"enum (?:class )?(\w+)", wanted).group(1)
            found = re.search(r"enum (?:class )?" + enum_name + r"(?::\w+)?\{[^}]*\};", headers[header])
            if not found or found.group(0) != wanted:
                changed.append(f"{header}: {declaration}\n    now: {found.group(0) if found else '(gone)'}")
        elif wanted not in headers[header]:
            changed.append(f"{header}: {declaration}  ({name})")

    source = args.headers or f"Dolphin {args.ref}"
    if changed:
        print(f"Changed in {source} (the hook relies on these; see src/dolphin/dolphinsymbols.h):")
        for line in changed:
            print("  " + line)
        return 1
    print(f"All {len(entries)} declarations the hook relies on are unchanged in {source}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
