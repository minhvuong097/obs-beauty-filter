#!/usr/bin/env python3
"""Check Markdown links, anchors and VI/EN documentation parity.

Runs in the meta CI job so a broken link or a missing translation fails the
build instead of shipping.  No third-party dependencies: the slug rules mirror
GitHub's heading anchors closely enough for this repository.
"""

from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]

DOCS = [
    "README.md",
    "README.en.md",
    "CHANGELOG.md",
    "CONTRIBUTING.md",
    "SECURITY.md",
]

HEADING = re.compile(r"^(#{1,6})\s+(.*)$")
INLINE = re.compile(r"`([^`]*)`")
LINK = re.compile(r"!?\[[^\]]*\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)")
HTML_HREF = re.compile(r'href="([^"]+)"')
HTML_IMG = re.compile(r"<img\s[^>]*>")
FENCE = re.compile(r"^```")


def slug(text: str) -> str:
    text = re.sub(r"<[^>]+>", "", text)
    text = INLINE.sub(r"\1", text)
    text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text)
    text = text.strip().lower()
    text = re.sub(r"[^\w\s-]", "", text, flags=re.UNICODE)
    return text.replace(" ", "-")


def headings(path: pathlib.Path) -> set[str]:
    found: set[str] = set()
    in_fence = False
    for line in path.read_text(encoding="utf-8").splitlines():
        if FENCE.match(line):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        match = HEADING.match(line)
        if match:
            found.add(slug(match.group(2)))
    return found


def markdown_files() -> list[pathlib.Path]:
    files = [ROOT / name for name in DOCS]
    files.extend(sorted((ROOT / "docs").glob("*.md")))
    return [f for f in files if f.exists()]


def main() -> int:
    problems: list[str] = []
    cache: dict[pathlib.Path, set[str]] = {}

    for path in markdown_files():
        text = path.read_text(encoding="utf-8")
        rel = path.relative_to(ROOT)

        targets = [m.group(1) for m in LINK.finditer(text)]
        targets += [m.group(1) for m in HTML_HREF.finditer(text)]

        for target in targets:
            if target.startswith(("http://", "https://", "mailto:")):
                continue

            file_part, _, anchor = target.partition("#")
            if not file_part:
                target_path = path
            else:
                target_path = (path.parent / file_part).resolve()
                if not target_path.exists():
                    problems.append(f"{rel}: missing link target {target}")
                    continue

            if anchor and target_path.suffix == ".md":
                cache.setdefault(target_path, headings(target_path))
                if anchor not in cache[target_path]:
                    problems.append(f"{rel}: missing anchor {target}")

        for img in HTML_IMG.finditer(text):
            if "alt=" not in img.group(0):
                problems.append(f"{rel}: <img> without alt text")

    for doc in sorted((ROOT / "docs").glob("*.md")):
        if doc.name.endswith(".en.md"):
            continue
        twin = doc.with_name(doc.stem + ".en.md")
        if not twin.exists():
            problems.append(f"{doc.relative_to(ROOT)}: missing English twin {twin.name}")

    for name in DOCS:
        if not (ROOT / name).exists():
            problems.append(f"missing required document {name}")

    if problems:
        for problem in problems:
            print(f"FAIL: {problem}", file=sys.stderr)
        return 1

    print(f"OK: links and anchors verified in {len(markdown_files())} documents")
    return 0


if __name__ == "__main__":
    sys.exit(main())
