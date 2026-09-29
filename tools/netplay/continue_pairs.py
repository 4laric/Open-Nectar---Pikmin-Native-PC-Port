"""Multi-session pair tests for netplay recovery v1 (M5c lane C, issue #887).

Drives the one-command launcher exactly like launch_pair.py (hidden, private,
bounded; codes moved through the files a human would paste; loopback ICE,
scripted inputs), but across SESSIONS that share one exe staging folder, so
the second session's `--netplay-host-ice --continue` finds the first
session's run folder under <stage>/netplay/ exactly as a player's would.

Scenarios (each gets its own stage folder under --out, so they never see each
other's campaigns):
  clean       S1 plays through the day-2 end (the day-end save, gen 1) and
              both peers quit at the tick limit on day 3; S2 continues with
              --continue on the host only: both start day 3 from gen 1, the
              joiner adopts the host's checkpoint, 3,000 identical ticks.
  desync      S1 saves day 2, then PIKMIN_NETPLAY_TEST_DESYNC_AT_FRAME fires
              on day 3: both peers exit 5 with the recovery message; S2
              continues from the last saved day.
  disconnect  S1 saves day 2; the joiner is killed mid-day 3; the host
              reports the lost connection; S2 continues from day 3's start.
  nosave      S1 never reaches a day end; S2's --continue says there is no
              saved day, starts a new campaign (day 2) and plays in sync; S0
              does the same on a stage that has no netplay folder at all.
  quit        the joiner's window is closed mid-session (WM_CLOSE to its
              hidden window): it tells the host at once (quit notice) and
              both print the recovery message.
  p2          --continue plumbing for a seed with P2 enemies (--p2-bootstrap,
              --p2-assets): S1 plays the seed from its own folder; a day-2
              checkpoint is then SYNTHESISED into S1's host campaign (a real
              day-2-end game block from --p2-donor-sav, re-signed with the P2
              seed's fingerprint and checkpoint header, plus the donor card
              files and two record lines), because the scripted pad cannot be
              relied on to reach and accept a P2 day-end save; S2 continues it
              with --continue --bootstrap <seed>: the overlay recorded by S1,
              the sidecars from S1's play/ folder, the joiner's transfer, and
              day 3 on both peers. The seed folder is checked unchanged.

Every scenario checks that the continued run folder is unchanged by the
session that continued it (same files, same bytes: --continue only reads it).

Hidden, private, bounded: PIKMIN_RANDOMIZER_TEST_BACKGROUND=1 and
SDL_AUDIODRIVER=dummy, loopback binds, hard timeouts, and only the PIDs this
script started are ever signalled.
"""

import argparse
import ctypes
import hashlib
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import launch_pair as lp  # noqa: E402
import run_pair as rp  # noqa: E402


def lines_with(path, *needles):
    out = []
    for n in needles:
        out += lp.grep(path, n)
    return [ln.strip() for ln in out]


def recovery_block(path):
    """The final message: the '==== netplay session ended ====' line and the
    [netplay] lines right after it."""
    try:
        text = Path(path).read_text(errors="replace").splitlines()
    except OSError:
        return []
    for i, ln in enumerate(text):
        if "==== netplay session ended ====" in ln:
            block = [ln.strip()]
            for nxt in text[i + 1:i + 9]:
                if nxt.startswith("[netplay] ") and "end banner" not in nxt and "hud" not in nxt:
                    block.append(nxt.strip())
                else:
                    break
            return block
    return []


def folder_digest(path):
    """{relative path: sha256} of every file under a run folder (junctions
    are not followed), for the 'the continued run is only read' check."""
    out = {}
    if path is None or not Path(path).exists():
        return out
    for root, dirs, files in os.walk(path):
        dirs[:] = sorted(d for d in dirs if not os.path.isjunction(os.path.join(root, d)))
        for name in sorted(files):
            full = Path(root) / name
            h = hashlib.sha256()
            with open(full, "rb") as f:
                for chunk in iter(lambda: f.read(65536), b""):
                    h.update(chunk)
            out[full.relative_to(path).as_posix()] = h.hexdigest()
    return out


