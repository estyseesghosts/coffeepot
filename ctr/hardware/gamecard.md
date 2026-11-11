# CTR game cards

Retail 3DS games come on small cartridges ("game cards", internally **CTR cards**) that look almost identical to Nintendo DS cards. A small notch on the top-right corner stops them from fitting into older DS/DSi consoles; 3dbrew adds that if the case is modified so a 3DS card fits, a DS or DSi refuses to detect it and shows no banner icon. The 3DS also reads DS and DSi cards from the same slot. The same card slot and card format apply to all models; the sources do not mention a New 3DS difference.

A card holds the game's read-only data and, usually, a small chip for save data. Cards can also carry a block of system-update data, which is why a console can be updated from a game cartridge.

## Where it fits

The card slot is read by the [ARM9](cpu.md) card controller (the `CTRCARD`/`NTRCARD` blocks in the [I/O map](io-and-peripherals.md)). The ARM9 verifies and decrypts what it reads (the card's header uses the [NCSD container](../software/file-formats.md)) and the ARM11 receives it through the OS. Save data on the card is covered on the [savegame page](../software/savegames.md); games can alternatively be installed from the SD card ([SD filesystem](../software/sd-filesystem.md), [titles](../software/titles.md)).

## How it works

When a card is inserted the console first talks to it in the old DS style (8-byte, unencrypted commands). A special command (`0x3E`) switches the card into a 3DS-only mode with 16-byte commands and encryption; the card announces it is ready by clocking a single `0x01` byte after the data line stays at `0x00`.

Card varieties differ:

- **ROM-based:** a ROM chip (128 MB to 4 GB) plus a separate save chip (128 KB or 512 KB).
- **Flash-based:** the whole card (up to 2 GB) is a flash chip partitioned into game and save areas.

Because every game links the SDK, the ROM/RAM a game may use depends on the SDK revision. Internally the ROM chip sits on an 8-bit data bus and the save memory on a serial bus, both clocked at up to 16.6 MHz (Copetti).

Development hardware uses two developer flash card types (`CTR-005(-01)`, shaped like a retail card, and the large `CTR-008` with replaceable save flash); see [models](models.md#model-identifiers).

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
| 9–16 | DAT0–DAT7 | Bidirectional data bus; meaning depends on which chip is selected (below) |

CLK frequencies apply to both save-SPI and ROM transfers. Pin signals by selected bus:

| Pin | Name | ROM bus (selected by RCS, pulled low to start a ROM transfer) | Save bus (selected by ECS, pulled low to start a save SPI transfer) |
| --- | ---- | ------------------------------------------------------------- | ------------------------------------------------------------------- |
| 9–12 | DAT0–DAT3 | Bidirectional data | not connected |
| 13 | DAT4 | Bidirectional data | NC / SIO3 |
| 14 | DAT5 | Bidirectional data | WP# / SIO2 |
| 15 | DAT6 | Bidirectional data | SO / SIO1 |
| 16 | DAT7 | Bidirectional data | SI / SIO0 |

| Property | 3DS | DS and DSi |
| -------- | --- | ---------- |
| VCC | Only enabled when the power-supply bits of `CFG9_CARDSTATUS` are set to `10` | Always available when a card is detected |
| Card detect | Physical insertion switch, readable through bit 0 of `CFG9_CARDSTATUS` | IRQ pin |
| Time to first clock pulse | about 280 ms | about 166 ms |

### Save chip

Save-game SPI transfers use CPOL=1, CPHA=1. The only chip identified so far returns ID `0xC22211` (Macronix); it reports as an MX25L1021E-compatible part, but that chip does not support the 4-bit transfers the 3DS uses, so it is probably a custom variant.

### Command protocol

Starting commands (8-byte mode, plain): `0x9F` reset; `0x71C93FE9BB0A3B18` "act as a 3DS card"; `0x90` get card ID 1; `0xA0` get card ID 2; `0x3E` enter 16-byte mode. In 16-byte mode, `0x82` returns the 0x200-byte header ("InitialData", which includes an NCCH header at `0x100`) and enables encryption; further commands are encrypted, as described next. After every `0x200`-byte data block in 16-byte mode the card appends a 4-byte CRC32 of the unencrypted block (polynomial `0x82608EDB`, output XOR `0xFFFFFFFF`).

### Encryption after command `0x82`

3dbrew gives this algorithm (big-endian byte order unless noted). The 0x200-byte response to `0x82` ("InitialData") starts with `0x10` bytes of key Y, then a 16-byte encrypted seed, a 16-byte tag and a 12-byte nonce; its bytes `0x100`-`0x1FF` hold an NCCH header (the `NCCH` magic is visible in the sample).

1. Set AES key slot `0x3B` key Y to bytes `0x000`-`0x00F`. The matching key X is set up by Boot9.
2. Decrypt bytes `0x010`-`0x01F` with AES-128-CCM in slot `0x3B` using the 12-byte nonce at `0x030`-`0x03B` to get the **primary seed**, and verify the 16-byte tag at `0x020`-`0x02F`. If the response to command `0xA0` ANDed with `3` equals `3`, slot `0x11` with an all-zero normal key is used instead.
3. Split the seed into left and right halves.
4. Initialise a **SNOW 2.0** stream cipher with the seed as the 128-bit key and a 128-bit static value (which depends on the card's ID2) as the IV. Run it 32 times (32 words, 1024 bits) and discard the output.
5. Initialise an **RC4** cipher with a 256-bit key: a 128-bit static value (depending on ID2) followed by four outputs of the SNOW stream. Run RC4 256 times and discard 256 bytes.
6. From now on all commands and responses are RC4-encrypted; the card controller and the card share the RC4 state and advance it together.

Command `0x83` **re-keys**: a new SNOW 2.0 context is keyed with the seed's left half followed by the low 64 bytes of the decrypted `0x83` command (as written on 3dbrew), the same IV, then steps 4 and 5 repeat with the new stream.

The static values are fixed inside the card controller and the cards; they do not come from Process9 or anywhere in `NATIVE_FIRM`, so a [security-architecture](../software/security-architecture.md) analysis cannot read them from firmware.

### Sample command sequence (3dbrew)

Observed for *LEGO Star Wars III: The Clone Wars* (EUR), title ID `0004000000038c00`. Sizes are in bytes of returned data.

| Size | Command (decrypted) | Meaning |
| ---- | ------------------- | ------- |
| 0x2000 | `9F00000000000000` | Reset |
| 0 | `71C93FE9BB0A3B18` | Signal that the card should act as a 3DS card |
| 4 | `9000000000000000` (sent twice) | Get card ID 1; response `9000FEC2` |
| 4 | `A000000000000000` | Get card ID 2; response `00000000` |
| 0 | `3E00000000000000` | Enter 16-byte command mode |
| 0x200 | `82000000000000000000000000000000` | Get header and enable encryption |
| 0 | `8300000000000000708DF1A731717D0B` | Seed (re-key) |
| 4 | `A2000000...708DF1A731717D0B` | Get card ID 1 (encrypted); `9000FEC2` |
| 4 | `A3000000...` | Get card ID 2 (encrypted) |
| 0 | `C5000000...` | Refresh card NAND |
| 0x200 | `BF00000000000000...` (the address is inside the command) | Read card at address 0, then at `0x4000` |

(Encrypted forms are in the source and depend on the RC4 state.) In 16-byte mode the data bus stays `0x00` until the card clocks out `0x01`, then the data follows.

### Update region

Cards smaller than 1 GB include an update region found with `CART_SIZE_MAX − (0x280000 × (CART_SIZE_MAX ÷ CART_SIZE_128MB)) − 0x2000000`, which is `0x2000000` bytes long.

## Homebrew notes

DS-compatible flash cartridges were the first route to running unofficial code, and the 3DS's own first-generation flashcards are covered in [history](../homebrew/history.md). The boot ROM's DS-cartridge boot path is the basis of [ntrboot](../homebrew/boot9strap.md#ntrboot).

## Open questions

- The static values that depend on the card's ID2 are not public in the sources used.
- The origin of the `0x71C93FE9BB0A3B18` command constant is not explained in the source.

## Sources

- [3dbrew: Gamecards](https://www.3dbrew.org/wiki/Gamecards) — physical interface, protocol, sample commands, update region.
- [Rodrigo Copetti, Nintendo 3DS Architecture: Games](https://www.copetti.org/writings/consoles/nintendo-3ds/) — card variants, capacities, clocks, the DS-to-CTR mode switch.
