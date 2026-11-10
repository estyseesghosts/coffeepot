# AGENTS.md — coffeepot

Coffeepot is a wiki for research on Nintendo consoles: their hardware, their software, and homebrew development.

It serves two readers: a curious human, and a future coding agent doing hardware/software work on these consoles. Every page must work for both.

## Repository layout

```text
AGENTS.md
glossary.md                  # global jargon glossary
README.md                    # human landing page (what this wiki covers)
<console>/                   # one directory per console platform (e.g. ctr/, nx/)
  index.md                   # console overview + hub
  glossary.md                # per-device jargon glossary
  hardware/<topic>.md        # one page per hardware component/system
  software/<topic>.md        # one page per system software / format / service
  homebrew/<topic>.md        # one page per exploit / tool / library / workflow
```

Current scope is two platforms: `ctr/` (Nintendo 3DS family) and `nx/` (Nintendo Switch family). Add a new top-level platform directory only for a distinct hardware platform, using its established codename or short kebab-case name (e.g. `ctr`, `nx`).

File names are kebab-case: `arm9-cpu.md`, `nand-filesystem.md`, `boot9strap.md`. One topic per file.

## Core principles

1. **Don't repeat yourself. Link instead.** If something is already explained on another page, summarize it in at most 1–2 sentences and link to the canonical page. Never copy a full explanation into two places.
2. **Write like a human.** Natural, plain language first. Short paragraphs. Active voice. A motivated non-expert should be able to follow the Overview of any page.
3. **Minimal jargon, full technical detail.** Avoid jargon unless it names something precisely. When jargon is necessary, define it once, link it to a glossary, then use it freely. Never simplify away technical facts (registers, addresses, versions, file offsets, behaviors) to sound readable — push them into a `Technical details` section instead of dropping them.
4. **Progressive disclosure.** Plain-language summary up top, exhaustive detail below. Human reads the top; agent reads the whole page.

## Jargon and glossaries

Jargon means any term a non-expert would stumble on: acronyms (`SoC`, `IPC`), Nintendo-specific codenames (`CTR`, `Horizon`), homebrew terms (`CFW`, `ROP`), and hardware terms used in a precise sense (`physical address`, `kernel privilege`).

Rules:

- On first use in a page, give the plain meaning, then the term in bold with a glossary link. Example: `the main chip that combines CPU and GPU (a **system-on-a-chip**, or **[SoC](../../glossary.md#soc)**)`. After that, just use `SoC`.
- Every jargon term used on a page must resolve to exactly one glossary entry: either the global glossary or that console's glossary.
- **Global `glossary.md`**: terms with the same meaning across two or more consoles, or general computing terms (`CPU`, `GPU`, `kernel`, `firmware`, `NAND`, `ARM`). Define once here.
- **Per-device `<console>/glossary.md`**: terms specific to one console (`boot9strap`, `Horizon`, `PICA200`). Define here.
- If a per-device term starts being used by pages for a second console with the same meaning, promote it to the global glossary and leave a pointer link behind in the per-device glossary.
- Glossary entries are short: 1–4 sentences of plain meaning, plus the precise technical sense in one more sentence if needed, plus a link to the canonical wiki page for the full story. Never put the full explanation in the glossary; the glossary points at pages, pages point back at the glossary.

Glossary entry format:

```markdown
### Term name {#anchor-slug}

One to four sentences: what it is in plain language, then the precise sense. Ends with a link to the canonical page.

Example: ### System-on-a-chip (SoC) {#soc}
```

## Page types

Pick exactly one type per page. If a topic spans types (e.g. a chip and its SDK driver), split into linked pages rather than one blended page.

- `<console>/index.md` — console overview and hub. What the console is, when it was sold, what makes it distinctive, spec summary (as a table of links, not raw numbers repeated from subpages), software landscape, homebrew state. Links to every major subpage.
- `hardware/<topic>.md` — one hardware component or subsystem (CPU, GPU, memory map, display, input, cartridge, wireless). What it does, where it sits in the system, its observable behavior, then full specs.
- `software/<topic>.md` — one software thing (OS/kernel, system menu, applet, file format, filesystem, system service, update mechanism). What the user sees, what a developer needs to know, formats and behaviors.
- `homebrew/<topic>.md` — one exploit, custom firmware, tool, library, or workflow. What it achieves, who it is for, how it works, how to use it, limits and risks.

## Templates

Copy the matching template. Delete sections that genuinely don't apply, but never delete `Technical details` or `Sources` — if detail is unknown, say what is unknown.

### Console index