def hash_lines(path):
    try:
        with open(path, "rb") as f:
            return sum(1 for _ in f)
    except OSError:
        return 0


def post_wm_close(pid):
    """Posts WM_CLOSE to every top-level window of `pid` (our own child)."""
    if sys.platform != "win32":
        return 0
    user32 = ctypes.windll.user32
    found = []
    WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)

    def cb(hwnd, _):
        wpid = ctypes.c_ulong(0)
        user32.GetWindowThreadProcessId(ctypes.c_void_p(hwnd), ctypes.byref(wpid))
        if wpid.value == pid:
            found.append(hwnd)
        return True

    user32.EnumWindows(WNDENUMPROC(cb), 0)
    for hwnd in found:
        user32.PostMessageW(ctypes.c_void_p(hwnd), 0x0010, 0, 0)  # WM_CLOSE
    return len(found)


class Ctx:
    def __init__(self, a, scenario):
        self.a = a
        self.root = (a.out / scenario).resolve()
        self.stage = self.root / "stage"
        self.checks = []
        self.sessions = {}

    def check(self, ok, what):
        self.checks.append({"ok": bool(ok), "what": what})
        print(f"continue_pairs: {'ok  ' if ok else 'FAIL'} {what}")
        return ok


def run_session(ctx, name, ticks, host_extra=(), host_env=None, join_env=None, expect="sync",
                kill_join_at=None, close_join_at=None, stage=None, join_extra=()):
    a = ctx.a
    out = ctx.root / name
    if out.exists():
        raise SystemExit(f"continue_pairs: {out} exists; pick a new --out (evidence is never overwritten)")
    host_cwd, join_cwd = out / "host", out / "join"
    host_cwd.mkdir(parents=True)
    join_cwd.mkdir(parents=True)
    rp.link_assets(host_cwd, a.assets)
    rp.link_assets(join_cwd, a.assets)
    stage = stage or ctx.stage
    exe = lp.stage_exe(a.exe.resolve(), stage)
    offer, answer = out / "offer.txt", out / "answer.txt"
    host_hash, join_hash = host_cwd / "hashes.txt", join_cwd / "hashes.txt"
    host_log, join_log = out / "host.log", out / "join.log"
    hin, jin = out / "host_inputs.pkni", out / "join_inputs.pkni"
    rp.gen_inputs(ticks + 50, a.seed_a, hin)
    rp.gen_inputs(ticks + 50, a.seed_b, jin)
    common = {
        "PIKMIN_NETPLAY_STUN": "none",
        "PIKMIN_NETPLAY_UNTHROTTLED": "1",
        "PIKMIN_NETPLAY_ICE_TIMEOUT_MS": "120000",
        "PIKMIN_NETPLAY_HANDSHAKE_TIMEOUT_MS": "30000",
    }
    henv = dict(common, PIKMIN_STATE_HASH_LOG=str(host_hash), PIKMIN_NETPLAY_LOCAL_INPUT_FILE=str(hin),
                PIKMIN_NETPLAY_ICE_PORT_BEGIN=str(a.port_base), PIKMIN_NETPLAY_ICE_PORT_END=str(a.port_base + 9),
                PIKMIN_NETPLAY_DELAY=str(a.delay_host))
    jenv = dict(common, PIKMIN_STATE_HASH_LOG=str(join_hash), PIKMIN_NETPLAY_LOCAL_INPUT_FILE=str(jin),
                PIKMIN_NETPLAY_ICE_PORT_BEGIN=str(a.port_base + 10),
                PIKMIN_NETPLAY_ICE_PORT_END=str(a.port_base + 19), PIKMIN_NETPLAY_DELAY=str(a.delay_join))
    if a.hud_shot_frames:
        for side, env in (("host", henv), ("join", jenv)):
            shots = out / f"shots-{side}"
            shots.mkdir()
            env["PIKMIN_NETPLAY_TEST_HUD_SHOT"] = str(shots)
            env["PIKMIN_NETPLAY_TEST_HUD_SHOT_FRAME"] = a.hud_shot_frames
    henv.update(host_env or {})
    jenv.update(join_env or {})
    host_args = ["--netplay-host-ice", "--netplay-code-out", str(offer), "--netplay-answer-in", str(answer),
                 "--netplay-test-hidden", "--netplay-test-ticks", str(ticks), "--netplay-input", "keyboard"]
    host_args += list(host_extra)
    join_args = ["--netplay-join-ice", f"@{offer}", "--netplay-code-out", str(answer), "--netplay-test-hidden",
                 "--netplay-test-ticks", str(ticks), "--netplay-input", "gamepad:0"]
    join_args += list(join_extra)
    s = {"name": name, "ticks": ticks, "expect": expect, "exe": str(exe), "exe_sha256": lp.sha256_file(exe),
         "host_cmd": [str(exe)] + host_args, "join_cmd": [str(exe)] + join_args,
         "host_env": {k: v for k, v in henv.items() if k.startswith("PIKMIN_NETPLAY_TEST") or k.endswith("DELAY")},
         "join_env": {k: v for k, v in jenv.items() if k.startswith("PIKMIN_NETPLAY_TEST") or k.endswith("DELAY")}}
    print(f"continue_pairs: [{name}] {' '.join(s['host_cmd'][1:])}")
    hp = jp = None
    files = []
    start = time.time()
    try:
        hp, f = lp.launch(exe, host_cwd, host_args, henv, host_log)
        files.append(f)
        lp.wait_code(offer, a.code_timeout, "host offer", [hp])
        jp, f = lp.launch(exe, join_cwd, join_args, jenv, join_log)
        files.append(f)
        lp.wait_code(answer, a.code_timeout, "joiner answer", [hp, jp])
        deadline = start + a.timeout
        acted = False
        while time.time() < deadline and (hp.poll() is None or jp.poll() is None):
            if not acted and (kill_join_at or close_join_at) and jp.poll() is None:
                at = kill_join_at or close_join_at
                if hash_lines(join_hash) >= at:
                    acted = True
                    s["acted_at_join_hash_lines"] = hash_lines(join_hash)
                    s["acted_wall"] = time.time()
                    if kill_join_at:
                        print(f"continue_pairs: [{name}] killing joiner pid {jp.pid} at hash line "
                              f"{s['acted_at_join_hash_lines']}")
                        jp.kill()
                    else:
                        n = post_wm_close(jp.pid)
                        print(f"continue_pairs: [{name}] WM_CLOSE to joiner pid {jp.pid} ({n} windows) at hash "
                              f"line {s['acted_at_join_hash_lines']}")
                        s["wm_close_windows"] = n
            if hp.poll() is not None and "host_exit_wall" not in s:
                s["host_exit_wall"] = time.time()
            time.sleep(0.2)
        if "host_exit_wall" not in s and hp.poll() is not None:
            s["host_exit_wall"] = time.time()
    except (RuntimeError, subprocess.TimeoutExpired) as e:
        s["error"] = str(e)
        print(f"continue_pairs: [{name}] {e}")
    finally:
        for p in (hp, jp):
            if p is not None and p.poll() is None:
                print(f"continue_pairs: [{name}] killing pid {p.pid} (timeout)")
                p.kill()
                try:
                    p.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    pass
        for f in files:
            f.close()
    s["seconds"] = round(time.time() - start, 1)
    s["exit"] = {"host": hp.returncode if hp else None, "join": jp.returncode if jp else None}
    if "acted_wall" in s and "host_exit_wall" in s:
        s["host_exit_after_action_s"] = round(s["host_exit_wall"] - s["acted_wall"], 2)
    r = subprocess.run([sys.executable, str(rp.CMP), str(host_hash), str(join_hash)], capture_output=True, text=True)
    tail = (r.stdout + r.stderr).strip().splitlines()
    s["compare"] = {"exit": r.returncode, "last": tail[-1] if tail else ""}
    for side, log, hashes in (("host", host_log, host_hash), ("join", join_log, join_hash)):
        rd = lp.run_dir_of(log)
        info = {
            "run_dir": str(rd) if rd else None,
            "stats": lp.gameplay_stats(hashes),
            "start_stage": lines_with(log, "START_STAGE"),
            "resumed": lines_with(log, "CAMPAIGN_RESUMED"),
            "continue": lines_with(log, "[netplay] launch: --continue", "[netplay] launch: Start a new",
                                   "[netplay] launch: not starting"),
            "checkpoint": lines_with(log, "[netplay] checkpoint", "[netplay] transfer:"),
            "barrier": lines_with(log, "save barrier frame=", "save barrier abandoned", "save barrier timeout",
                                  "CAMPAIGN_SAVED"),
            "reseed": lines_with(log, "[netplay-det] reseed"),
            "delay": lines_with(log, "[netplay] session started:"),
            "end": recovery_block(log),
            "banner": lines_with(log, "[netplay] end banner"),
            "events": lines_with(log, "[netplay] disconnected", "[netplay] desync detected", "[netplay] quit:",
                                 "[netplay] test:"),
            "hud": lines_with(log, "[netplay] hud"),
            "link": lines_with(log, "[netplay] link:")[-3:],
            "p2": lines_with(log, "[netplay] launch: P2", "sidecars received", "[netplay] p2 digests",
                             "[netplay] transfer: sending"),
        }
        if rd:
            for fname in ("campaign-record.txt", "launch.txt"):
                try:
                    info[fname] = (Path(rd) / fname).read_text(errors="replace").splitlines()
                except OSError:
                    info[fname] = None
            info["tree"] = lp.tree(rd)
        s[side] = info
    (out / "session.json").write_text(json.dumps(s, indent=2))
    print(f"continue_pairs: [{name}] exit host={s['exit']['host']} join={s['exit']['join']} "
          f"{s['seconds']}s compare: {s['compare']['last']}")
    for side in ("host", "join"):
        i = s[side]
        print(f"continue_pairs: [{name}] {side} run {i['run_dir']} {i['stats']}")
        for key in ("continue", "checkpoint", "resumed", "start_stage", "barrier", "reseed", "delay", "events",
                    "end", "banner", "hud", "link"):
            for ln in i[key]:
                print(f"continue_pairs: [{name}] {side} {ln}")
        for ln in i.get("campaign-record.txt") or []:
            if not ln.startswith("#"):
                print(f"continue_pairs: [{name}] {side} record: {ln}")
    ctx.sessions[name] = s
    return s


