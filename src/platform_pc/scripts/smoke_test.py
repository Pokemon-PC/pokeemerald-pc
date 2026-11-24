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
    """--world: new game -> truck -> Littleroot Town -> inside the house ->
    a wild battle (PEM_TEST hook: Mudkip vs Poochyena, fought with A)."""
    env = dict(os.environ, PEM_HEADLESS="1", PEM_FAST="1", PEM_HANG_EXIT="1", PEM_EXIT_AFTER="13500",
               PEM_TEST="battle@10900",
               PEM_INPUT="400-404:START,1400-1404:START,1700-7450/25:A,9000-9004:B,9100-9400:RIGHT,"
                         "9800-10800/40:A,11400-13400/45:A",
               PEM_SHOT="7600:truck.bmp,9650:town.bmp,10555:house.bmp,11700:battle.bmp,12500:battle2.bmp")
    r = subprocess.run([exe], cwd=work, env=env, capture_output=True, text=True, timeout=900)
    log = r.stdout + r.stderr
    errors = []
    if r.returncode != 0 or "[pem:crash]" in log or "[pem:hang]" in log:
        errors.append(f"exit {r.returncode}, crash/hang in log")
    if "test: wild battle" not in log:
        errors.append("the test battle did not start")
    for n in ("truck", "town", "house", "battle", "battle2"):
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


def run_game(exe, work, env_extra, timeout=900):
    env = dict(os.environ, PEM_HEADLESS="1", PEM_FAST="1", PEM_HANG_EXIT="1", PEM_TRACE="1", **env_extra)
    r = subprocess.run([exe], cwd=work, env=env, capture_output=True, text=True, timeout=timeout)
    return r.returncode, r.stdout + r.stderr


def main_save(exe, work):
    """--save: new game -> menus (bag, trainer card) -> save; then a second
    run continues the save, lands where it was saved, opens party/summary."""
    errors = []
    rc, log = run_game(exe, work, dict(
        PEM_EXIT_AFTER="13700", PEM_INPUT=SAVE_RUN_INPUT,
        PEM_SHOT="12300:bag.bmp,12700:card.bmp"))
    if rc != 0 or "[pem:crash]" in log or "[pem:hang]" in log:
        errors.append(f"first run: exit {rc}, crash/hang")
    save = os.path.join(work, "save.bin")
    if not os.path.exists(save) or os.path.getsize(save) != 0x20000:
        errors.append("no 128 KiB save.bin after saving")
    saved_at = [l for l in log.splitlines() if l.startswith("[pem] trace@")]
    saved_pos = saved_at[-1].split()[2:4] if saved_at else None   # map=..., pos=...

    rc, log = run_game(exe, work, dict(
        PEM_EXIT_AFTER="2900", PEM_TEST="givemon@1800",
        PEM_INPUT="400-404:START,1400-1404:START,1700-1704:A,1850-1853:START,1950-1953:A,"
                  "2150-2153:A,2250-2253:A,2500-2503:RIGHT,2700-2703:RIGHT",
        PEM_SHOT="1600:continue.bmp,2100:party.bmp,2450:summary.bmp,2850:moves.bmp"))
    if rc != 0 or "[pem:crash]" in log or "[pem:hang]" in log:
        errors.append(f"second run: exit {rc}, crash/hang")
    if "flash: loaded save.bin" not in log:
        errors.append("save.bin not loaded")
    free = [l for l in log.splitlines() if l.startswith("[pem] trace@") and "free=1" in l]
    if not free:
        errors.append("never got control after Continue")
    elif saved_pos and free[0].split()[2:4] != saved_pos:
        errors.append(f"continued at {free[0].split()[2:4]}, saved at {saved_pos}")
    else:
        print("continued at", " ".join(free[0].split()[2:4]))
    for n in ("bag", "card", "continue", "party", "summary", "moves"):
        p = os.path.join(work, n + ".bmp")
        if not os.path.exists(p) or colors(p) < 16:
            errors.append(f"{n} frame missing or empty")
    for e in errors:
        print("FAIL", e)
    if errors:
        print(log[-3000:])
        return 1
    print("save/continue/menus test passed")
    return 0


SAVE_RUN_INPUT = ("400-404:START,1400-1404:START,1700-7450/25:A,9000-9004:B,9100-9400:RIGHT,"
                  "9800-12040/40:A,12100-12103:START,12150-12153:A,12400-12403:B,12500-12503:DOWN,"
                  "12550-12553:A,12800-12803:B,12900-12903:DOWN,12950-12953:A,13100-13103:A,13500-13503:A")

