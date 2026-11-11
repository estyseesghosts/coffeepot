# Commercial 3DS game case studies

Several studios that shipped 3DS games have said, in interviews and articles, how they made those games fit the console. This page collects those statements for five games and a few related accounts, and keeps three things apart: what the developer said, what this wiki infers from it, and what would have to be measured on hardware to confirm it. Nothing here is a measurement by this wiki. Every claim is `[reported]` by a third party, taken from press coverage rather than from engine source code or profiler traces.

## Before you start

- Applies to the Old 3DS first (see [model terms](../glossary.md#old-new-3ds)). The sources rarely say which model a statement was made on; where it matters the entry says `> Unknown:`. **New 3DS:** the model was released from 2014, so a game released earlier, such as Resident Evil: Revelations in January 2012, was written for the Old 3DS CPU and memory.
- Sources were fetched on 11 November 2026. Quotes are short phrases with attribution; everything else is paraphrase.
- Interviews reveal decisions and constraints, not complete renderers. Do not read an interview sentence as proof of an undocumented internal algorithm. For source-available engines and measured work see [porting](porting.md) and [performance](performance.md).
- Evidence labels follow [AGENTS.md](../../AGENTS.md#evidence-labels). "Stated" means the developer said it. "Inferred" is this wiki's reading and is a `[hypothesis]`.

## Summary

| Game | Stated by | Main stated decision | Transferable idea (inferred) |
| ---- | --------- | -------------------- | ---------------------------- |
| Retro City Rampage DX | Brian Provinciano, Vblank Entertainment | New renderer using less CPU, plus a shadow renderer using less CPU and RAM; a 2015 update redid the rendering code for steady 60 FPS | Replace the backend, keep the game |
| Shovel Knight | Nick Wozniak, Yacht Club Games | Careful VRAM use; flat images replace some complex 3D backgrounds | Cut cost in art production before runtime |
| Resident Evil: Revelations | Capcom staff via Impress Watch, Jun Takeuchi | MT Framework Mobile; 30 FPS in 3D; antialiasing off in 3D | Spend the budget per display mode |
| IronFall: Invasion | Fernando Velez, VD-Dev | 60 FPS target; assembly language "very helpful" | Gameplay and networking share the frame budget |
| Jett Rocket II | Manfred Linzner, Shin'en | Engine improved for 60 FPS; stereo was over budget | Treat stereo as a budget line from the start |

## Retro City Rampage DX (Vblank Entertainment)

**What was done.** Vblank ported its game to the 3DS and, per its developer, wrote a new renderer. A free update in early 2015 redid the rendering code again.

**Stated.**

- Brian Provinciano, Nintendo Life interview, 23 August 2013 (before the 3DS release): the 3DS version needed "a new renderer that used less CPU, and a new shadow renderer which used both less CPU and less RAM". The article does not name a 3DS model. `[reported: Nintendo Life, 23 Aug 2013]`
- Vblank's post, quoted by Nintendo Everything on 12 February 2015: the update "boasts a brand new renderer", the rendering code "has been completely redone", the old version "would occasionally dip below 60 fps", and the frame rate is now "rock solid, even on the original 3DS systems". It says the update helps both the original and the New 3DS, and runs natively on New 3DS to use the faster CPU. `[reported: Nintendo Everything, 12 Feb 2015]`
- Nintendo Life, 11 February 2015: "a stable 60 frames per second"; New 3DS gets smoother effects from its faster CPU. `[reported: Nintendo Life, 11 Feb 2015]`

**Inferred.** The stated fix is in the renderer's CPU and RAM cost, not in game logic, which fits the advice in [performance](performance.md) to find the dominant cost first. How much of the gain came from the renderer versus other changes in the patch is not stated. `[hypothesis]`

> Conflict: coffeepot4.md merges two statements into one. The shadow-renderer remark is from the 2013 interview, before release; no fetched article about the 2015 update mentions a shadow renderer. The "60 FPS on all models" wording is a Nintendo Life tag-page summary; Vblank's own post says "rock solid" on the original 3DS and says nothing about a fixed figure for every model. A Nintendo Everything summary line saying the game "will not remain consistent at 60" contradicts its own quoted Vblank post and looks like an error.

**Verify on hardware.** Swap the renderer behind a fixed game-state replay and measure CPU time per frame and the worst frame, not average FPS, on an Old 3DS ([performance](performance.md)).

## Shovel Knight (Yacht Club Games)

**What was done.** Some backgrounds that could have been 3D scenes were made flat images.

**Stated.** In a Digital Trends retrospective by Tomas Franzese (27 March 2023), Nick Wozniak says the team "had to be very careful about what was being loaded into the 3DS' VRAM", that the 3DS "is not as powerful as some of the other platforms", and that the flatter look of the Iron Whale vault background follows from this. He also says each game pixel mapped to one screen pixel. `[reported: Digital Trends, 27 Mar 2023]`

> Conflict: coffeepot4.md names the developer "Sandy Wozniak". The Digital Trends page names Nick Wozniak. This page follows the source.

> Unknown: no VRAM figure, model or profile is given. The console's VRAM size is on [memory](../hardware/memory.md); how much any background used is not stated.

**Inferred.** The decision was made in art production: a flat painted layer replaces geometry and its textures. `[hypothesis]` See [asset pipeline](asset-pipeline.md) and [GPU rendering](gpu-rendering.md).

**Verify on hardware.** Draw the same background as geometry and as one textured quad; compare VRAM use and per-frame CPU and GPU time on an Old 3DS.

## Resident Evil: Revelations (Capcom)

**What was done.** Capcom ran a portable version of its MT Framework engine on the 3DS.

**Stated.** Nintendo Life (Trevor Chan, 14 September 2010), reporting an Impress Watch interview with Capcom staff:

- Engine: MT Framework Mobile, based on MT Framework 2.0.
- Effects listed: HDR rendering, self-shadows, normal mapping, colour correction, depth of field and gamma correction; motion blur was in a test state.
- Jun Takeuchi: footage since E3 was rendered in real time on 3DS development hardware.
- 30 fps is held during 3D play; antialiasing is disabled when 3D is on, and the filter runs at double speed with 3D off.

`[reported: Nintendo Life, 14 Sep 2010]`. Siliconera (12 September 2010) lists HDR lighting, colour correction, self-shadowing, normal mapping, depth of field and motion blur for the "MT Framework build running on 3DS". `[reported: Siliconera, 12 Sep 2010]`

> Conflict: coffeepot4.md attributes "MT Framework Mobile", the 30 FPS stereo target and the antialiasing change to the Siliconera article. The fetched Siliconera article uses none of those details; they come from Nintendo Life.

> Unknown: coffeepot4.md also says enemy designs reduced the need for expensive textures and skeletal physics. This could not be verified: the Iwata Asks page failed with a certificate error and the Fandom "Making Of" page returned HTTP 402. It is not stated here as fact.

> Unknown: both reports predate the January 2012 release and describe development-time plans on development hardware. They name no console model and may differ from the shipped build.

**Inferred.** Lower antialiasing and 30 fps in 3D look like a per-mode trade: two views cost more, so image quality is reduced. `[hypothesis]` For stereo cost see [GPU rendering](gpu-rendering.md#stereoscopic-3d-and-wide-mode).

**Verify on hardware.** Measure frame time for a fixed homebrew scene with stereo on and off on an Old 3DS.

## IronFall: Invasion (VD-Dev)

**What was done.** A third-person shooter with many enemies, bullets and online multiplayer, targeting 60 FPS.

**Stated.** Nintendo Life interview (published 24 February 2015): Fernando Velez says the genre needs standard-level AI, "a lot of enemies" on screen, many bullets and better connections between players, that "Coding the game in Assembly was very helpful to circumvent some hurdles", and that multiplayer alone took almost a year. `[reported: Nintendo Life, 24 Feb 2015]` The interview mentions detecting a New Nintendo 3DS for controls but gives no per-model performance statement.

> Conflict: coffeepot4.md says Velez "described balancing" these systems within a 60 FPS frame budget. The fetched text lists them as challenges of the genre; it does not describe a frame budget or a balancing method, and does not say which routines were written in assembly.

**Reconciling existing wiki material.** The wiki already cites this game for something else: 3dbrew reads debug text in the *Ironfall: Invasion* binary as suggesting shader limits of 512 instructions and 128 operand descriptors. That is a single-game inference from a binary, not a developer statement, and is covered with its conflict in [PICA200 shader programming](pica200-shader-programming.md) and [GPU hardware](../hardware/gpu.md); it is not repeated here. Velez says nothing about shaders, so the two sources do not conflict. The game also appears as the `ironhax` target in [exploits](../homebrew/exploits.md). The retail-budget sentence in [performance](performance.md#the-budget) ("Commercial games treated these budgets as design constraints") is unsourced; this page is the sourced version, and it shows only that these studios cared about a frame target, not how they split it.

**Inferred.** A shooter with many actors has frame cost in gameplay and networking, not only rendering. Assembly use implies some hot loops on the [ARM11](../hardware/arm11-mpcore.md) were hand-written; which is unknown. `[hypothesis]`

**Verify on hardware.** Profile update, rendering and network time separately ([performance](performance.md)); compare a hand-written loop with compiler output using [CPU optimization](cpu-optimization.md).

## Jett Rocket II: The Wrath of Taikai (Shin'en Multimedia)

**What was done.** Shin'en improved its engine so a 3D platformer ran at 60 FPS, including with stereoscopic 3D.

**Stated.**

- Manfred Linzner, Nintendo Life interview (21 August 2013): "We feel 60 FPS is the only way" to get the precise controls wanted; "a lot of tiny changes" in older engine code and in how each critical asset was drawn "finally gave us what we were looking for"; with stereo enabled the game was "a few percent too much on the GPU and CPU". `[reported: Nintendo Life, 21 Aug 2013]`
- Linzner, SuperPhillip Central (23 April 2013): "While developing JR2 we also improved our engine a lot to make the game possible in 60fps". `[reported: SuperPhillip Central, 23 Apr 2013]`
- Nintendo Life review (22 November 2013): runs at 60 FPS with 3D on; the reviewer's observation, model not named. `[reported: Nintendo Life, 22 Nov 2013]`

> Conflict: coffeepot4.md says the team changed its game design because stereo increased rendering demands. Neither interview says so: the stated response to the stereo overrun was optimisation. The cited Gamereactor page is not an interview about this; the Gamereactor iDÉAME item (17 April 2013) only describes gameplay footage. The design-change claim is not supported by any fetched source.

> Unknown: no fetched source names the 3DS model.

**Inferred.** A "few percent" overrun suggests the second view was not free, but how much work is shared between eyes is not stated. `[hypothesis]`

**Verify on hardware.** Measure per-eye CPU and GPU time for a fixed scene and how much of the cost is common to both views ([GPU rendering](gpu-rendering.md#stereoscopic-3d-and-wide-mode)).

## Related indie accounts

The same Digital Trends article has more notes, all `[reported]`, none with numbers: Bertil Hörberg (Gunman Clive) says there was no middleware support and the fixed-function pipeline made him unsure the game would work; Voldi Way (WayForward) says Nintendo reminded the team some players cannot see stereoscopically, so 3D was made optional in Mighty Switch Force; Jools Watsham (Renegade Kid) says discrete 2D layers at different depths read clearly in stereo.

## Technical details

- Evidence type for the whole page: `[reported]` only. No frame-time, memory or draw-call figure here is a wiki measurement; the wiki has run no benchmark.
- Dates are the publication dates shown on the fetched pages.
- Unreachable on 11 November 2026: Iwata Asks (certificate error), Fandom "Making Of" (HTTP 402), Andriasang (connection refused). Claims depending on them are marked `> Unknown:`.
- The review text given to this task linked image-proxy URLs rather than the articles; the articles above were located by search and fetched directly.

## Pitfalls

- Treating "60 FPS" in a press quote as a measurement. It is a target or an impression unless a number and conditions are given.
- Carrying a result to a different workload or model.
- Trusting a summary of an interview over the interview; several summaries above overstated their sources.

## Open questions

| Question | What would settle it |
| -------- | -------------------- |
| Which stated approaches hold for homebrew on an Old 3DS? | Run the [benchmarks](../benchmarks/README.md) on an Old 3DS |
| Cost of a flat background versus geometry | A paired test with fixed content on an Old 3DS |
| Cost of stereo in a retained-geometry scene | Per-eye CPU and GPU timing on an Old 3DS |
| Did Capcom's enemy design reduce texture or physics cost? | The original interview text |

## Sources

- [Developer Interview: Brian Provinciano on Retro City Rampage for 3DS](https://www.nintendolife.com/news/2013/08/developer_interview_brian_provinciano_on_retro_city_rampage_for_3ds_wiiware_misunderstandings_and_wii_u) — renderer and shadow renderer quote (23 Aug 2013).
- [Retro City Rampage DX update fully detailed (Nintendo Everything)](https://nintendoeverything.com/retro-city-rampage-dx-update-fully-detailed-vblank-will-make-another-3ds-game/) — Vblank's update post (12 Feb 2015).
- [Retro City Rampage DX update arrives this week in Europe](http://www.nintendolife.com/news/2015/02/retro_city_rampage_dx_update_arrives_this_week_in_europe) — 60 fps and New 3DS notes (11 Feb 2015).
- [As the 3DS eShop closes, devs reflect (Digital Trends)](https://www.digitaltrends.com/gaming/3ds-eshop-retrospective-indie-creators/) — Shovel Knight VRAM and indie notes (27 Mar 2023).
- [Resident Evil: Revelations uses a version of Capcom's MT Framework](https://www.nintendolife.com/news/2010/09/resident_evil_revelations_uses_a_version_of_capcoms_mt_framework) — MT Framework Mobile, 30 fps in 3D, antialiasing (14 Sep 2010).
- [The 3DS's graphical fidelity detailed by Capcom (Siliconera)](https://www.siliconera.com/the-3dss-graphical-fidelity-detailed-by-capcom/) — effects list (12 Sep 2010).
- [Interview: IronFall: Invasion's developers](https://www.nintendolife.com/news/2015/02/interview_ironfall_invasions_developers_on_bringing_third-person_shooting_to_the_3ds_masses) — VD-Dev, 60 fps, assembly (24 Feb 2015).
- [Developer Interview: Shin'en Multimedia on Jett Rocket II](https://www.nintendolife.com/news/2013/08/developer_interview_shinen_multimedia_talks_about_jett_rocket_ii_new_modes_and_sticking_with_nintendo) — 60 fps, stereo overrun (21 Aug 2013).
- [SPC Interviews: Manfred Linzner](https://www.superphillipcentral.com/2013/04/spc-interviews-manfred-linzner-shinen.html) — engine improved for 60 fps (23 Apr 2013).
- [Jett Rocket II review (Nintendo Life)](https://www.nintendolife.com/reviews/3ds-eshop/jett_rocket_ii_the_wrath_of_taikai) — 60 fps with 3D on (22 Nov 2013).
- Wiki pages: [porting](porting.md), [performance](performance.md), [GPU rendering](gpu-rendering.md), [PICA200 shader programming](pica200-shader-programming.md).