def gameplay_ok(ctx, s, name, min_distinct):
    ok = True
    for side in ("host", "join"):
        st = s[side]["stats"]
        ok &= ctx.check(st["ticks"] == s["ticks"] and st["distinct_tuples"] >= min_distinct,
                        f"{name} {side}: {st['ticks']} hash lines, {st['distinct_tuples']} distinct tuples "
                        f"(>= {min_distinct})")
        ok &= ctx.check(bool(s[side]["start_stage"]), f"{name} {side}: START_STAGE logged")
    ok &= ctx.check(s["compare"]["exit"] == 0, f"{name}: hash logs identical ({s['compare']['last']})")
    return ok


def day_end_ok(ctx, s, name):
    """S1 saved day 2 on both peers (gen 1) and the record confirms it."""
    ok = True
    for side in ("host", "join"):
        b = [ln for ln in s[side]["barrier"] if "save barrier frame=" in ln and "gen=1" in ln and "host_ok=1" in ln]
        ok &= ctx.check(bool(b), f"{name} {side}: day-end save barrier agreed gen=1 ({b[:1]})")
        rec = s[side].get("campaign-record.txt") or []
        ok &= ctx.check(any(r.startswith("saved gen=1 ") for r in rec) and any(r.startswith("day gen=1 day=3")
                                                                               for r in rec),
                        f"{name} {side}: record has 'saved gen=1' and 'day gen=1 day=3'")
    return ok


