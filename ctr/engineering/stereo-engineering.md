# Stereo engineering: two eye images, depth and comfort

Stereoscopic 3D on the 3DS is two separate problems: drawing a left-eye and a right-eye image of the top screen without doubling every cost, and producing a picture that is comfortable to look at. This page covers both for Old 3DS homebrew: what the second eye costs, how to build the two cameras with citro3d and read the 3D slider, how to choose the depth plane, where HUD and sprite layers sit, and how to check comfort on a real console.

> **Disclaimer: models without 3D.** The 2DS family has no stereoscopic 3D at all (Nintendo 2DS, New Nintendo 2DS XL): the top panel is flat, so only the left-eye image exists there. Code on this page must work when `osGet3DSliderState()` is 0 and on those models ([display](../hardware/display.md), [models](../hardware/models.md)). **New 3DS:** adds head-tracked "Super Stable 3D" for the same two images ([display](../hardware/display.md#head-tracking-and-super-stable-3d-new-3ds)); everything below is unchanged by it, and nothing here was checked on a New 3DS.

## Before you start

- Checked against libctru v2.7.1 (`gfx.h`, `os.h`), citro3d v1.7.1 (`maths.h`, `mtx_perspstereo.c`, `mtx_perspstereotilt.c`, `base.c`, `renderqueue.c`) at the commits on [source versions](source-versions.md), and `3ds-examples` `graphics/gpu/composite_scene` and `stereoscopic_2d`.
- Applies to the Old 3DS. No stereo measurement has been made on this wiki: every cost statement below is either from source code, arithmetic, a `[reported]` third-party result, or a `[hypothesis]`.
- Terms: **[stereoscopic 3D](../../glossary.md#stereoscopic-3d)** is the top screen's two-image mode; **parallax** is the horizontal shift of an object between the two eye images; the **reference plane** (also called the screen plane or zero-parallax plane) is the depth at which an object has no shift and appears at the screen surface; **IOD** (interocular distance) is the separation between the two virtual cameras.
- Related pages: 2D layers in stereo are in [2D rendering](rendering-2d.md#stereoscopic-2d-layers) (not repeated here); render targets and VRAM in [GPU rendering](gpu-rendering.md#stereoscopic-3d-and-wide-mode); frame timing in [frame pacing](frame-pacing.md); the command-list side in [PICA200 command pipeline](pica200-command-pipeline.md). Scene-side reductions are in [3D scene optimisation](3d-scene-optimisation.md).

## What the second eye costs

Each eye needs its own colour and depth buffers and its own pass over the scene, because the projection differs. Per frame, the top screen goes from one render-target pass to two. The bottom screen is unaffected.

| Cost | Per eye? | Source of the statement |
| ---- | -------- | ----------------------- |
| Render-target memory (colour + depth, in VRAM) | Yes, a second target | Arithmetic in [GPU rendering](gpu-rendering.md#stereoscopic-3d-and-wide-mode) |
| Vertex shader work, rasterisation, fill, texture fetches | Yes | Follows from drawing the scene twice `[hypothesis]` on the exact factor; nothing measured here |
| Culling and visible-object list | No: compute once, reuse | Inference: the lists depend on the scene, not the eye (see below) `[hypothesis]` |
| Uniform and material set-up, vertex buffers | Mostly no | `[documented]` buffers are plain linear-memory data that both passes read; state is re-sent per pass by citro3d `[hypothesis]` on its size |
| Display transfer (colour buffer to LCD framebuffer) | Yes, one per eye target | `[documented: renderqueue.c]`: the top screen is swapped in stereo only when the right target was used |
| Animation, physics, game logic | No | Independent of rendering |

A realistic structure is one **update + cull** step followed by two **submit** steps:

```c
update_world();                       // once
build_visible_list(&cam_centre);      // once, with a slightly widened frustum (below)
C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
  draw_scene_pass(targetLeft,  -iod); // same list, same buffers
  if (iod > 0.0f)
    draw_scene_pass(targetRight, +iod);
C3D_FrameEnd(0);
```

Both eyes see slightly different volumes, so cull against a frustum that covers the union of the two (the centre camera with a marginally larger horizontal field of view, or both frusta combined). Culling against only the centre camera can drop an object at the screen edge that one eye still sees `[hypothesis]` on how much widening is needed; it depends on IOD and the nearest object.

### What shipped code and ports reported

> Conflict: coffeepot4.md describes the G-Diffuser result as an advanced stereo technique that "improved the frame rate from 40.2 to 44.6 FPS" in one Old 3DS menu comparison and says the developers "also documented smaller improvements". The page itself (below) calls that menu result clean but the race result only suggestive; the review's wording is accurate for the menu and omits the caveat for the race.

| Item | What the source states | Label |
| ---- | ---------------------- | ----- |
| Source | G-Diffuser native-renderer postmortem, [hypnotize.works/gdx-3ds/postmortem/native.html](https://hypnotize.works/gdx-3ds/postmortem/native.html), fetched 2026-11-11 (the fetch tool returned a summary, so exact wording was not checked line by line) | `[reported]` |
| Before | Stereo drew each draw twice, once per eye; the frame body was emitted once with a per-draw uniform carrying the eye shift | `[reported]` |
| Change | Both eyes in one GPU submission: the GPU runs the body, jumps to a right-eye prologue, then runs the body again, using the PICA200's second command-buffer channel | `[reported]` |
| Reason given | Each replayed list was its own GX queue entry with its own GSP submit and completion interrupt; a clean citro3d draw cost 10.9 microseconds and each backlogged queue entry added 0.4 to 0.5 ms of GPU dead time | `[reported]` |
| Results | Static 3D menu 40.2 to 44.6 fps (6 blocks against 9, plus or minus 0.2 fps), called "the clean result"; 1P 3D race 59.7 to 59.8 fps against a baseline of 56.4 / 59.3, called "suggestive" (two blocks per arm); a 16-machine 1P 3D race 53.7 fps; 3D multiplayer locked to 30 | `[reported]` |
| Model | Old 3DS for the numbers above; the New 3DS result was "neutral" | `[reported]` |

These numbers describe that renderer's workloads. They do not show that command replay helps any other renderer, and the technique bypasses citro3d's state tracking (see [PICA200 command pipeline](pica200-command-pipeline.md)). Treat it as a late optimisation after the simple reuse above is exhausted.

Other reported statements about stereo cost in the review (Capcom's Resident Evil: Revelations targeting 30 fps with 3D and no antialiasing in that mode; Shin'en adjusting Jett Rocket II's design because stereo raised rendering demands) were **not fetched for this page** and are not repeated as facts. They are secondary claims in coffeepot4.md; see the commercial case-study pages if they exist.

## Frame rate when the eye pass is added

citro3d with `C3D_FRAME_SYNCDRAW` waits for the previous frame's GPU work and then presents on vertical blanks ([frame pacing](frame-pacing.md)). If drawing both eyes pushes GPU time past one 60 Hz interval (16.67 ms), the frame misses its V-blank and the visible rate drops. With a fixed present cadence the available rates are the divisors of 60: 60, 30, 20 and so on `[documented: renderqueue.c, C3D_FrameRate accumulator]`. This is why 3D games commonly choose 30 fps up front, and why a game that hits 60 in 2D can fall to 30 when the slider goes up.

> Unknown: no measurement here shows the actual 2D versus 3D frame time ratio for any workload on an Old 3DS. The G-Diffuser account above reports a 3D race frame of about 19.8 ms against 16.5 ms in 2D for its renderer, which is far from a factor of two; that is that renderer's result and cannot be extrapolated. A benchmark is proposed under Open questions.

Two practical rules follow from the source:

1. **Skip the right eye at slider 0.** citro3d presents the top screen in stereo only when the right target was drawn that frame, so leaving it undrawn gives a 2D frame `[documented: renderqueue.c; composite_scene example]`. This halves top-screen work for players who play in 2D.
2. **Do not switch frame-rate policy by the slider unannounced.** Moving the slider from 0 to a small value changes the workload suddenly. If you lock at 30 for 3D, decide whether 2D stays at 60 and how the transition looks `[hypothesis]` on what players tolerate.

## Camera separation, convergence and the slider

### What the citro3d call actually does

`Mtx_PerspStereo` and `Mtx_PerspStereoTilt` take an `iod` and a `screen` ("focal length") argument. The header says: plug in `-iod` for the left eye and `iod` for the right, and objects further than `screen` appear inside the screen, nearer ones pop out `[documented: citro3d maths.h]`. The `composite_scene` example uses `iod = slider/3`, `screen = 2.0`, a 40 degree field of view and `C3D_AspectRatioTop` (400/240), and calls the tilted variant for each eye `[documented: composite_scene/source/main.c]`.

Reading `mtx_perspstereo.c` and `mtx_perspstereotilt.c`, the horizontal shear is `iod / (2 * screen)` divided by `tan(fov/2) * aspect`, and a constant `iod / 2` is added to the clip x. Working through the arithmetic (and checking it numerically for a right-handed matrix with a 40 degree field of view, aspect 400/240, `screen = 2.0`), the point where the two eye images coincide is at view-space distance

`D = screen * tan(fov / 2) * aspect`

for the arguments as passed, which is 1.213 units for the example's values, not 2.0. This comes from reading the matrix source and is `[derived by arithmetic]`, not run on hardware or checked against any documentation.

> Conflict: the header says `screen` is the focal length at which objects appear at the screen. The matrix source implies the zero-parallax distance is `screen * tan(fov/2) * aspect`. The two agree only when `tan(fov/2) * aspect` equals 1. Whether the header is loose or the matrix is meant that way is not established; the helper below sidesteps it by solving for the argument.

### A helper that sets convergence in world units

This computes the `screen` argument from the distance you want at zero parallax, scales IOD by the slider, and tells the caller when to skip the right eye. It compiles with devkitARM and citro3d; it has not been run.

```c
#include <3ds.h>
#include <citro3d.h>
#include <math.h>

/* Zero-parallax distance D = screen * tan(fov/2) * aspect (derived from
   mtx_perspstereotilt.c), so pass screen = D / (tan(fov/2) * aspect). */
static float StereoScreenArg(float convergenceDist, float fov, float aspect)
{
	return convergenceDist / (tanf(fov * 0.5f) * aspect);
}

/* eye: -1 left, +1 right. slider: osGet3DSliderState() (0.0 to 1.0).
   iodMax: separation at full slider, in your scene units.
   Returns false at slider 0 so the caller can skip the right-eye pass. */
static bool StereoProjection(C3D_Mtx *out, int eye, float slider, float iodMax,
                             float convergenceDist, float fov, float nearZ, float farZ)
{
	const float aspect = C3D_AspectRatioTop;
	float iod = slider * iodMax;
	float screen = StereoScreenArg(convergenceDist, fov, aspect);
	Mtx_PerspStereoTilt(out, fov, aspect, nearZ, farZ, eye * iod, screen, false);
	return iod > 0.0f;
}
```

Verification status: builds (devkitARM `arm-none-eabi-gcc` with `-Wall -Wextra -O2 -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft`, `-specs=3dsx.specs -lcitro3d -lctru -lm`, inside a minimal `main`, no warnings); emulator-tested: no; Old 3DS-tested: no; New 3DS-tested: no. Which sign puts a near object in front of or behind the screen was not checked on a 3D screen.

### The slider

`osGet3DSliderState()` returns a float from 0.0 to 1.0 read from shared memory `[documented: libctru os.h]`. Read it once per frame, before building the matrices, and use that one value for both eyes. Enable the stereo framebuffer with `gfxSet3D(true)` before creating the render targets; wide mode and stereo are mutually exclusive `[documented: libctru gfx.h]`. Create two top targets with `C3D_RenderTargetSetOutput(target, GFX_TOP, GFX_LEFT / GFX_RIGHT, flags)` as `composite_scene` does.

- Scale IOD linearly by the slider, as the example does (`iod = slider / 3`). Whether a linear map feels linear to players is untested `[hypothesis]`.
- Apply a small dead zone (treat values under about 0.02 as 0) so a slider resting at the bottom stop does not draw a wasted right eye for sensor noise `[hypothesis]` on the threshold; the slider's noise floor is not documented here.
- Keep the maximum IOD conservative. Nintendo's own account (below) is that large disparity made images look doubled when players moved the console.

## Choosing the depth plane

Parallax is zero at the reference plane, positive (behind the screen) beyond it and negative (in front) nearer. Three placements are in use, and the Nintendo account below describes the trade between two of them.

| Convergence at | Effect | Cost |
| -------------- | ------ | ---- |
| Far (background at screen) | Whole world pops out of the screen | Fatiguing; edge-of-screen objects get cut by the window frame ("window violation") `[hypothesis]` on this wiki's data |
| Near the player's focus object | The thing the player watches has no disparity so is least likely to blur or double | Everything in front of it pops out strongly, so keep foreground sparse |
| Middle of the playable depth range | Balanced | Less dramatic depth |

Practical method: pick the object the player looks at most (character, crosshair target), set `convergenceDist` to its distance, and move it with the camera each frame rather than fixing it. Smooth the change over several frames; a sudden jump of the reference plane is a visible pop `[hypothesis]`.

### Nintendo's accounts `[reported]`

These are interview statements. They describe design intent and tester feedback, not code. Paraphrased from the pages listed under Sources; no further generalisation is made.

**Super Mario 3D Land** (Iwata Asks, Vol. 1, page 4, nintendo.com UK mirror, fetched 2026-11-11; the interview dates from 2011). Hayashida says the console's 3D is good at showing deep worlds, which is how the game was first made, but images with large depth have a big difference between the two eyes, and if the player does not hold the console directly in front of them the images can look distorted. Action-game players move their bodies. Miyamoto observed that Mario Kart 7 does not blur, and the team concluded this is because the player's kart sits where there is no binocular parallax. They called that place the reference plane and adjusted it to Mario, so he is less likely to blur; this became the "Normal View". Late in development (about a month before the end, he says) testers responded about 80 percent that Normal View was more comfortable, while 10 to 20 percent preferred the earlier look, so the earlier view was kept as "Extended Depth", switchable with the +Control Pad. Motokura adds that in design, more data makes stereoscopy easier to produce but tires the eye more easily, and the team cut the game's density and used easy-to-grasp graphics. Earlier in development, making Mario the reference plane made things pop out too much; the team weakened the stereo strength and changed the camera. The game was also made playable with the slider off, for example with shadows placed under objects and on enemies, so depth cues exist without 3D.

Model and conditions: the page does not state the model beyond "Nintendo 3DS". The "80 percent" figure is an internal tester response, not a study.

> Unknown: coffeepot4.md attributes "reduced visual density and alternative depth settings" to the Iwata Asks page. The page supports both (density cut, Extended Depth as the alternative view); "alternative depth settings" is the interview's "Extended Depth", a second view, not a user-tunable depth range.

**Mario & Luigi: Dream Team** (Iwata Asks, "4. Memorable Experiences", nintendo.co.uk, fetched 2026-11-11). Kubota says the first wall they hit was stereoscopic backgrounds: previously the team could "fudge" the placement of things in 2D, but in stereoscopic vision that does not work. Iwata paraphrases it as the earlier shortcuts being off limits with stereo. The excerpt fetched does not mention specific techniques, number of layers or what the 2D shortcuts were. A search summary of another page of the same interview states the backgrounds were made in 3D while the characters are 2D pixel art, and that this made development longer; that page was not read in full here, so it is not relied on.

> Conflict: coffeepot4.md cites the Dream Team claim as "Page 4" with no URL, and describes it as visual shortcuts that "could break when stereoscopic depth revealed inconsistent spatial relationships". The fetched page supports the general point (2D fudging does not survive stereo) but not the detail about "inconsistent spatial relationships". Do not cite it as evidence for a specific technique.

Lesson that does follow from both accounts, labelled as inference: layers whose depth is inconsistent in 2D can be tolerated by the eye in 2D and stop working when each eye sees them at a different offset. Any 2D shortcut (a flat layer standing in for 3D scenery) needs a consistent depth in stereo.

## HUD, sprite layers and parallax offsets

2D layering in stereo (opposite per-eye offsets scaled by the slider, how `C2D_DrawImageAt` is used for each target, and the sign question) is covered in [2D rendering: stereoscopic 2D layers](rendering-2d.md#stereoscopic-2d-layers). Do not duplicate it; what this page adds is where to put each layer in a 3D scene.

| Element | Suggested parallax | Reasoning |
| ------- | ------------------ | --------- |
| HUD text and gauges | Zero, or a small fixed offset toward the viewer, constant for all slider values you have chosen | Text at screen depth is the sharpest; moving it with the slider makes it harder to read `[hypothesis]` |
| Crosshair / target reticle | Match the depth of the object under it | A reticle at a different depth from its target is hard to fuse `[hypothesis]` |
| Screen-space sprites over 3D (damage numbers, markers) | Place at the depth of the thing they annotate | Same reasoning |
| Distant 2D backdrops | Largest behind-the-screen offset | Matches far depth |
| Full-screen overlays and fades | Zero | They should not appear to float |

If HUD elements overlap near scene geometry that pops out of the screen, the HUD can appear behind something it is drawn over. Keep the foreground out of HUD regions or render the HUD at a nearer depth than any scene object, which costs comfort `[hypothesis]`.

Pixel-art cameras that snap to whole pixels need whole-pixel parallax too, or the shimmer reappears; rounding the offset per eye is a design choice to test on a console `[hypothesis]`.

## Per-eye render-target costs

Numbers for render-target memory are on [GPU rendering](gpu-rendering.md#stereoscopic-3d-and-wide-mode): a stereo top plus a bottom target is about 2.15 MB of the 6 MiB of VRAM, leaving room for textures; supersampled both eyes do not fit alongside a bottom target `[documented arithmetic]`. A second target also means a second clear, a second depth clear (see the early depth clear in [PICA200 command pipeline](pica200-command-pipeline.md)) and a second display transfer each frame. You can lower the cost by:

- using a 16-bit colour format for the target if the art tolerates it (halves the colour part),
- skipping the right-eye pass at slider 0,
- reducing or removing depth-buffer precision you do not need,
- not enabling antialiasing modes for stereo; a reported shipped example is Resident Evil: Revelations (see the unverified note above).

## How to validate comfort on an Old 3DS

A host build or emulator is not enough: parallax comfort depends on the real barrier panel, viewing distance and slider. There is no instrument for this here; the method is structured human testing on hardware, with the console in the hand and not on a stand ([testing on hardware](testing-on-hardware.md)).

1. Make a test scene with objects at known distances: a plane at the convergence distance, one at half and double, and a horizontal ruler across the screen. Check that the reference-plane object is a single sharp image at slider 100 percent.
2. Step the slider from 0 to 100 percent at several points. At 0 percent the image must be identical in both eyes and the scene must stay legible. Record where doubling begins for each tester and each distance.
3. Hold the console slightly off-axis (turned a few degrees, nearer and farther) as players do, and note where fusion breaks. Super Mario 3D Land's reported experience is that distortion appears when the console is not directly in front of the player.
4. Check edges: objects crossing the left and right screen borders at pop-out depth, and the HUD overlapping the nearest geometry.
5. Play for at least the length of a normal session at the slider settings you recommend and ask about eye strain. Record the console model, screen brightness and ambient light.
6. Record the frame rate and any dropped frames with the slider at 100 percent as well, because discomfort can come from timing as well as depth.
7. Repeat on a second console if available. **New 3DS:** Super Stable 3D changes how the image tracks the head; test separately and do not merge the results.

> Unknown: how many testers, what viewing positions and what scoring give a result that predicts player comfort. Nintendo's 80/20 split above shows that individuals differ, so provide a way for the player to lower or disable depth.

## Technical details

| Item | Value | Source |
| ---- | ----- | ------ |
| Slider range | 0.0 to 1.0, `osGet3DSliderState()` reads `OS_SharedConfig->slider_3d` | `[documented: libctru os.h]` |
| Enable stereo | `gfxSet3D(true)` before creating targets; default off; incompatible with wide mode | `[documented: libctru gfx.h]` |
| Eye selection | `GFX_LEFT` / `GFX_RIGHT` in `C3D_RenderTargetSetOutput` | `[documented: gfx.h, composite_scene]` |
| Right eye skipped | citro3d presents stereo only when the right target was drawn that frame | `[documented: renderqueue.c]` |
| Projection | `Mtx_PerspStereo(Tilt)(mtx, fovy, aspect, near, far, iod, screen, isLeftHanded)` | `[documented: citro3d maths.h]` |
| Zero-parallax distance | `screen * tan(fov/2) * aspect` for the arguments as passed | `[derived by arithmetic from mtx_perspstereo(tilt).c]` |
| Example constants | `iod = slider/3`, `screen = 2.0`, 40 degrees, aspect 400/240, near 0.01, far 1000 | `[documented: composite_scene]` |

## Pitfalls

- **Right eye drawn at slider 0**: wasted GPU time and, with a depth clear, extra work. Skip it.
- **Slider read twice per frame**: left and right eye got different IOD; read once.
- **Sign of `iod` per eye reversed**: scene appears inside out (near objects behind the screen). Confirm on a 3D screen with a near and a far object.
- **`screen` taken literally as the convergence distance**: per the arithmetic above the plane moves by `tan(fov/2) * aspect`; use the helper.
- **Culling against the centre camera only**: objects at the screen edge pop for one eye.
- **2D backdrop layers with inconsistent depth**: the Dream Team account suggests these break under stereo.

## Open questions

- Ratio of 3D to 2D frame time for a simple static-scene renderer on an Old 3DS. Experiment: render the same scene to one and two targets with the timing methods in [performance](performance.md), median and p99 over several hundred frames.
- Is the zero-parallax distance really `screen * tan(fov/2) * aspect`? Draw a flat plane at that distance on a 3D screen and see whether it shows no shift at slider 100 percent.
- Which sign of the per-eye offset pushes a 2D layer in front of or behind the screen plane (also open in [2D rendering](rendering-2d.md#stereoscopic-2d-layers)).
- Does culling once with a widened frustum save measurable CPU time against culling per eye?
- Cost of replaying the right-eye command list (the G-Diffuser technique) compared with plain two-pass submission in citro3d.
- Whether the reported Nintendo comfort findings reproduce for third-party art styles.

## Sources

- [citro3d](https://github.com/devkitPro/citro3d) — `include/c3d/maths.h`, `source/maths/mtx_perspstereo.c`, `mtx_perspstereotilt.c`, `renderqueue.c` at the pinned commit.
- [libctru](https://github.com/devkitPro/libctru) — `gfx.h` (3D mode, wide-mode exclusion), `os.h` (`osGet3DSliderState`).
- [3ds-examples](https://github.com/devkitPro/3ds-examples) — `graphics/gpu/composite_scene`, `stereoscopic_2d`.
- [Iwata Asks: Super Mario 3D Land, Vol. 1, 4. "It's So High I'm Scared!"](https://www.nintendo.com/en-gb/Iwata-Asks/Iwata-Asks-SUPER-MARIO-3D-LAND/Vol-1-SUPER-MARIO-3D-LAND/4-It-s-So-High-I-m-Scared-/4-It-s-So-High-I-m-Scared--216999.html) — reference plane, Normal View and Extended Depth, density, fetched 2026-11-11.
- [Iwata Asks: Mario & Luigi: Dream Team Bros., 4. Memorable Experiences](https://www.nintendo.co.uk/Iwata-Asks/Iwata-Asks-Mario-Luigi-Dream-Team-Bros-/Mario-Luigi-Dream-Team-Bros-/4-Memorable-Experiences/4-Memorable-Experiences-778733.html) — stereoscopic backgrounds and the end of 2D fudging, fetched 2026-11-11.
- [G-Diffuser: Road to 60, All Over Again](https://hypnotize.works/gdx-3ds/postmortem/native.html) — Old 3DS stereo submission results, `[reported]`.