```markdown
# <Console common name>

Plain-language overview: what this console is, when it was sold, what makes it notable. 3–6 sentences.

## At a glance

| Area | Summary |
| ---- | ------- |
| CPU | [One-line plain summary](hardware/<page>.md) |
| GPU / video | [One-line plain summary](hardware/<page>.md) |
| Memory | [One-line plain summary](hardware/<page>.md) |
| Storage / media | [One-line plain summary](hardware/<page>.md) |
| Operating software | [One-line plain summary](software/<page>.md) |
| Homebrew status | 1–2 sentences + link to key [homebrew page](homebrew/<page>.md) |

Keep numbers out of this table unless they fit in one line. Full specs live on subpages.

## Hardware

Bulleted list of subpages with one plain sentence each: [Page](hardware/<page>.md) — what it covers.

## Software

Same: [Page](software/<page>.md) — what it covers.

## Homebrew

Same: [Page](homebrew/<page>.md) — what it achieves and who it is for.

## Notes and open questions

Anything uncertain, disputed, or TODO. Be explicit: what is unknown, what would answer it.

## Sources

- [Title](URL) — what this source covers
```

### Hardware page

```markdown
# <Component name>

Plain-language overview: what this part does and why it matters. 3–6 sentences. No unexplained acronyms here.

## Where it fits

How this component connects to the rest of the system (1 short paragraph + links to neighboring pages). Example: CPU reads from X, sends display lists to [GPU](gpu.md).

## How it works

Observable behavior in plain language: what software can make it do, what the user would notice. Link jargon to glossaries on first use.

## Technical details

Full detail for agent work: models, revisions, clock speeds, registers, addresses, memory maps, timing, quirks, revision differences. Use tables for registers/specs. Do not simplify here. If a value differs by model or firmware, give the table, not a single number.

## Homebrew notes

What a homebrew developer needs: how to access this hardware, what libraries/registers/services expose it, common pitfalls. 1–2 sentences + links if the full workflow lives on a [homebrew page](../homebrew/<page>.md).

## Sources

- [Title](URL) — what this source covers
```

### Software page

```markdown
# <Software / format / service name>

Plain-language overview: what the user sees, what it is for. 3–6 sentences.

## Behavior

What it does, when it runs, how it interacts with other software/hardware. Plain language, linked jargon.

## Technical details

Full detail: versions, file layouts, offsets, headers, cryptographic checks, IPC interfaces, error behavior, version differences. Tables over prose for formats.

## Homebrew notes

How homebrew interacts with this: patched, replaced, abused, or avoided — and links to the relevant [homebrew pages](../homebrew/<page>.md).

## Sources

- [Title](URL) — what this source covers
```

### Homebrew page

```markdown
# <Exploit / tool / library name>

Plain-language overview: what this achieves and who it is for (player, developer). 2–4 sentences. State the required console, firmware range, and outcome in the first paragraph.

## What you need

Console model(s), firmware version range, files, prior setup. Be exact; version ranges are technical facts, not trivia.

## How it works

High-level mechanism first (plain language, 1 paragraph), then the precise chain: entry point, escalation, persistence. Link each stage to the relevant hardware/software pages instead of re-explaining them.

## Technical details

Full detail: offsets, payloads, addresses, file hashes if relevant, failure modes, version-specific differences.

## Limits and risks

What it cannot do, what can brick or ban, what undoes it (updates, resets). Be blunt.

## Sources

- [Title](URL) — what this source covers (upstream repo, write-up, release notes)
```

## Writing guidelines

- **Structure every page the same way**: Overview → behavior/fit → Technical details → Homebrew notes (where relevant) → Sources. Readers and agents should always know where the precise facts are.
- **Define before use.** Never use an acronym or codename without its plain meaning + glossary link on first use per page.
- **Link aggressively.** At least: first mention of any wiki-covered topic links to its page; first mention of any jargon links to its glossary entry; spec tables link outward rather than duplicating numbers. Use relative links (`../software/os.md`, `../../glossary.md#term` from a `hardware/` or `software/` page).
- **Cite sources.** Every page ends with Sources. Prefer primary sources (hardware manuals, leaked SDK docs where legally usable, upstream repos, reputable console wikis with verification). Note what each source was used for.
- **State uncertainty explicitly.** Use `Notes and open questions` or inline `> Unknown:` callouts. Never present a guess as fact. Record the firmware/model/revision a fact was verified against where it matters.
- **Keep it readable.** Short paragraphs, one idea per paragraph, tables for specs, code blocks or hex tables for layouts. No walls of unexplained hex.
- **No placeholders.** Don't create a page that is only headings. If research is thin, write the Overview + what is known + open questions, and say so.

## Agent workflow

1. Before writing, search the wiki for existing coverage (`hardware/`, `software/`, `homebrew/`, both glossaries). If a canonical page exists, link to it — don't fork the explanation.
2. Pick the one correct page type and copy its template. One topic per file, correct directory, kebab-case name.
3. Draft Overview in plain language first, then fill Technical details completely. If you must choose, include the technical fact with a plain wrapper rather than omitting it.
4. On first jargon use, add the `**term** ([glossary](path#anchor))` link. If no glossary entry exists, add one in the right glossary (per-device first; promote to global only when the promotion rule above is met).
5. Cross-link both directions: new page links out to components/formats/tools it depends on; update the console `index.md` hub and any neighbor pages that should point back. Keep the hub lists to one sentence per link.
6. End with Sources. If you could not verify a technical claim, move it to open questions rather than stating it.