def continue_ok(ctx, s, name, from_run, gen=1, day=3):
    ok = True
    h, j = s["host"], s["join"]
    want = f"continuing the campaign of {Path(from_run).as_posix()}: checkpoint {gen} (day {day})"
    ok &= ctx.check(any(want.replace("\\", "/") in ln.replace("\\", "/") for ln in h["continue"]),
                    f"{name} host: '{want}'")
    ok &= ctx.check(any(f"CAMPAIGN_RESUMED generation={gen}" in ln for ln in h["resumed"]),
                    f"{name} host: CAMPAIGN_RESUMED generation={gen}")
    ok &= ctx.check(any(f"checkpoint adopted gen={gen}" in ln for ln in j["checkpoint"]),
                    f"{name} join: checkpoint adopted gen={gen}")
    for side in ("host", "join"):
        # The resumed first stage starts through MapSelect (stage id 0 in the
        # log, as in B2's resume evidence).
        ok &= ctx.check(any("START_STAGE" in ln and f" day={day} resumed=1 generation={gen}" in ln
                            for ln in s[side]["start_stage"]),
                        f"{name} {side}: START_STAGE ... day={day} resumed=1 generation={gen}")
        rec = s[side].get("campaign-record.txt") or []
        ok &= ctx.check(any(r.startswith(f"start gen={gen} ") for r in rec), f"{name} {side}: record 'start gen={gen}'")
    rec = h.get("campaign-record.txt") or []
    ok &= ctx.check(any(r.startswith(f"carried gen={gen} day={day} from=") for r in rec),
                    f"{name} host: record 'carried gen={gen} day={day}'")
    return ok


