#!/usr/bin/env python3
"""Check that every internal link in the reference resolves to a heading.

Pandoc derives an identifier (slug) for every heading when building the PDF;
a link to a non-existent anchor becomes a LaTeX "undefined hyperref" warning
and a dead link in the PDF. This script recomputes pandoc's auto identifiers
for every heading in reference/Pico_Logo_Reference.md and verifies each
[text](#anchor) link against them, so the break is caught at review time
instead of during the dist.sh PDF build.

Usage: scripts/check_reference_anchors.py [path-to-markdown]
Exit status: 0 = all links resolve, 1 = broken links found.
"""

import re
import sys


def pandoc_slug(heading):
    """Compute pandoc's auto_identifiers slug for a heading."""
    # Strip inline formatting: backticks, emphasis, and link syntax
    text = heading.replace("`", "")
    text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text)
    text = text.strip()
    # Lowercase; keep alphanumerics, _, -, ., and spaces; drop the rest
    text = text.lower()
    text = re.sub(r"[^\w\s.-]", "", text)
    # Spaces to hyphens
    text = re.sub(r"\s+", "-", text)
    # Identifiers must start with a letter: strip leading non-letters
    text = re.sub(r"^[^a-z]+", "", text)
    return text or "section"


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "reference/Pico_Logo_Reference.md"
    with open(path, encoding="utf-8") as f:
        source = f.read()

    # Collect heading slugs, tracking pandoc's -1, -2 ... duplicate suffixes.
    slugs = set()
    counts = {}
    in_code = False
    for line in source.splitlines():
        if line.startswith("```"):
            in_code = not in_code
            continue
        if in_code:
            continue
        m = re.match(r"^(#{1,6})\s+(.*)$", line)
        if not m:
            continue
        slug = pandoc_slug(m.group(2))
        if slug in counts:
            counts[slug] += 1
            slug = f"{slug}-{counts[slug]}"
        else:
            counts[slug] = 0
        slugs.add(slug)

    # Collect internal links and check them (skip code blocks the same way).
    broken = []
    in_code = False
    for lineno, line in enumerate(source.splitlines(), 1):
        if line.startswith("```"):
            in_code = not in_code
            continue
        if in_code:
            continue
        for m in re.finditer(r"\]\(#([^)]+)\)", line):
            anchor = m.group(1)
            if anchor not in slugs:
                broken.append((lineno, anchor))

    if broken:
        print(f"{len(broken)} broken internal link(s) in {path}:")
        for lineno, anchor in broken:
            print(f"  line {lineno}: #{anchor}")
        return 1

    print(f"OK: all internal links resolve ({len(slugs)} headings)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
