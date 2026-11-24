#!/usr/bin/env python3
"""Builds the distributable zip of the PC version.

Contents: pokeemerald.exe (debug symbols stripped: smaller; crash addresses
stay valid against the unstripped build-pc exe, the base is fixed), the
README, and a version note. Everything the game needs is built into the exe.

Never packaged: saves (save.bin, save.rtc), config.ini, ROMs, logs.

Usage: python src/platform_pc/scripts/package.py [build-pc] [dist]
"""
import datetime
import os
import shutil
import subprocess
import sys
import zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
FORBIDDEN = (".bin", ".rtc", ".sav", ".gba", ".ini", ".log", ".wav", ".bmp")


def main():
    build = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build-pc"))
    dist = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "dist"))
    exe_name = "pokeemerald.exe" if os.name == "nt" else "pokeemerald"
    exe = os.path.join(build, exe_name)
    if not os.path.exists(exe):
        sys.exit(f"{exe} not found: build first")

    stage = os.path.join(dist, "pokeemerald-pc")
    shutil.rmtree(stage, ignore_errors=True)
    os.makedirs(stage)
    out_exe = os.path.join(stage, exe_name)
    strip = shutil.which("llvm-strip") or shutil.which("strip")
    if strip:
        subprocess.run([strip, "--strip-debug", "-o", out_exe, exe], check=True)
    else:
        shutil.copy2(exe, out_exe)
    shutil.copy2(os.path.join(ROOT, "README_PC.md"), os.path.join(stage, "README_PC.md"))
    commit = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=ROOT,
                            capture_output=True, text=True).stdout.strip() or "unknown"
    with open(os.path.join(stage, "VERSION.txt"), "w", encoding="utf-8") as f:
        f.write(f"pokeemerald PC port\ncommit {commit}\nbuilt {datetime.date.today()}\n"
                "Crash addresses: symbolize with the matching unstripped build-pc exe.\n")

    for dirpath, _, files in os.walk(stage):
        for name in files:
            if name.lower().endswith(FORBIDDEN):
                sys.exit(f"refusing to package {name}: saves, ROMs and configs stay private")

    zip_path = os.path.join(dist, f"pokeemerald-pc-{commit}.zip")
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
        for dirpath, _, files in os.walk(stage):
            for name in files:
                full = os.path.join(dirpath, name)
                z.write(full, os.path.relpath(full, dist))
    size = os.path.getsize(zip_path) / (1 << 20)
    print(f"{zip_path} ({size:.1f} MiB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
