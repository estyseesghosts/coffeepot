#!/usr/bin/env python3
"""Run a Switch .nro in the Eden emulator and wait for a report file on the emulated SD card.

Usage: python tools/eden_run.py <file.nro> <sd-subdir> <report-file> [timeout-seconds] [extra-file ...]

Starts Eden with the .nro, waits until <sdmc>/<sd-subdir>/<report-file> exists and its last line is `note,done`,
then stops the emulator, prints the report and copies the report and any extra files next to the .nro (named
<nro-stem>.<file>). Exit status 1 on timeout or when any report line says FAIL or `error,`.

This is an EMULATOR check. It shows that a program starts, finishes and produces the files it claims to. Emulator
timings are never results, and a program that works here can still fail on a console (Eden does not model the
console's memory limits, audio timing, thermal or suspend behaviour). Windows only; Eden's %APPDATA% layout.
"""
import os
import shutil
import subprocess
import sys
import time

EDEN = os.environ.get("EDEN_EXE", os.path.expandvars(r"%USERPROFILE%\Downloads\Eden-Windows-v0.2.1-amd64-clang-pgo\eden.exe"))
SDMC = os.path.join(os.environ.get("APPDATA", ""), "eden", "sdmc")


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    nro, sub, report = sys.argv[1], sys.argv[2], sys.argv[3]
    timeout = float(sys.argv[4]) if len(sys.argv) > 4 else 120.0
    extras = sys.argv[5:]
    folder = os.path.join(SDMC, *sub.split("/"))
    path = os.path.join(folder, report)
    if os.path.exists(path):
        os.remove(path)
    proc = subprocess.Popen([EDEN, "-g", os.path.abspath(nro)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t0, done = time.time(), False
    try:
        while time.time() - t0 < timeout:
            time.sleep(2.0)
            if os.path.exists(path):
                lines = [l.strip() for l in open(path, encoding="utf-8", errors="replace") if l.strip()]
                if lines and lines[-1].startswith("note,done"):
                    done = True
                    break
    finally:
        subprocess.run(["taskkill", "/PID", str(proc.pid), "/T", "/F"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        proc.wait()
    if not done:
        print("TIMEOUT: no finished report at", path)
        return 1
    stem = os.path.splitext(os.path.abspath(nro))[0]
    bad = False
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.rstrip()
        print(line)
        if "FAIL" in line or line.startswith("error,"):
            bad = True
    for name in [report] + extras:
        src = os.path.join(folder, name)
        if os.path.exists(src):
            shutil.copy(src, f"{stem}.{name}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
