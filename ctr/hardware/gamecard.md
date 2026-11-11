# CTR game cards

Retail 3DS games come on small cartridges ("game cards", internally **CTR cards**) that look almost identical to Nintendo DS cards. A small notch on the top-right corner stops them from fitting into older DS/DSi consoles. The 3DS also reads DS and DSi cards from the same slot.

A card holds the game's read-only data and, usually, a small chip for save data. Cards can also carry a block of system-update data, which is why a console can be updated from a game cartridge.

## Where it fits

The card slot is read by the [ARM9](cpu.md) card controller (the `CTRCARD`/`NTRCARD` blocks in the [I/O map](io-and-peripherals.md)). The ARM9 verifies and decrypts what it reads (the card's header uses the [NCSD container](../software/file-formats.md)) and the ARM11 receives it through the OS. Save data on the card is covered on the [savegame page](../software/savegames.md); games can alternatively be installed from the SD card ([SD filesystem](../software/sd-filesystem.md), [titles](../software/titles.md)).

## How it works

When a card is inserted the console first talks to it in the old DS style (8-byte, unencrypted commands). A special command (`0x3E`) switches the card into a 3DS-only mode with 16-byte commands and encryption; the card announces it is ready by clocking a single `0x01` byte after the data line stays at `0x00`.

Card varieties differ:

- **ROM-based:** a ROM chip (128 MB to 4 GB) plus a separate save chip (128 KB or 512 KB).
- **Flash-based:** the whole card (up to 2 GB) is a flash chip partitioned into game and save areas.

Because every game links the SDK, the ROM/RAM a game may use depends on the SDK revision.

## Technical details

### Physical interface

Same 17-pin layout as DS and DSi cards:

| Pin | Name | Description |
| --- | ---- | ----------- |
| 1, 17 | GND | Ground |
| 2 | CLK | Clock: 6.7 MHz and 4.2 MHz for DS/DSi cards; up to 16.6 MHz for 3DS cards |
| 3 | NC | Not connected (possibly used for programming) |
| 4 | RCS | ROM select, active low |
| 5 | RST | Reset, active low |
| 6 | ECS | Save chip select, active low |
| 7 | IRQ | Removal detection |
| 8 | VCC | 3.3 V supply |
| 9–16 | DAT0–DAT7 | Bidirectional data bus (8 bits for ROM; for the save chip they carry SPI/SIO signals) |

On a 3DS the card power is only enabled when the power supply bits of `CFG9_CARDSTATUS` are set to `10`; card insertion is readable from bit 0 of the same register. Time to first clock pulse is about 280 ms (3DS) versus 166 ms (DS/DSi).

### Save chip

Save-game SPI transfers use CPOL=1, CPHA=1. The only chip identified so far returns ID `0xC22211` (Macronix); it reports as an MX25L1021E-compatible part, but that chip does not support the 4-bit transfers the 3DS uses, so it is probably a custom variant.

### Command protocol

Starting commands (8-byte mode, plain): `0x9F` reset; `0x71C93FE9BB0A3B18` "act as a 3DS card"; `0x90` get card ID 1; `0xA0` get card ID 2; `0x3E` enter 16-byte mode. In 16-byte mode, `0x82` returns the 0x200-byte header ("InitialData", which includes an NCCH header at `0x100`) and enables encryption; further commands are encrypted with keys derived from the header and AES key slot `0x3B` (key X is set up by Boot9; key Y comes from the first 16 bytes of the header response). If the response to command `0xA0` ANDed with 3 equals 3, 3dbrew says slot `0x11` with an all-zero normal key is used instead. After every `0x200`-byte data block in 16-byte mode the card appends a 4-byte CRC32 of the unencrypted block (polynomial `0x82608EDB`, output XOR `0xFFFFFFFF`).

### Update region

Cards smaller than 1 GB include an update region found with `CART_SIZE_MAX − (0x280000 × (CART_SIZE_MAX ÷ CART_SIZE_128MB)) − 0x2000000`, which is `0x2000000` bytes long.

## Homebrew notes

DS-compatible flash cartridges were the first route to running unofficial code, and the 3DS's own first-generation flashcards are covered in [history](../homebrew/history.md). The boot ROM's DS-cartridge boot path is the basis of [ntrboot](../homebrew/boot9strap.md#ntrboot).

## Open questions

- Details of the card key derivation beyond the header are not reproduced here; see the 3dbrew *Gamecards* page.
- The origin of the `0x71C93FE9BB0A3B18` command constant is not explained in the source.

## Sources

- [3dbrew: Gamecards](https://www.3dbrew.org/wiki/Gamecards) — physical interface, protocol, sample commands, update region.
- [Rodrigo Copetti, Nintendo 3DS Architecture: Games](https://www.copetti.org/writings/consoles/nintendo-3ds/) — card variants, capacities, clocks, the DS-to-CTR mode switch.