# From the save made in the house (1F): Continue, upstairs, set the wall
# clock (YES), Mom, downstairs (TV), out, the rival's house and bedroom (Poke
# Ball, meeting the rival), out, north: the twin, Route 101, Prof. Birch
# chased, his bag, choose the first starter, the battle, his lab.
STORY_INPUT = ",".join([
    "400-404:START", "1400-1404:START", "1700-1704:A",                     # title, Continue
    "1760-1880:UP", "1930-1965:LEFT", "1990-1994:UP", "2030-2034:A",      # stairs, clock
    "2150-2154:A", "2210-2214:A", "2400-2404:A", "2650-2654:A",            # set the time
    "2750-2754:UP", "2800-2804:A", "2950-3900/50:A",                       # YES, Mom
    "3960-3985:RIGHT", "4010-4030:UP", "4200-6400/50:A",                   # downstairs, TV
    "5900-5968:RIGHT", "5990-6050:DOWN",                                   # out
    "6150-6296:RIGHT", "6310-6330:UP", "6500-7600/50:A",                   # rival's house
    "7620-7730:UP", "7800-7866:RIGHT", "7880-7898:DOWN", "7920-7924:A",    # bedroom, Poke Ball
    "8000-10250/50:A",                                                     # the rival
    "10310-10330:UP", "10345-10415:LEFT", "10430-10450:UP", "10560-10660:DOWN",
    "10740-10800:LEFT", "10815-10918:UP", "10925-10938:RIGHT", "10945-10972:UP",
    "11050-11700/50:A", "11710-11740:UP", "11900-12550/50:A",              # twin, Route 101
    "12560-12620:LEFT", "12640-12644:UP", "12680-12684:A",                 # Birch's bag
    "12780-12784:A", "12900-12904:A", "13000-17900/40:A",                  # starter, battle
])


def main_story(exe, work):
    """--story: the save run of --save, then the first act of the story from
    that save, up to Prof. Birch's lab with the starter."""
    errors = []
    rc, log = run_game(exe, work, dict(PEM_EXIT_AFTER="13700", PEM_INPUT=SAVE_RUN_INPUT))
    if rc != 0 or "[pem:crash]" in log or "[pem:hang]" in log:
        errors.append(f"save run: exit {rc}, crash/hang")
    rc, log = run_game(exe, work, dict(
        PEM_EXIT_AFTER="18000", PEM_INPUT=STORY_INPUT,
        PEM_SHOT="2780:clock.bmp,8500:rival.bmp,11800:birch.bmp,12880:starter.bmp,"
                 "14200:battle.bmp,17950:lab.bmp"))
    if rc != 0 or "[pem:crash]" in log or "[pem:hang]" in log:
        errors.append(f"story run: exit {rc}, crash/hang")
    trace = [l for l in log.splitlines() if l.startswith("[pem] trace@")]
    for label, map_id in (("rival's bedroom", "map=1.3 "), ("Route 101", "map=0.16 "), ("Birch's lab", "map=1.4 ")):
        if not any(map_id in l for l in trace):
            errors.append(f"never reached {label}")
    for n in ("clock", "rival", "birch", "starter", "battle", "lab"):
        p = os.path.join(work, n + ".bmp")
        if not os.path.exists(p) or colors(p) < 16:
            errors.append(f"{n} frame missing or empty")
    for e in errors:
        print("FAIL", e)
    if errors:
        print(log[-3000:])
        return 1
    print("story test passed (lab reached)")
    return 0


def main_french(exe, work, rom):
    """--french: the same new game with the French text read from the
    player's French ROM (PEM_FR_ROM at configure time; never in the repo)."""
    rc, log = run_game(exe, work, dict(
        PEM_LANG="fr", PEM_ROM=rom, PEM_EXIT_AFTER="13500", PEM_TEST="battle@10900",
        PEM_INPUT="400-404:START,1400-1404:START,1700-7450/25:A,9000-9004:B,9100-9400:RIGHT,"
                  "9800-10800/40:A,11400-13400/45:A",
        PEM_SHOT="1500:menu_fr.bmp,1850:birch_fr.bmp,9650:town_fr.bmp,12100:battle_fr.bmp"))
    errors = []
    if rc != 0 or "[pem:crash]" in log or "[pem:hang]" in log:
        errors.append(f"exit {rc}, crash/hang")
    import re
    m = re.search(r"lang: French from .* \((\d+) strings", log)
    if not m or int(m.group(1)) < 12000:
        errors.append("French text not loaded" if not m else f"only {m.group(1)} French strings")
    for n in ("menu_fr", "birch_fr", "town_fr", "battle_fr"):
        p = os.path.join(work, n + ".bmp")
        if not os.path.exists(p) or colors(p) < 16:
            errors.append(f"{n} frame missing or empty")
    for e in errors:
        print("FAIL", e)
    if errors:
        print(log[-3000:])
        return 1
    print(f"French test passed ({m.group(1)} strings)")
    return 0


def main():
    if sys.argv[1] == "--french":
        exe, work, rom = os.path.abspath(sys.argv[2]), os.path.abspath(sys.argv[3]), sys.argv[4]
        os.makedirs(work, exist_ok=True)
        for f in os.listdir(work):
            if f.endswith((".bmp", ".wav", ".bin", ".rtc")):
                os.remove(os.path.join(work, f))
        return main_french(exe, work, rom)
    if sys.argv[1] in ("--save", "--story"):
        exe, work = os.path.abspath(sys.argv[2]), os.path.abspath(sys.argv[3])
        os.makedirs(work, exist_ok=True)
        for f in os.listdir(work):
            if f.endswith((".bmp", ".wav", ".bin", ".rtc")):
                os.remove(os.path.join(work, f))
        return (main_story if sys.argv[1] == "--story" else main_save)(exe, work)
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
