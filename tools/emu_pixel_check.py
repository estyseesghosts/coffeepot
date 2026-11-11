#!/usr/bin/env python3
"""Run the tile-renderer pixel-equivalence variants in the Azahar emulator and compare their pictures.

Usage: python tools/emu_pixel_check.py <dir-with-3dsx-files> <output-dir>

The directory comes from tools/build_pixel_check.sh. Each variant is started in Azahar; the program freezes its
simulation at frame 300, writes both screens as raw RGB8 to the emulated SD card 6 frames later, and the harness
copies the files out and stops the emulator. Variants are compared with the reference named in pixel-check.txt.

This is an EMULATOR check. It shows that strategies draw the same picture in Azahar; it says nothing about
timing (emulator timings are never results) and is not a substitute for a hardware picture comparison.
Windows only (Azahar's default install and %APPDATA% layout).
"""
import json
import os
import shutil
import struct
import subprocess
import sys
import time
import zlib

AZAHAR = r"C:\Program Files\Azahar\azahar.exe"
SDMC = os.path.join(os.environ.get("APPDATA", ""), "Azahar", "sdmc")
TOP_BYTES, BOTTOM_BYTES = 240 * 400 * 3, 240 * 320 * 3


def sessions():
    root = os.path.join(SDMC, "coffeepot")
    if not os.path.isdir(root):
        return set()
    return {d for d in os.listdir(root) if d.startswith("s") and os.path.isdir(os.path.join(root, d))}


def page_offsets(csv_path):
    """The program records the buffer's offset inside its 4 KiB page: note,shot,<screen>,<w>,<h>,page_offset=<n>."""
    offs = {}
    try:
        for line in open(csv_path, encoding="utf-8"):
            if line.startswith("note,shot,"):
                p = line.strip().split(",")
                offs[p[2]] = int(p[5].split("=")[1])
    except OSError:
        pass
    return offs


