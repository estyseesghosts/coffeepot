# Display, input and storage

The handheld screen, detachable controllers and internal storage define the physical interface a game sees. Their consumer specifications do not by themselves describe the APIs or permissions available to a homebrew process.

## Where it fits

Input and presentation are mediated by system software and the active runtime. Storage includes internal flash for system and user data and a removable microSD Express card; the process's filesystem access is a separate question.

## How it works

[Documented] Nintendo specifies a 7.9-inch 1920 × 1080 LCD with HDR10 and variable refresh up to 120 Hz. TV output supports up to 3840 × 2160 at 60 Hz or 1920 × 1080/2560 × 1440 at up to 120 Hz, subject to the display and mode. These are output capabilities, not a guarantee that a game renders internally at those resolutions or rates.

## Technical details

| Device | Details |
| ------ | ------- |
| Internal storage | [Documented] 256 GB UFS, part reserved by the system. |
| Removable storage | [Documented] microSD Express; ordinary microSD cards are not a substitute for game storage. |
| Input | Joy-Con 2 supports conventional controls and mouse use. Availability of mouse data depends on the software interface and controller state. |
| External connections | Two USB-C ports; the dock supplies HDMI and wired LAN. |
| Audio | Built-in microphone and speakers; Nintendo specifies 5.1-channel linear PCM over HDMI. |

[Reverse-engineered] SwitchBrew describes four UFS logical units and boot-related partitions. Do not assume a partition layout is a stable application filesystem API. Game content and save data have different ownership and lifetimes.

## Homebrew notes

Use the runtime's input abstraction first, handle controller disconnects and test both orientations and modes. For raw controller experiments, ndeadly's research documents report IDs and protocol details; USB and Bluetooth transport and initialization must be considered separately. Use the runtime's save API where available and confirm commit behavior rather than treating internal flash as a writable directory.

## Sources

- [Nintendo technical specifications](https://www.nintendo.com/en-ca/gaming-systems/switch-2/tech-specs/) — display, storage, connections and audio.
- [SwitchBrew flash filesystem](https://switchbrew.org/wiki/Switch_2:_Flash_Filesystem) — reverse-engineered UFS layout.
- [Switch 2 controller research](https://github.com/ndeadly/switch2_controller_research) — controller protocols and captures.
