#!/usr/bin/env python3
"""Check the coffeepot wiki for broken structure.

Checks, for every Markdown page outside private_please_gitignore/:
  * relative links point at files that exist;
  * #anchors in relative links exist in the target page (GitHub heading
    slugs and explicit HTML <a name="..."> anchors);
  * glossary links point at an existing glossary entry;
  * wiki pages in console directories (except glossaries) end
    with a "## Sources" section, as AGENTS.md requires;
  * no page repeats a verification status that newer results have replaced
    (STALE_PHRASES below; add a phrase when a status changes, put
    <!-- stale-ok --> on a line that must keep an old phrase).

With --external, also fetches every http(s) link in the wiki and reports the
ones that do not resolve (404, 410, DNS failure). That needs the network and
can be flaky, so CI runs it as a separate, non-blocking job.

Code blocks and inline code spans are ignored. Exits with status 1 if any
problem is found. Usage: python3 tools/check_docs.py [--external] [repo-root]
"""
import os
import re
import sys

ARGS = [a for a in sys.argv[1:] if not a.startswith("--")]
EXTERNAL = "--external" in sys.argv[1:]
ROOT = os.path.abspath(ARGS[0] if ARGS else os.path.join(os.path.dirname(__file__), ".."))
SKIP_DIRS = {".git", "private_please_gitignore", "build"}
CONSOLE_DIRS = {"ctr", "nx", "oz"}


# Statements that were true until results arrived and are false now. A page that repeats one needs updating.
# (regex, why it is stale). Matched case-insensitively against each line outside code blocks.
STALE_PHRASES = [
    (r"contain no measurements made for this wiki", "the benchmark suite has Old 3DS and New 3DS results"),
    (r"(benchmarks?|programs?)[^.\n]{0,80}\bhave not yet been run on hardware", "the benchmarks have been run on hardware"),
    (r"no results yet from the benchmark suite", "the benchmark suite has results"),
    (r"\bno result exists\b", "benchmark results exist; name the experiment that has none instead"),
    (r"the project has not been run on a console or emulator", "the tile renderer has been run on consoles and in an emulator"),
    (r"the gpu is a separate chip", "the PICA200 is inside the same SoC as the CPU"),
    (r"\*\*results:\*\* not run on hardware", "the microbenchmarks have Old 3DS and New 3DS results"),
    (r"none has been run for this wiki, and no figure in it", "the benchmark programs that use these recipes have hardware results"),
    (r"pictures (were|are) not compared", "pictures were compared in an emulator; say that, and that hardware is owed"),
    (r"has \*\*not been run\*\* on a console or an emulator", "the first game's self-test ran in Eden; say what was and was not run"),
    (r"compile-check program", "the program is now examples/api-check, and only checks signatures"),
    (r"crash-safe sequence from", "the save design is now two checksummed slots; see storage-and-saves.md"),
    (r"(membench|flushbench|drawbench|icachebench)[^.\n]{0,100}\b(not run on hardware|no results recorded)", "these have Old 3DS and New 3DS results"),
]


def stale_problems(rel, text):
    out = []
    for n, line in enumerate(text.split("\n"), 1):
        if "stale-ok" in line:
            continue
        for pattern, why in STALE_PHRASES:
            if re.search(pattern, line, re.I):
                out.append(f"{rel}:{n}: stale status ({why}): {line.strip()[:100]}")
    return out


def external_problems(pages):
    """Fetch every http(s) link once. Returns problem strings for links that definitely do not resolve."""
    import urllib.error
    import urllib.request
    from concurrent.futures import ThreadPoolExecutor

    urls = {}
    for page in pages:
        with open(page, encoding="utf-8") as f:
            text = strip_code(f.read())
        for m in re.finditer(r"\]\((https?://[^)\s]+)\)", text):
            urls.setdefault(m.group(1).split("#")[0], os.path.relpath(page, ROOT))

    def probe(url):
        req = urllib.request.Request(url, headers={"User-Agent": "coffeepot-link-check"})
        try:
            with urllib.request.urlopen(req, timeout=20) as r:
                return url, r.status
        except urllib.error.HTTPError as e:
            return url, e.code
        except Exception as e:  # DNS, TLS, timeout
            return url, str(e)[:60]

    out = []
    with ThreadPoolExecutor(max_workers=8) as pool:
        for url, status in pool.map(probe, urls):
            if status in (404, 410) or isinstance(status, str):
                out.append(f"{urls[url]}: external link does not resolve ({status}): {url}")
    return out


def strip_code(text):
    text = re.sub(r"^```.*?^```", "", text, flags=re.S | re.M)
    return re.sub(r"`[^`\n]*`", "", text)


def slug(heading):
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
        found = set(re.findall(r"<a\s+name=[\"']([^\"']+)[\"']\s*></a>", text, re.I))
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

        for m in re.finditer(r"^#{1,6}\s+.*\{#[^}]+\}\s*$", text, re.M):
            problems.append(f"{rel}: unsupported heading anchor syntax: {m.group(0)}")

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

    for page in pages:
        rel = os.path.relpath(page, ROOT)
        if rel.split(os.sep)[0] in CONSOLE_DIRS and "hardware-runs" not in rel:
            with open(page, encoding="utf-8") as f:
                problems.extend(stale_problems(rel, strip_code(f.read())))
    if EXTERNAL:
        problems.extend(external_problems(pages))

    for p in problems:
        print(p)
    print(f"checked {len(pages)} pages, {len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
