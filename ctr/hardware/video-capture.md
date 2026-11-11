# CTR screen video capture points

Hobbyists have found the points on the 3DS motherboard where the picture for the **lower screen** can be tapped as a digital video signal, which is how video-capture devices for the 3DS work. This page records those test points and the one documented capture experiment. It is for people who solder to the board; ordinary software capture (screenshots, streaming from the console) is not covered here, and the sources say nothing about the **upper screen**.

The experiment was done on an Old 3DS (the source calls it an "EU-O3DS"); the sources do not say whether a New 3DS board has the same test points. Commercial capture products exist for both groups (see the links below).

## Where it fits

The test points expose 24 colour lines, a clock and two sync lines for the lower screen, which suggests a plain parallel RGB feed to the panel (the source does not describe the panel interface itself). They are reached through numbered pads on the front of the PCB, each traced to a pin on one of two connectors (CN1, CN2). The screens themselves, their resolutions and the 3D top panel are on the [display page](display.md); the board-level parts list is on [models](models.md).

## How it works

The video signal is an 8-bit-per-colour bus (24 bits) plus three control lines: a pixel clock, a horizontal sync and a vertical sync. A logic analyser or an FPGA samples the 24 colour lines on each clock edge and rebuilds the image; the sync lines mark line and frame starts. The documented capture used an FPGA development board and a signal-analyser tool, sampling at 50 MHz, and recorded about 2.6 ms of signal.

## Technical details

Test points on the front of the board, lower screen (3dbrew *Video Capture*, found by 3dbrew user Matyapiro31). "Pic #" is the number printed on the source's annotated photo; "CN" is the connector pin the signal arrives on.

| Bit | Red: Pic # / pin / TP | Green: Pic # / pin / TP | Blue: Pic # / pin / TP |
| --- | --------------------- | ----------------------- | ---------------------- |
| 0 | 10 / CN2-31 / 184 | 20 / CN2-39 / 186 | 24 / CN1-34 / 188 |
| 1 | 12 / CN2-32 / 178 | 19 / CN2-40 / 180 | 18 / CN1-33 / 182 |
| 2 | 13 / CN2-33 / 172 | 21 / CN1-40 / 174 | 17 / CN1-32 / 176 |
| 3 | 11 / CN2-34 / 166 | 14 / CN1-39 / 168 | 23 / CN1-31 / 170 |
| 4 | 2 / CN2-35 / 183 | 22 / CN1-38 / 185 | 5 / CN1-30 / 187 |
| 5 | 3 / CN2-36 / 177 | 16 / CN1-37 / 179 | 6 / CN1-29 / 181 |
| 6 | 1 / CN2-37 / 171 | 15 / CN1-36 / 173 | 7 / CN1-28 / 175 |
| 7 | 4 / CN2-38 / 165 | 9 / CN1-35 / 167 | 8 / CN1-27 / 169 |

Control signals:

| Pic # | Pin | TP | Signal |
| ----- | --- | -- | ------ |
| 25 | CN2-30 | 189 | CLK (pixel clock) |
| 26 | CN2-28 | 191 | VSYNC |
| 27 | CN2-27 | 190 | HSYNC |

Notes recorded by the source: test point 168 sits above 180 and 173 below 179 on the board (to tell them apart); bit 0 is the least significant bit of each colour; the example plots show the clock, HSYNC (TP190) and VSYNC (TP191) sampled at 50 MHz, a full plot of about 2.6 ms and a zoomed one of 1.28 microseconds mainly showing the clock.

Setup used: a DE10-Nano FPGA board with Intel Signal Tap, and "5 wires soldered to the TPs of an EU-O3DS (roughly 25 cm long, parallel wired)". The page is marked as a stub.

> Unknown: the source says five wires but lists 27 signals; how the full 24-bit capture was wired (or whether only some colour bits were captured in the experiment) is not stated. Capture files (VCD and CSV in a 7z archive) are linked on the source page and were not read.

> Unknown: pixel-clock frequency, line and frame timing, and the upper screen's test points are not documented in the source.

### Commercial and community capture devices (links on the source page)

DS and DSi capture boards (DSCapture, NisetroCaptureDS/DSi), 3DS and New 3DS products (NisetroCapture3DS, NisetroCaptureNew3DS, NisetroCapturenew3DSXL) and a USB and an HDMI device by "Loopy" are listed on 3dbrew with links to the sellers; their internals are not described.

## Homebrew notes

None of this is reachable from software; it is hardware modding. The screens and their framebuffers are described on the [display page](display.md).

## Open questions

- Which of the test points exist on each model, including the New 3DS.
- The upper screen's video lines and the timing of the lower screen signal.

## Sources

- [3dbrew: Video Capture](https://www.3dbrew.org/wiki/Video_Capture) — test point table, control signals, capture setup (stub page).
