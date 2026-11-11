#!/usr/bin/env python3
"""Smoke-test a benchmark .3dsx in the Azahar emulator: does it run to the end without reporting errors?

Usage: python tools/emu_smoke.py <file.3dsx> <timeout-seconds> <csv-name> [<csv-name> ...]

Starts the program, waits until every named CSV (for example membench.csv or icachebench-sp1.csv) exists in the new
session directory on the emulated SD card and its last line begins with "note,done", then stops the emulator and
prints each file's error count. A timeout, a missing file, or an `error,` line is a failure (exit status 1).

This is an EMULATOR smoke test: it shows the program starts, finishes and writes its log. Emulator timings are
never results, and an emulator that stubs a service (Azahar does not implement the New 3DS speed-up call, for
example) can hide a problem that real hardware would show. Windows only (Azahar's default install and %APPDATA%).
"""
import os
import subprocess
import sys
import time

AZAHAR = r"C:\Program Files\Azahar\azahar.exe"
SDMC = os.path.join(os.environ.get("APPDATA", ""), "Azahar", "sdmc", "coffeepot")


def sessions():
    if not os.path.isdir(SDMC):
        return set()
    return {d for d in os.listdir(SDMC) if d.startswith("s") and os.path.isdir(os.path.join(SDMC, d))}


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    prog, timeout, names = sys.argv[1], float(sys.argv[2]), sys.argv[3:]
    before = sessions()
    proc = subprocess.Popen([AZAHAR, prog], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t0 = time.time()
    state = {}
    try:
        while time.time() - t0 < timeout:
            time.sleep(2.0)
            new = sorted(sessions() - before)
            if not new:
                continue
            d = os.path.join(SDMC, new[-1])
            state = {}
            for n in names:
                path = os.path.join(d, n)
                if not os.path.exists(path):
                    continue
                lines = [l.strip() for l in open(path, encoding="utf-8", errors="replace") if l.strip()]
                state[n] = (lines[-1].startswith("note,done") if lines else False,
                            sum(1 for l in lines if l.startswith("error,")),
                            lines[-1][:70] if lines else "")
            if len(state) == len(names) and all(v[0] for v in state.values()):
                break
    finally:
        subprocess.run(["taskkill", "/PID", str(proc.pid), "/T", "/F"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        proc.wait()
    ok = True
    for n in names:
        if n not in state:
            print("%-28s MISSING" % n)
            ok = False
            continue
        finished, errors, last = state[n]
        status = "ok" if finished and errors == 0 else "FAIL"
        ok = ok and status == "ok"
        print("%-28s %-4s finished=%s error_lines=%d  last: %s" % (n, status, finished, errors, last))
    print("seconds: %.0f" % (time.time() - t0))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
