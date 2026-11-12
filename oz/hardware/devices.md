# Display, input and storage

The handheld screen, detachable controllers and internal storage define the physical interface a game sees. Their consumer specifications do not by themselves describe the APIs or permissions available to a homebrew process. The controllers have their own page; see [controllers](controllers.md).

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

### The internal storage layout

[Reverse-engineered] Switchbrew's flash filesystem research documents a four-LUN UFS layout. The boot chain lives in LUN0, and LUN1 holds a redundant copy, presumably for the update process:

| Offset | Size | Contents |
| ------ | ---- | -------- |
| `0x0` | `0x2000` | BRBCT |
| `0x2000` | `0x1C000` | Reserved |
| `0x1E000` | `0x2000` | MB1 BCH |
| `0x20000` | Variable | MB1 |
| `0x9E000` | `0x2000` | PSC-BL BCH |
| `0xA0000` | Variable | PSC-BL |
| `0xC0000` | `0x2000` | MB1-BCT BCH |
| `0xC2000` | Variable | MB1-BCT |
| `0x103000` | `0x1000` | BootConfig |
| `0x104000` | Variable | Package2 |

LUN2 and LUN3 are not documented.

Do not treat this partition layout as an application filesystem API. Game content and save data have different ownership and lifetimes, and the boot chain above it is covered on the [firmware page](../software/firmware.md).

### Content formats that changed

[Reverse-engineered] Switch 2 also uses [NAX1](../glossary.md#nax) where the original Switch's removable-storage content used NAX0, and the newer `.nca.CONCAT` content-directory naming. The overall content-system concepts survive, but parsers and storage tooling need explicit version awareness.

This matters beyond parsing: the microSD Express support update described in the [homebrew page](../homebrew/state-and-workflow.md) changed a heap layout the first public exploit chain depended on, so a storage-side change had consequences well outside storage.

### Content and authorization

[Documented] Nintendo distinguishes three kinds of physical release:

| Release | What it holds |
| ------- | ------------- |
| Ordinary game card | Game data on the card; updates or additional downloads can still be required |
| Game-Key Card | An authorization key rather than the full game data; the game must still be downloaded, but the card remains necessary to launch it |
| Switch 2 Edition | A version enhanced for Switch 2; distribution can vary by release |

[Documented] The Game-Key Card flow is: insert the card, download the game, then play by inserting the card again as with any physical release. An internet connection is needed only for the first launch, after which the game starts offline, but the card must be in the console to run it at all. A Nintendo Account is not required to download the data, and the free space needed is stated on the card's packaging.

[Documented] Playing software from a Game-Key Card requires system software 20.1.1 or later. This is why 20.1.1 is the first version in the [firmware table](../software/firmware.md) with a dated release.

[Documented] Save data remains in internal storage in every case.

The Game-Key Card distinction is the interesting one for tooling: it separates the right to launch from the content itself, so a game card is not necessarily a copy of the game, and "extract the cartridge" does not have a single meaning on this console.

### GameChat and GameShare

[Documented] GameChat is a Switch 2 feature for voice chat, video chat, screen sharing and playing together with friends. A group of up to twelve can take part in a room, and up to four of them can share video or their screen at once. Video chat needs a compatible USB camera, sold separately as the Nintendo Switch 2 Camera; voice chat uses the console's built-in microphone. The C button on Joy-Con 2, the Pro Controller and the GameCube Controller opens the C menu that drives the whole feature, and the HOME menu is the fallback for controllers without one.

[Documented] GameShare also supports local play. Over the internet it uses GameChat and requires Switch 2 consoles at both ends; an original Switch can join a compatible Switch 2 host over local wireless.

[Documented] GameChat requires a Nintendo Switch Online membership and a persistent internet connection, with a connection test during setup. During the GameChat open-access period, from June 5, 2025 to March 31, 2026, it could be used without a membership.

These are platform features rather than APIs, but they matter to the systems they use: The underlying microphone, camera and screen-capture interfaces exposed to a game or homebrew runtime require separate verification. See the [graphics and system services page](../software/services.md).

## Homebrew notes

Use the runtime's input abstraction first, handle controller disconnects and test both orientations and modes. For raw controller experiments, see the [controllers page](controllers.md).

Use the runtime's save API where available and confirm commit behavior rather than treating internal flash as a writable directory. Where a tool extracts packaged content, check whether it obtained the content by asking the console's own filesystem services to load it or by defeating the package encryption on its own; the distinction determines what the tool can and cannot do. See the [project inventory](../homebrew/project-inventory.md) for the tools that do each.

## Sources

- [Nintendo technical specifications](https://www.nintendo.com/en-ca/gaming-systems/switch-2/tech-specs/) — display, storage, connections and audio.
- [SwitchBrew flash filesystem](https://switchbrew.org/wiki/Switch_2:_Flash_Filesystem) — reverse-engineered UFS layout and LUN structure.
- [SwitchBrew hardware](https://switchbrew.org/wiki/Switch_2:_Hardware) — board revisions and component identities.
- [Nintendo Game-Key Card overview](https://en-americas-support.nintendo.com/app/answers/detail/a_id/68415) — physical authorization against downloaded content, and the 20.1.1 firmware requirement.
- [Nintendo GameChat guide and FAQ](https://en-americas-support.nintendo.com/app/answers/detail/a_id/68390) — GameChat, and the GameShare rules for cross-generation play.
- [iFixit Switch 2 chip identification](https://www.ifixit.com/Teardown/Nintendo%2BSwitch%2B2%2BChip%2BID/188725) — first-hand board inspection.
