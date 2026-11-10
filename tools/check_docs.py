#!/usr/bin/env python3
"""Check the coffeepot wiki for broken structure.

Checks, for every Markdown page outside private_please_gitignore/:
  * relative links point at files that exist;
  * #anchors in relative links exist in the target page (GitHub heading
    slugs, or explicit {#anchor} ids as used by the glossaries);
  * glossary links point at an existing glossary entry;
  * wiki pages in console directories (except glossaries) end
    with a "## Sources" section, as AGENTS.md requires.

Code blocks and inline code spans are ignored. Exits with status 1 if any
problem is found. Usage: python3 tools/check_docs.py [repo-root]
"""
import os
import re
import sys

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), ".."))
SKIP_DIRS = {".git", "private_please_gitignore", "build"}
CONSOLE_DIRS = {"ctr", "nx"}


def strip_code(text):
    text = re.sub(r"^```.*?^```", "", text, flags=re.S | re.M)
    return re.sub(r"`[^`\n]*`", "", text)


def slug(heading):
    m = re.search(r"\{#([^}]+)\}\s*$", heading)
    if m:
        return m.group(1)
    h = heading.strip().lower()
    h = re.sub(r"[^\w\- ]", "", h)
    return h.replace(" ", "-")


def markdown_files():
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for name in filenames:
            if name.endswith(".md"):
                yield os.path.join(dirpath, name)


def anchors_of(path, cache={}):
    if path not in cache:
        with open(path, encoding="utf-8") as f:
            text = re.sub(r"^```.*?^```", "", f.read(), flags=re.S | re.M)
        found = set()
        counts = {}
        for m in re.finditer(r"^#+\s+(.*)$", text, re.M):
            s = slug(m.group(1))
            n = counts.get(s, 0)
            found.add(s if n == 0 else f"{s}-{n}")
            counts[s] = n + 1
        cache[path] = found
    return cache[path]


def main():
    problems = []
    pages = sorted(markdown_files())
    for page in pages:
        rel = os.path.relpath(page, ROOT)
        with open(page, encoding="utf-8") as f:
            raw = f.read()
        text = strip_code(raw)

        for m in re.finditer(r"\]\(([^)\s]+)\)", text):
            url = m.group(1)
            if re.match(r"^[a-z]+:", url):
                continue
            target, _, frag = url.partition("#")
            dest = os.path.normpath(os.path.join(os.path.dirname(page), target)) if target else page
            if not os.path.exists(dest):
                problems.append(f"{rel}: missing link target {url}")
                continue
            if frag and dest.endswith(".md") and frag not in anchors_of(dest):
                problems.append(f"{rel}: missing anchor {url}")

        top = rel.split(os.sep)[0]
        if top in CONSOLE_DIRS and os.path.basename(page) != "glossary.md" and "## Sources" not in raw:
            problems.append(f"{rel}: no '## Sources' section")

    for p in problems:
        print(p)
    print(f"checked {len(pages)} pages, {len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