def recovery_ok(ctx, s, name, side, needles):
    block = s[side]["end"]
    ok = ctx.check(bool(block), f"{name} {side}: final recovery message printed ({len(block)} lines)")
    for n in needles:
        ok &= ctx.check(any(n in ln for ln in block), f"{name} {side}: message contains '{n}'")
    return ok


def scenario_dayend(ctx, kind):
    a = ctx.a
    ticks1 = a.dayend_ticks
    host_env = {}
    kill = close = None
    expect = "sync"
    if kind == "desync":
        host_env["PIKMIN_NETPLAY_TEST_DESYNC_AT_FRAME"] = str(a.event_frame)
        expect = "exit5"
    elif kind == "disconnect":
        kill = a.event_frame
        expect = "disconnect"
    s1 = run_session(ctx, "s1", ticks1, host_env=host_env, expect=expect, kill_join_at=kill, close_join_at=close)
    day_end_ok(ctx, s1, "s1")
    if kind == "clean":
        gameplay_ok(ctx, s1, "s1", a.min_distinct)
        ctx.check(s1["exit"] == {"host": 0, "join": 0}, f"s1: both exit 0 ({s1['exit']})")
        ctx.check(any("reseed day=3" in ln for ln in s1["host"]["reseed"]), "s1: day 3 started before the quit")
    elif kind == "desync":
        ctx.check(s1["exit"] == {"host": 5, "join": 5}, f"s1: both exit 5 ({s1['exit']})")
        for side in ("host", "join"):
            ctx.check(any("desync detected" in ln for ln in s1[side]["events"]), f"s1 {side}: desync detected")
            recovery_ok(ctx, s1, "s1", side, ["DESYNC", "Last saved day: day 3", "--continue"])
            ctx.check(any("closed after" in ln for ln in s1[side]["banner"]), f"s1 {side}: end banner shown and closed")
        ctx.check(any(r.startswith("end kind=desync code=5") for r in s1["host"].get("campaign-record.txt") or []),
                  "s1 host: record 'end kind=desync code=5'")
        # The injection alters only the reported checksum: the two hash logs
        # stay equal up to where each peer stopped (information, not a gate).
        print(f"continue_pairs: s1 hash logs up to the stop: {s1['compare']['last']}")
    elif kind == "disconnect":
        ctx.check(s1["exit"]["host"] == 0, f"s1: host exit 0 after the lost connection ({s1['exit']})")
        ctx.check(any("disconnected" in ln for ln in s1["host"]["events"]), "s1 host: disconnected")
        recovery_ok(ctx, s1, "s1", "host", ["CONNECTION LOST", "Last saved day: day 3", "--continue"])
        ctx.check(s1.get("acted_at_join_hash_lines", 0) >= a.event_frame, f"s1: joiner killed on day 3 at hash "
                  f"line {s1.get('acted_at_join_hash_lines')}")
    host_run = s1["host"]["run_dir"]
    before = folder_digest(host_run)
    s2 = run_session(ctx, "s2", a.continue_ticks, host_extra=["--continue"], expect="sync")
    after = folder_digest(host_run)
    ctx.check(bool(before) and before == after, f"s2: the continued run folder is unchanged ({len(before)} files, "
              f"same bytes)")
    continue_ok(ctx, s2, "s2", host_run)
    gameplay_ok(ctx, s2, "s2", a.min_distinct)
    ctx.check(s2["exit"] == {"host": 0, "join": 0}, f"s2: both exit 0 ({s2['exit']})")
    if kind == "clean" and not a.skip_s3:
        # S3: `--continue <run folder>` naming S1's JOINER run: its mirror
        # campaign is the same campaign, so either player can host next.
        join_run = s1["join"]["run_dir"]
        jbefore = folder_digest(join_run)
        s3 = run_session(ctx, "s3", a.nosave_ticks, host_extra=["--continue", join_run], expect="sync")
        ctx.check(bool(jbefore) and folder_digest(join_run) == jbefore, "s3: the named joiner run folder is unchanged")
        continue_ok(ctx, s3, "s3", join_run)
        gameplay_ok(ctx, s3, "s3", a.min_distinct // 2)
        ctx.check(s3["exit"] == {"host": 0, "join": 0}, f"s3: both exit 0 ({s3['exit']})")


def scenario_nosave(ctx):
    a = ctx.a
    # S0: a stage with no netplay folder at all.
    s0 = run_session(ctx, "s0-empty-stage", a.nosave_ticks, host_extra=["--continue"],
                     stage=ctx.root / "stage-empty")
    for s, name in ((s0, "s0"),):
        ctx.check(any("no saved day yet" in ln for ln in s["host"]["continue"]), f"{name} host: 'no saved day yet'")
        ctx.check(any("starting a new campaign instead" in ln for ln in s["host"]["continue"]),
                  f"{name} host: 'starting a new campaign instead'")
        gameplay_ok(ctx, s, name, a.min_distinct // 2)
        ctx.check(s["exit"] == {"host": 0, "join": 0}, f"{name}: both exit 0 ({s['exit']})")
    s1 = run_session(ctx, "s1", a.nosave_ticks)
    gameplay_ok(ctx, s1, "s1", a.min_distinct // 2)
    host_run = s1["host"]["run_dir"]
    before = folder_digest(host_run)
    s2 = run_session(ctx, "s2", a.nosave_ticks, host_extra=["--continue"])
    ctx.check(folder_digest(host_run) == before, "s2: the old run folder is unchanged")
    ctx.check(any("no saved day yet" in ln for ln in s2["host"]["continue"]), "s2 host: 'no saved day yet'")
    ctx.check(any("starting a new campaign instead" in ln for ln in s2["host"]["continue"]),
              "s2 host: 'starting a new campaign instead'")
    for side in ("host", "join"):
        ctx.check(any("START_STAGE 1 day=2" in ln for ln in s2[side]["start_stage"]), f"s2 {side}: new campaign on day 2")
        ctx.check(not s2[side]["resumed"], f"s2 {side}: nothing resumed")
    gameplay_ok(ctx, s2, "s2", a.min_distinct // 2)
    ctx.check(s2["exit"] == {"host": 0, "join": 0}, f"s2: both exit 0 ({s2['exit']})")


def scenario_quit(ctx):
    a = ctx.a
    # The close must land well before the session's own tick limit.
    s1 = run_session(ctx, "s1", max(a.nosave_ticks, a.quit_frame + 1500), close_join_at=a.quit_frame, expect="quit")
    ctx.check(s1.get("wm_close_windows", 0) >= 1, f"s1: WM_CLOSE reached the joiner's window ({s1.get('wm_close_windows')})")
    ctx.check(any("quit: this game left the session" in ln for ln in s1["join"]["events"]), "s1 join: quit notice sent")
    recovery_ok(ctx, s1, "s1", "join", ["You left the session", "Nothing is saved yet"])
    ctx.check(any("disconnected" in ln for ln in s1["host"]["events"]), "s1 host: disconnected")
    recovery_ok(ctx, s1, "s1", "host", ["THE OTHER PLAYER LEFT"])
    ctx.check(s1.get("host_exit_after_action_s", 99) < 10.0,
              f"s1: host ended {s1.get('host_exit_after_action_s')} s after the joiner closed (quit notice, not the "
              f"15 s timeout)")
    ctx.check(s1["exit"] == {"host": 0, "join": 0}, f"s1: both exit 0 ({s1['exit']})")


def fnv1a64(data):
    h = 14695981039346656037
    for b in data:
        h ^= b
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def seed_flags_header(boot_text):
    """(magic, used-count) of pc_randomizer.cpp write_campaign_checkpoint for
    a bootstrap: BENEFITS <mode> sets bombDeliveries (bit 0 of mode-1),
    bombTraps (bit 2), proggTraps (bit 3) and prereleaseTraps (bit 4)."""
    mode = 0
    toks = boot_text.split()
    for i, t in enumerate(toks[:-1]):
        if t == "BENEFITS":
            mode = int(toks[i + 1])
    m = mode - 1 if mode > 0 else 0
    if mode > 0 and m & 16:
        return "PIKMIN_CAMPAIGN_5", 7
    if mode > 0 and m & 8:
        return "PIKMIN_CAMPAIGN_4", 6
    if mode > 0 and m & 4:
        return "PIKMIN_CAMPAIGN_3", 5
    if mode > 0 and m & 1:
        return "PIKMIN_CAMPAIGN_2", 4
    return "PIKMIN_CAMPAIGN_1", 3


def scenario_p2(ctx):
    a = ctx.a
    boot = a.p2_bootstrap.resolve()
    seed_dir = boot.parent
    before_seed = folder_digest(seed_dir)
    s1 = run_session(ctx, "s1", a.nosave_ticks, host_extra=["--bootstrap", str(boot)],
                     join_extra=["--netplay-p2-assets", str(a.p2_assets.resolve())])
    gameplay_ok(ctx, s1, "s1", a.min_distinct // 4)
    ctx.check(s1["exit"] == {"host": 0, "join": 0}, f"s1: both exit 0 ({s1['exit']})")
    ctx.check(any("p2_assets " in ln for ln in s1["host"].get("launch.txt") or []), "s1 host: launch.txt records p2_assets")
    host_run = Path(s1["host"]["run_dir"])
    # Synthesise the day-2-end checkpoint (see the module docstring).
    text = boot.read_text(errors="replace")
    fp = text.split("FINGERPRINT", 1)[1].split()[0]
    magic, used = seed_flags_header(text)
    donor = a.p2_donor_sav.read_bytes()
    block = donor[donor.index(b"\n") + 1:]
    meta = f"{magic} {fp} 1" + " 0" * used
    sav = (meta + f" {fnv1a64((meta + chr(10)).encode() + block)}\n").encode() + block
    camp = host_run / "session" / "campaign"
    (camp / "card" / "card0").mkdir(parents=True, exist_ok=True)
    (camp / "00000000000000000001.sav").write_bytes(sav)
    donor_card = a.p2_donor_sav.parent / "card" / "card0"
    for f in sorted(donor_card.iterdir()):
        (camp / "card" / "card0" / f.name).write_bytes(f.read_bytes())
    with open(host_run / "campaign-record.txt", "a", newline="\n") as f:
        f.write("saved gen=1 frame=0 day_ended=2 synthetic=continue_pairs-p2\nday gen=1 day=3\n")
    print(f"continue_pairs: [p2] synthesised {magic} gen 1 ({len(sav)} B) from {a.p2_donor_sav} into {camp}")
    ctx.check(True, f"p2: synthetic day-2 checkpoint {magic}/{used} written into {camp} (test setup)")
    run_before = folder_digest(host_run)
    s2 = run_session(ctx, "s2", a.continue_ticks, host_extra=["--continue", "--bootstrap", str(boot)],
                     join_extra=["--netplay-p2-assets", str(a.p2_assets.resolve())])
    ctx.check(folder_digest(host_run) == run_before, "s2: the continued P2 run folder is unchanged")
    continue_ok(ctx, s2, "s2", str(host_run))
    ctx.check(any("assets -> " in ln and Path(a.p2_assets).name in ln.replace("\\", "/") for ln in s2["host"]["p2"])
              or any("assets -> " in ln for ln in s2["host"]["p2"]), "s2 host: P2 working directory with the overlay")
    ctx.check(any("sidecars:" in ln and "copied from" in ln and "/play" in ln.replace("\\", "/") for ln in s2["host"]["p2"]),
              "s2 host: P2 sidecars copied from the continued run's play/ folder")
    gameplay_ok(ctx, s2, "s2", a.min_distinct // 4)
    ctx.check(s2["exit"] == {"host": 0, "join": 0}, f"s2: both exit 0 ({s2['exit']})")
    ctx.check(folder_digest(seed_dir) == before_seed, f"the seed folder {seed_dir} is unchanged")


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--exe", type=Path, required=True, help="netplay build nectar.exe (copied into each stage)")
    p.add_argument("--out", type=Path, required=True, help="evidence root; <out>/<scenario>/ must not exist")
    p.add_argument("--scenario", choices=("clean", "desync", "disconnect", "nosave", "quit", "p2"), required=True)
    p.add_argument("--p2-bootstrap", type=Path, default=None, help="p2: the seed's bootstrap.txt (its folder holds assets/)")
    p.add_argument("--p2-assets", type=Path, default=None, help="p2: the joiner's copy of the overlay")
    p.add_argument("--p2-donor-sav", type=Path, default=None,
                   help="p2: a real day-2-end checkpoint of the same PROFILE (its game block and card are reused)")
    p.add_argument("--assets", type=Path, default=rp.DEFAULT_ASSETS)
    p.add_argument("--port-base", type=int, default=48850)
    p.add_argument("--seed-a", type=int, default=101)
    p.add_argument("--seed-b", type=int, default=202)
    p.add_argument("--delay-host", type=int, default=2)
    p.add_argument("--delay-join", type=int, default=1)
    p.add_argument("--dayend-ticks", type=int, default=31500)
    p.add_argument("--event-frame", type=int, default=30000, help="day-3 frame of the desync / joiner kill")
    p.add_argument("--quit-frame", type=int, default=1500)
    p.add_argument("--continue-ticks", type=int, default=3000)
    p.add_argument("--nosave-ticks", type=int, default=1500)
    p.add_argument("--min-distinct", type=int, default=1000)
    p.add_argument("--timeout", type=float, default=2400)
    p.add_argument("--hud-shot-frames", default="",
                   help="comma list of frames: both peers write HUD captures to <session>/shots-<side>/ "
                        "(and banner.bmp on an end banner)")
    p.add_argument("--skip-s3", action="store_true", help="clean: skip the --continue <joiner run> session")
    p.add_argument("--code-timeout", type=float, default=180)
    a = p.parse_args(argv)
    ctx = Ctx(a, a.scenario)
    if ctx.root.exists():
        raise SystemExit(f"continue_pairs: {ctx.root} exists; pick a new --out")
    ctx.root.mkdir(parents=True)
    t0 = time.time()
    if a.scenario in ("clean", "desync", "disconnect"):
        scenario_dayend(ctx, a.scenario)
    elif a.scenario == "nosave":
        scenario_nosave(ctx)
    elif a.scenario == "p2":
        scenario_p2(ctx)
    else:
        scenario_quit(ctx)
    ok = all(c["ok"] for c in ctx.checks)
    summary = {"scenario": a.scenario, "pass": ok, "seconds": round(time.time() - t0, 1), "checks": ctx.checks,
               "sessions": list(ctx.sessions)}
    (ctx.root / "summary.json").write_text(json.dumps(summary, indent=2))
    print(f"continue_pairs: scenario={a.scenario} {sum(c['ok'] for c in ctx.checks)}/{len(ctx.checks)} checks, "
          f"{summary['seconds']}s")
    print(f"continue_pairs: {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
