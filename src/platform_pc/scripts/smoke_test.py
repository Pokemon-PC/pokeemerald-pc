#!/usr/bin/env python3
"""End-to-end check of the PC build (run by CTest as `game_smoke`).

Plays the real game without a window, fast: boot -> copyright -> intro ->
title screen (Start) -> main menu (Start) -> new game (A, A), recording the
sound. Fails if the game crashes or hangs (watchdog), if the title screen
does not hold until Start (it needs the music sequencer), if the frames that
should differ do not, or if the music is silent.

Usage: smoke_test.py GAME_EXE WORK_DIR
"""
import array
import math
import os
import subprocess
import sys
import wave


def frame_bytes(path):
    with open(path, "rb") as f:
        return f.read()[54:]   # skip the BMP header


def colors(path):
    data = frame_bytes(path)
    return len({data[i:i + 3] for i in range(0, len(data) - 2, 3)})


def main_world(exe, work):
    """--world: new game -> truck -> Littleroot Town -> inside the house."""
    env = dict(os.environ, PEM_HEADLESS="1", PEM_FAST="1", PEM_HANG_EXIT="1", PEM_EXIT_AFTER="10600",
               PEM_INPUT="400-404:START,1400-1404:START,1700-7450/25:A,9000-9004:B,9100-9400:RIGHT,"
                         "9800-10500/40:A",
               PEM_SHOT="7600:truck.bmp,9650:town.bmp,10555:house.bmp")
    r = subprocess.run([exe], cwd=work, env=env, capture_output=True, text=True, timeout=900)
    log = r.stdout + r.stderr
    errors = []
    if r.returncode != 0 or "[pem:crash]" in log or "[pem:hang]" in log:
        errors.append(f"exit {r.returncode}, crash/hang in log")
    for n in ("truck", "town", "house"):
        p = os.path.join(work, n + ".bmp")
        if not os.path.exists(p):
            errors.append(f"no {n} frame")
            continue
        c = colors(p)
        print(f"{n}: {c} colours")
        if c < 16:
            errors.append(f"{n} frame looks empty ({c} colours)")
    for e in errors:
        print("FAIL", e)
    if errors:
        print(log[-3000:])
        return 1
    print("world smoke test passed")
    return 0


def main():
    if sys.argv[1] == "--world":
        exe, work = os.path.abspath(sys.argv[2]), os.path.abspath(sys.argv[3])
        os.makedirs(work, exist_ok=True)
        for f in os.listdir(work):
            if f.endswith((".bmp", ".wav", ".bin", ".rtc")):
                os.remove(os.path.join(work, f))
        return main_world(exe, work)
    exe, work = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    os.makedirs(work, exist_ok=True)
    for f in os.listdir(work):
        if f.endswith((".bmp", ".wav", ".bin", ".rtc")):
            os.remove(os.path.join(work, f))
    env = dict(os.environ, PEM_HEADLESS="1", PEM_FAST="1", PEM_HANG_EXIT="1", PEM_WAV="smoke.wav",
               PEM_EXIT_AFTER="2000",
               PEM_INPUT="400-404:START,1400-1404:START,1700-1704:A,1900-1904:A",
               PEM_SHOT="900:title1.bmp,1300:title2.bmp,1500:menu.bmp,1800:birch.bmp")
    r = subprocess.run([exe], cwd=work, env=env, capture_output=True, text=True, timeout=600)
    log = r.stdout + r.stderr
    errors = []
    if r.returncode != 0:
        errors.append(f"exit code {r.returncode}")
    if "[pem:crash]" in log or "[pem:hang]" in log:
        errors.append("crash or hang reported")
    shots = {n: os.path.join(work, n + ".bmp") for n in ("title1", "title2", "menu", "birch")}
    missing = [n for n, p in shots.items() if not os.path.exists(p)]
    if missing:
        errors.append(f"missing frames: {missing}")
    else:
        t1, t2, menu, birch = (frame_bytes(shots[n]) for n in ("title1", "title2", "menu", "birch"))
        # The title screen must still be up 400 frames later (not back to the
        # intro): only the animated parts change, most pixels stay.
        same = sum(1 for a, b in zip(t1, t2) if a == b) / len(t1)
        if same < 0.5:
            errors.append(f"title screen did not hold ({same:.0%} identical)")
        if menu == t2 or birch == menu:
            errors.append("Start/A did not change screens")
    wav = os.path.join(work, "smoke.wav")
    if os.path.exists(wav):
        w = wave.open(wav, "rb")
        a = array.array("h")
        a.frombytes(w.readframes(w.getnframes()))
        sr = w.getframerate()
        w.close()
        seg = a[2 * 8 * sr:2 * 14 * sr]    # 8 s .. 14 s: intro music
        rms = math.sqrt(sum(x * x for x in seg) / max(1, len(seg)))
        if rms < 200:
            errors.append(f"intro music silent (RMS {rms:.0f})")
        print(f"intro music RMS {rms:.0f}")
    else:
        errors.append("no recording")
    for e in errors:
        print("FAIL", e)
    if errors:
        print(log[-3000:])
        return 1
    print("game smoke test passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