def deskew(raw, page_off):
    """Azahar writes the RGB8 display-transfer output so that every 4 KiB page of the framebuffer holds whole
    3-byte pixels and one unused byte at its end: the pixel stream restarts at each page boundary. Rebuild the
    contiguous stream (zero padded to the original length) so pixels can be addressed normally. Found by
    inspecting the first dump (the byte phase of a flat colour changed every 4096 bytes); not seen on hardware."""
    out = bytearray()
    pos = 0
    first = (4096 - page_off) % 4096 or 4096
    for block in [first] + [4096] * (len(raw) // 4096 + 1):
        if pos >= len(raw):
            break
        chunk = raw[pos:pos + block]
        pos += block
        out += chunk[:len(chunk) - len(chunk) % 3]
    return bytes(out) + bytes(len(raw) - len(out))


def run_variant(path, name, timeout=90):
    """Start the emulator, wait for both screen dumps, stop it. Returns (top, bottom) bytes or None."""
    before = sessions()
    proc = subprocess.Popen([AZAHAR, path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t0 = time.time()
    result = None
    try:
        while time.time() - t0 < timeout:
            time.sleep(1.0)
            new = sorted(sessions() - before)
            if not new:
                continue
            d = os.path.join(SDMC, "coffeepot", new[-1])
            top, bot = (os.path.join(d, "shot-%s-%s.rgb" % (name, k)) for k in ("top", "bottom"))
            if os.path.exists(top) and os.path.exists(bot):
                time.sleep(1.0)  # let the writes finish
                if os.path.getsize(top) == TOP_BYTES and os.path.getsize(bot) == BOTTOM_BYTES:
                    offs = page_offsets(os.path.join(d, "tile-%s.csv" % name))
                    result = (deskew(open(top, "rb").read(), offs.get("top", 0)),
                              deskew(open(bot, "rb").read(), offs.get("bottom", 0)))
                    break
    finally:
        subprocess.run(["taskkill", "/PID", str(proc.pid), "/T", "/F"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        proc.wait()
    return result


def to_image(raw, w_img, h_img):
    """Framebuffer is column-major, each column bottom to top, bytes B,G,R. Returns rows of (r,g,b)."""
    rows = []
    for y in range(h_img):
        row = bytearray()
        for x in range(w_img):
            i = (x * h_img + (h_img - 1 - y)) * 3
            row += bytes((raw[i + 2], raw[i + 1], raw[i]))
        rows.append(bytes(row))
    return rows


def write_png(path, rows):
    h, w = len(rows), len(rows[0]) // 3
    raw = b"".join(b"\x00" + r for r in rows)

    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def compare(a, b):
    n = len(a)
    diff_px = 0
    max_d = 0
    xs, ys = [], []
    for i in range(0, n, 3):
        d = max(abs(a[i] - b[i]), abs(a[i + 1] - b[i + 1]), abs(a[i + 2] - b[i + 2]))
        if d:
            diff_px += 1
            max_d = max(max_d, d)
            col, row = divmod(i // 3, 240)  # column-major, 240 rows per column
            xs.append(col)
            ys.append(row)
    box = [min(xs), min(ys), max(xs), max(ys)] if xs else None
    return {"differing_pixels": diff_px, "max_channel_difference": max_d, "bounding_box_col_row": box}


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    spec = []
    for line in open(os.path.join(src, "pixel-check.txt"), encoding="utf-8"):
        line = line.strip()
        if line and not line.startswith("#"):
            name, _flags, ref = line.split("|")
            spec.append((name, ref))
    shots = {}
    for name, _ in spec:
        print("running", name, flush=True)
        r = run_variant(os.path.join(src, name + ".3dsx"), name)
        if r is None:
            print("  NO PICTURE (timeout)", flush=True)
            continue
        shots[name] = r
        # Only the top screen is turned into a picture: its layout (column-major, bottom to top, BGR) is verified by
        # eye. The bottom screen's dump is compared byte for byte but not decoded; its geometry in the emulator was not
        # worked out, so a PNG of it would be misleading.
        write_png(os.path.join(out, "%s-top.png" % name), to_image(r[0], 400, 240))
    results = {}
    for name, ref in spec:
        if name not in shots:
            results[name] = {"status": "no picture"}
            continue
        if ref == "-":
            results[name] = {"status": "reference"}
            continue
        if ref not in shots:
            results[name] = {"status": "reference missing"}
            continue
        entry = {"reference": ref}
        for k, idx in (("top", 0), ("bottom", 1)):
            entry[k] = compare(shots[ref][idx], shots[name][idx])
        entry["identical"] = all(entry[k]["differing_pixels"] == 0 for k in ("top", "bottom"))
        results[name] = entry
    # Negative control: two variants that must differ (16 px and 32 px tiles). If the harness called these identical it
    # would be comparing nothing.
    if "t16_direct" in shots and "t32_direct" in shots:
        results["_control_t16_vs_t32"] = {"top": compare(shots["t16_direct"][0], shots["t32_direct"][0]),
                                          "must_differ": True}
    with open(os.path.join(out, "results.json"), "w", encoding="utf-8") as f:
        json.dump(results, f, indent=2)
    for name, e in results.items():
        if name.startswith("_control"):
            n = e["top"]["differing_pixels"]
            print("%-14s %s (top: %d px differ)" % (name, "OK, detected" if n else "FAILED: control not detected", n))
        elif "identical" in e:
            print("%-14s vs %-12s %s  top=%d px (max %d)  bottom=%d px (max %d)" % (
                name, e["reference"], "IDENTICAL" if e["identical"] else "DIFFERS",
                e["top"]["differing_pixels"], e["top"]["max_channel_difference"],
                e["bottom"]["differing_pixels"], e["bottom"]["max_channel_difference"]))
        else:
            print("%-14s %s" % (name, e["status"]))


if __name__ == "__main__":
    main()
