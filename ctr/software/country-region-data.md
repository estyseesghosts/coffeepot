# CTR country and region data archive

Title `0004009B00010402` is a **shared data archive** [title](titles.md) (the `0004009B` category holds data-only system titles). The research note this page is based on shows that it contains LZ-compressed tables of countries and regions, with names, sort order and map coordinates. The note is short and speculative: most fields are marked with a question mark, and nothing in the sources says which programs read the archive.

## Behavior

Not documented in the sources used. By the shape of the data (country and region IDs, names, a sort index and latitude/longitude) it appears to back the console's country/region selection, but that is an inference and is not confirmed.

## Technical details

The files are named `country_LZ.bin` and `$(country_id)_LZ.bin` and are LZ-compressed (the note does not name the LZ variant; compare the LZ-compressed `.ARC` archives in [file formats](file-formats.md)). Layouts after decompression:

### `country_LZ.bin`

```c
struct {
    struct {
        u32 entry_count;
        struct {                      // size = 0x83C
            struct { u8 zero?; u8 zero?; u8 zero?; u8 country_id; } address_id;
            u32 region_count?;        // maybe max_region_id
            u32 unknown?;             // maybe a bool
            struct { char16_t country_name[0x40]; } country_names[0x10];   // one per language
            u8  sort_index?[0x10];
            u8  zero_padding[0x20];
        } country_entries[entry_count];
    } sections?[2];
    u8 unknown?[entry_count_0][0x10];
};
```

### `<country_id>_LZ.bin`

```c
struct {
    u32 entry_count?;
    struct {                          // size = 0x818
        struct { u8 zero?; u8 zero?; u8 region_id; u8 country_id; } address_id;
        struct { char16_t region_name[0x40]; } region_names[0x10];   // one per language
        u8  sort_index?[0x10];
        u16 latitude;                 // 0x0000–0x4000 -> +0..+90; 0xC000–0xFFFF -> -90..-0
        u16 longitude;                // 0x0000–0x8000 -> +0..+180; 0x8000–0xFFFF -> -180..-0
    } regions[1 + entry_count];
};
```

The names appear in sixteen fixed slots per record; the note does not say what the slots index (languages are a guess). Coordinates are 16-bit fractions of the half-range: latitude 0x4000 = 90°, longitude 0x8000 = 180°.

## Homebrew notes

None known.

## Open questions

- Nearly every field with a trailing `?` is unverified (entry counts, flags, the use of the trailing 16-byte rows).
- The origin of the source note is not recorded; the file was supplied without a URL.

## Sources

- Research note `3ds-0004009B00010402.txt` (provided with the wiki's private source set, author and origin not recorded) — the structures above. No live link available.
- [3dbrew: Title list](https://www.3dbrew.org/wiki/Title_list) — `0004009B` shared data archive category.
