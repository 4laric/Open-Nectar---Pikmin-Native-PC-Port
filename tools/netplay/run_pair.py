"""Two-peer lockstep launcher for netplay M3 (issue #880).

Launches a host and a joiner as two hidden, private, bounded instances over
loopback UDP, each with its own scripted local input file (two different
gen_inputs.py seeds), the same bootstrap content, and the lossy impairment
env on both. Both peers run unthrottled (as fast as the session allows) for
tests and stop via PIKMIN_NETPLAY_EXIT_AFTER_TICKS.

Modelled on run_replay.py: junctioned read-only assets, a state.txt
refresher thread per run dir, the game run with the run dir as cwd
(settings are read relative to cwd), NECTAR_SAVE_DIR pointed at a private
dir, PIKMIN_RANDOMIZER_TEST_BACKGROUND=1 and SDL_AUDIODRIVER=dummy. Only
the spawned PIDs are ever signalled.

After both peers exit it compares the two hash logs with compare_hashes.py,
greps both stdout logs for desync / handshake-refused / disconnected lines,
and prints a summary.

Expectations (--expect):
  sync        both exit 0, identical hash logs, 0 desync lines (default)
  refuse      both exit 4 with a '[netplay] handshake refused:' line
  disconnect  joiner is killed mid-session; host exits 0 with a
              '[netplay] disconnected:' line
  desync      (B2 fix round 1) a day-end save barrier desync: both exit 5,
              both with a '[netplay] save barrier: ... mismatch' line
  barrier-timeout  (B2 fix round 1, with --env-host
              PIKMIN_NETPLAY_TEST_HOST_DIE_AT_BARRIER=1) the host exits 7 at
              its day-end save; the joiner exits 6 with '[netplay] save
              barrier timeout' and retracts its pending checkpoint
  --expect-hold N (M4 lane B1) implies sync and additionally requires
              exactly N '[netplay] hold at' / 'held at' / 'resume at'
              triples, identical on both peers (the per-peer held_ms is
              compared separately), and no 'disconnected:' line.

M4 lane B1 (issue #885) inputs: --bootstrap-template (both bootstraps from
one template, {TOKEN} = run token for SESSION and FINGERPRINT),
--host-stale-window START:SECONDS (the host refresher writes nothing in
[START, START+SECONDS) wall seconds), --host-state-missing-until SECONDS
(no host state.txt at all until then) and --host-session-json FILE (copied
to <out>/session.json, the runner ledger the host reads). The summary adds
the hold/resume lines, per-peer START_STAGE / [Pikmin Randomizer] /
distinct navi-piki-teki-item tuple counts and the journal/mirror files
present in each run dir.

M4 lane B2 (issue #885) sessions across day ends: --token HEX64 reuses a run
token (SESSION and FINGERPRINT; session 2 must reuse session 1's, otherwise
its checkpoint's fingerprint does not match); --run-name NAME puts the run
dirs at out/host/NAME and out/join/peer/NAME while the derived campaign dirs
stay out/campaign and out/join/campaign, so a second session reuses both
campaigns; --join-campaign-mode keep|clear|foreign:<file.sav>|newer prepares
the joiner's campaign before launch (clear moves it aside to
campaign.moved-<n>; foreign plants a checkpoint from another run; newer
plants a valid copy of the host's newest checkpoint re-stamped as the next
generation, header and hash included). The summary adds both peers'
checkpoint / transfer / save barrier / CAMPAIGN_RESUMED / reseed lines and
the distinct tuples after the day-3 reseed tick.
"""

import argparse
import os
import re
import shutil
import struct
import subprocess
import sys
import threading
import time
import uuid
from pathlib import Path

try:
    import _winapi
except ImportError:  # non-Windows: junctions unavailable; caller must symlink
    _winapi = None

DEFAULT_ASSETS = Path("C:/Users/alari/bbft/dist/cohesion/pikmin/assets")
HERE = Path(__file__).resolve().parent
GEN = HERE / "gen_inputs.py"
CMP = HERE / "compare_hashes.py"


def parse_kv(items, what):
    out = {}
    for item in items or []:
        if "=" not in item:
            raise SystemExit(f"--{what} needs key=value, got {item!r}")
        key, value = item.split("=", 1)
        out[key.strip()] = value.strip()
    return out


def fnv1a64(data):
    """pc_randomizer.cpp checkpointHash (FNV-1a 64)."""
    h = 14695981039346656037
    for byte in data:
        h ^= byte
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def restamp_checkpoint(src, dst, new_gen):
    """B2 --join-campaign-mode newer: a VALID checkpoint for new_gen: the
    header's generation field is replaced and the FNV-1a hash recomputed
    (a bare rename would be a header/name mismatch, which the joiner sets
    aside as stale instead of reporting a newer checkpoint)."""
    raw = Path(src).read_bytes()
    nl = raw.index(b"\n")
    header, block = raw[:nl].decode("ascii"), raw[nl + 1:]
    parts = header.split(" ")
    parts[2] = str(new_gen)
    body = " ".join(parts[:-1])
    h = fnv1a64(body.encode("ascii") + b"\n" + block)
    Path(dst).write_bytes((body + " " + str(h) + "\n").encode("ascii") + block)


def prepare_join_campaign(mode, host_campaign, join_campaign):
    """B2: prepare the joiner's campaign dir; returns a description."""
    if mode in (None, "keep"):
        return "keep"
    if mode == "clear":
        if not join_campaign.exists():
            return "clear (nothing to move)"
        n = 1
        while (join_campaign.parent / f"campaign.moved-{n}").exists():
            n += 1
        dest = join_campaign.parent / f"campaign.moved-{n}"
        join_campaign.rename(dest)
        return f"clear (moved to {dest.name})"
    if mode.startswith("foreign:"):
        src = Path(mode[len("foreign:"):])
        join_campaign.mkdir(parents=True, exist_ok=True)
        dst = join_campaign / src.name
        if dst.exists():
            n = 1
            while (join_campaign / f"{src.name}.harness-replaced-{n}").exists():
                n += 1
            dst.rename(join_campaign / f"{src.name}.harness-replaced-{n}")
        shutil.copyfile(str(src), str(dst))
        return f"foreign ({src} -> {dst.name})"
    if mode == "newer":
        savs = sorted(p for p in host_campaign.glob("*.sav") if re.match(r"^\d{20}\.sav$", p.name))
        if not savs:
            raise SystemExit("--join-campaign-mode newer: the host has no checkpoint")
        gen = int(savs[-1].name[:20]) + 1
        join_campaign.mkdir(parents=True, exist_ok=True)
        dst = join_campaign / f"{gen:020d}.sav"
        restamp_checkpoint(savs[-1], dst, gen)
        return f"newer ({savs[-1].name} re-stamped as {dst.name})"
    raise SystemExit(f"bad --join-campaign-mode {mode}")


def write_bootstrap(path, token, profile, flarlic=10):
    path.write_text(
        f"PIKMIN_RANDOMIZER 5\nSESSION {token}\nFINGERPRINT {token}\n"
        f"PROFILE {profile}\nCATALOG gameplay-checks-v5\nPLACEMENT identity-v1\n"
        f"GOAL 25\nDAYS repeat-day29-v1\nCOLOR red\nSTARTING_FLARLIC {flarlic}\nEND\n"
    )


def load_state_script(path, token):
    """M4a --host-state-script/--join-state-script loader (issue #885).

    Text file, one schedule entry per line: `<seconds> <PIKMIN_STATE ...>`.
    `{TOKEN}` in a line is replaced with the peer's run token. The first
    entry must be at t=0 (the refresher's initial content); later entries
    switch the hosted state.txt at that many wall seconds after the refresher
    starts. Blank lines and `#` comments are ignored.
    """
    sched = []
    for lineno, raw in enumerate(Path(path).read_text().splitlines(), 1):
        ln = raw.strip()
        if not ln or ln.startswith("#"):
            continue
        t, _, line = ln.partition(" ")
        try:
            ft = float(t)
        except ValueError:
            raise SystemExit(f"state script {path}:{lineno}: bad time {t!r}")
        if ft < 0 or not line.strip().startswith("PIKMIN_STATE"):
            raise SystemExit(
                f"state script {path}:{lineno}: want '<seconds> PIKMIN_STATE ...'")
        sched.append((ft, line.replace("{TOKEN}", token).strip() + "\n"))
    sched.sort(key=lambda e: e[0])
    if not sched or sched[0][0] != 0:
        raise SystemExit(f"state script {path}: first entry must be at t=0")
    return sched


def apply_lines(path):
    """Canonical `[netplay] randstate gen=<g> applied at frame=<F>` lines."""
    try:
        text = Path(path).read_text(errors="replace")
    except OSError:
        return []
    out = []
    for ln in text.splitlines():
        if "randstate gen=" in ln and "applied at frame=" in ln:
            out.append(ln[ln.index("randstate"):].strip())
    return out


def link_assets(run, assets):
    link = run / "assets"
    if not link.exists():
        if _winapi is not None:
            _winapi.CreateJunction(str(assets.resolve()), str(link))
        else:
            os.symlink(str(assets.resolve()), str(link), target_is_directory=True)


def gen_inputs(ticks, seed, out):
    r = subprocess.run(
        [sys.executable, str(GEN), "--ticks", str(ticks), "--seed", str(seed),
         "--out", str(out)],
        capture_output=True, text=True,
    )
    if r.returncode != 0:
        raise SystemExit(f"gen_inputs failed: {r.stderr[-2000:]}")


PKNI_HEADER = 10  # magic(4) + version/pad count/record size (3 x u16 LE)


def neutralize_after(path, keep):
    """M4 B1 fix round 1: keep the first `keep` records of a v2 .pkni as
    generated and make pad 0 hands-off afterwards (buttons, both sticks,
    triggers and analog A/B zeroed; the connected byte and control yaw stay
    as generated). Returns (records, neutralized)."""
    data = bytearray(Path(path).read_bytes())
    if data[:4] != b"PKNI":
        raise SystemExit(f"neutralize_after: {path} is not a PKNI file")
    version, pads, size = struct.unpack_from("<HHH", data, 4)
    if version != 2 or pads != 4 or size != 56:
        raise SystemExit(f"neutralize_after: unsupported PKNI v{version} pads={pads} size={size}")
    records = (len(data) - PKNI_HEADER) // size
    changed = 0
    for i in range(max(0, keep), records):
        off = PKNI_HEADER + i * size  # pad 0 is the first 14 bytes
        data[off:off + 10] = bytes(10)
        changed += 1
    Path(path).write_bytes(bytes(data))
    return records, changed


SCRUB_KEYS = (
    "PIKMIN_INPUT_RECORD",
    "PIKMIN_INPUT_REPLAY",
    "PIKMIN_STATE_HASH_LOG",
    "PIKMIN_NETPLAY_EXIT_AFTER_TICKS",
    "PIKMIN_NETPLAY_TEST_PREROLL_RAND",
    "PIKMIN_NETPLAY_DETERMINISTIC",
    "PIKMIN_NETPLAY_UNTHROTTLED",
    "PIKMIN_NETPLAY_TEST_CAMERA_WOBBLE",
    "PIKMIN_NETPLAY_DEBUG_NAVI_POS",
    "PIKMIN_NETPLAY_HOST",
    "PIKMIN_NETPLAY_JOIN",
    "PIKMIN_NETPLAY_DELAY",
    "PIKMIN_NETPLAY_SEED",
    "PIKMIN_NETPLAY_LOCAL_INPUT_FILE",
    "PIKMIN_NETPLAY_TEST_LATENCY_MS",
    "PIKMIN_NETPLAY_TEST_JITTER_MS",
    "PIKMIN_NETPLAY_TEST_LOSS_PCT",
    "PIKMIN_NETPLAY_TEST_SEED",
    "PIKMIN_NETPLAY_TEST_REORDER_PCT",
    "PIKMIN_NETPLAY_TEST_REORDER_MS",
    "PIKMIN_NETPLAY_HANDSHAKE_TIMEOUT_MS",
    "PIKMIN_NETPLAY_TEST_DROP_HS_FIRST_N",
    "PIKMIN_NETPLAY_TEST_DROP_FINAL_ACK",
    "PIKMIN_NETPLAY_DISCONNECT_MS",
    "PIKMIN_NETPLAY_TEST_LOAD_DELAY_MS",
    "PIKMIN_NETPLAY_TEST_SCRIPT_VIA_ACCUM",
    "PIKMIN_NETPLAY_RANDSTATE_STREAM",
    "PIKMIN_NETPLAY_TEST_DEATHLINK_AS_ORDINARY",
    "PIKMIN_NETPLAY_TEST_HOST_SAVE_FAIL",
    "PIKMIN_NETPLAY_TEST_TAMPER_SIDECARS",
    "PIKMIN_NETPLAY_TEST_BARRIER_CORRUPT",
    "PIKMIN_NETPLAY_TEST_HOST_DIE_AT_BARRIER",
    "PIKMIN_NETPLAY_TEST_BULK_DROP_SAVE",
    "NECTAR_CARD_DEBUG",
)


def launch(exe, run, boot, extra_args, env_extra, stdout_log, unthrottled=True):
    cmd = [str(exe.resolve()), "--randomizer-seed", str(boot)] + list(extra_args)
    env = dict(os.environ)
    for key in SCRUB_KEYS:
        env.pop(key, None)
    env.update(
        PIKMIN_RANDOMIZER_TEST_BACKGROUND="1",
        SDL_AUDIODRIVER="dummy",
        NECTAR_SAVE_DIR=str((run / "save").resolve()),
    )
    # M5: throttled runs (the path humans use: 30 Hz pacing + the
    # frames-ahead throttle) set PIKMIN_NETPLAY_UNTHROTTLED=0 explicitly.
    env["PIKMIN_NETPLAY_UNTHROTTLED"] = "1" if unthrottled else "0"
    env.update(env_extra)
    env.pop("BBFT_PORT", None)
    startup = None
    if sys.platform == "win32":
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
    child_out = open(stdout_log, "w")
    proc = subprocess.Popen(
        cmd, cwd=str(run), env=env, startupinfo=startup,
        stdout=child_out, stderr=subprocess.STDOUT,
    )
    return proc, child_out


def hold_lines(path):
    """M4 lane B1: (hold, held, resume, held_ms) from one native log.

    resume lines are canonicalised without their per-peer ` held_ms=<ms>`
    suffix (wall time differs per peer); the ms values come back separately.
    """
    try:
        text = Path(path).read_text(errors="replace")
    except OSError:
        return [], [], [], []
    hold, held, resume, ms = [], [], [], []
    for ln in text.splitlines():
        if "[netplay] hold at frame=" in ln:
            hold.append(ln[ln.index("hold at"):].strip())
        elif "[netplay] held at frame=" in ln:
            held.append(ln[ln.index("held at"):].strip())
        elif "[netplay] resume at frame=" in ln:
            body = ln[ln.index("resume at"):].strip()
            if " held_ms=" in body:
                body, _, val = body.partition(" held_ms=")
                try:
                    ms.append(float(val))
                except ValueError:
                    ms.append(-1.0)
            resume.append(body)
    return hold, held, resume, ms


def hash_tuples(path, lo=None, hi=None):
    """Distinct (navi, piki, teki, item) tuples of a hash log, i.e.
    awk '{print $3,$4,$5,$6}' hashes.txt | sort -u | wc -l, optionally
    restricted to ticks lo <= tick <= hi."""
    seen = set()
    try:
        with open(path, "r", errors="replace") as f:
            for line in f:
                cols = line.split()
                if len(cols) < 6:
                    continue
                try:
                    tick = int(cols[0])
                except ValueError:
                    continue
                if lo is not None and tick < lo:
                    continue
                if hi is not None and tick > hi:
                    continue
                seen.add(tuple(cols[2:6]))
    except OSError:
        return 0
    return len(seen)


def frame_of(line):
    """Integer after 'frame=' in a canonical hold/held/resume line."""
    try:
        return int(line.split("frame=", 1)[1].split()[0])
    except (IndexError, ValueError):
        return None


def count_lines(path):
    try:
        with open(path, "r", errors="replace") as f:
            return sum(1 for line in f if line.strip())
    except OSError:
        return 0


def grep(path, needle):
    try:
        text = Path(path).read_text(errors="replace")
    except OSError:
        return []
    return [ln for ln in text.splitlines() if needle in ln]


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--exe", type=Path, required=True, help="netplay game executable (host)")
    p.add_argument("--exe-b", type=Path, default=None, help="joiner executable override (exe negative test)")
    p.add_argument("--ticks", type=int, required=True)
    p.add_argument("--out", type=Path, required=True, help="private output dir (host/ + join/ below)")
    p.add_argument("--assets", type=Path, default=DEFAULT_ASSETS)
    p.add_argument("--profile", default="foh-day2")
    p.add_argument("--host-port", type=int, default=5077)
    p.add_argument("--seed-a", type=int, default=101, help="host local-input gen seed")
    p.add_argument("--seed-b", type=int, default=202, help="joiner local-input gen seed")
    p.add_argument("--netplay-seed", type=int, default=1, help="PIKMIN_NETPLAY_SEED for both")
    p.add_argument("--delay", type=str, default="2",
                   help="PIKMIN_NETPLAY_DELAY for both (frames, or 'auto')")
    p.add_argument("--delay-host", type=str, default=None,
                   help="PIKMIN_NETPLAY_DELAY for the host only (asymmetric test)")
    p.add_argument("--delay-join", type=str, default=None,
                   help="PIKMIN_NETPLAY_DELAY for the joiner only (asymmetric test)")
    p.add_argument("--latency-ms", type=float, default=0.0)
    p.add_argument("--jitter-ms", type=float, default=0.0)
    p.add_argument("--loss-pct", type=float, default=0.0)
    p.add_argument("--impair-seed", type=int, default=7)
    p.add_argument("--timeout", type=float, default=900)
    p.add_argument("--handshake-timeout-ms", type=int, default=30000)
    p.add_argument("--env", nargs="*", default=[], metavar="K=V", help="extra env for both peers")
    p.add_argument("--env-host", nargs="*", default=[], metavar="K=V")
    p.add_argument("--env-join", nargs="*", default=[], metavar="K=V")
    p.add_argument("--config-overrides", nargs="*", default=[], metavar="key=value",
                   help="private pikmin_settings.conf for both peers")
    p.add_argument("--config-overrides-join", nargs="*", default=[], metavar="key=value",
                   help="extra private settings for the joiner only (settings negative test)")
    p.add_argument("--bootstrap-b", type=Path, default=None,
                   help="joiner bootstrap override file (bootstrap negative test)")
    p.add_argument("--throttled", action="store_true",
                   help="run the real-time 30 Hz path (no UNTHROTTLED); M5 evidence")
    p.add_argument("--no-restamp-session", action="store_true",
                   help="keep --bootstrap-b bytes verbatim (m8 SESSION-strip positive test)")
    p.add_argument("--session-only-difference", action="store_true",
                   help="m8 positive test: join bootstrap differs from the host only in "
                        "SESSION (same FINGERPRINT); the handshake must succeed")
    p.add_argument("--exe-args", nargs="*", default=[])
    p.add_argument("--expect", choices=("sync", "refuse", "disconnect", "desync", "barrier-timeout"),
                   default="sync")
    p.add_argument("--kill-joiner-after", type=float, default=20.0,
                   help="disconnect test: seconds after start to kill the joiner")
    p.add_argument("--flarlic", type=int, default=10,
                   help="STARTING_FLARLIC in both bootstraps (M4a: <10 allows a "
                        "flarlic change in a state script)")
    p.add_argument("--host-state-script", type=Path, default=None,
                   help="M4a: schedule file for the host state.txt refresher "
                        "(lines: '<seconds> PIKMIN_STATE ...', {TOKEN} = run token)")
    p.add_argument("--join-state-script", type=Path, default=None,
                   help="M4a: schedule file for the joiner state.txt refresher "
                        "(negative control: a deliberately different schedule)")
    p.add_argument("--bootstrap-template", type=Path, default=None,
                   help="M4 B1: both peers' bootstrap from this template; {TOKEN} "
                        "becomes the run token (SESSION and FINGERPRINT)")
    p.add_argument("--host-stale-window", type=str, default=None, metavar="START:SECONDS",
                   help="M4 B1: the host state.txt refresher writes nothing during "
                        "[START, START+SECONDS) wall seconds (link goes stale -> HOLD)")
    p.add_argument("--host-state-missing-until", type=float, default=None, metavar="SECONDS",
                   help="M4 B1: the host has no state.txt at all until SECONDS "
                        "(any existing file is removed first)")
    p.add_argument("--host-session-json", type=Path, default=None,
                   help="M4 B1: copied to parent^2(host run)/session.json, i.e. "
                        "<out>/session.json (the runner mirror ledger)")
    p.add_argument("--expect-hold", type=int, default=None, metavar="N",
                   help="M4 B1: implies sync; exactly N hold/held/resume triples, "
                        "identical on both peers, and 0 disconnected lines")
    p.add_argument("--expect-held-ms", type=float, default=None, metavar="MIN",
                   help="M4 B1 fix round 1: with --expect-hold, every resume line's "
                        "held_ms (hold at -> resume, per peer) must be >= MIN")
    p.add_argument("--min-tuples", type=int, default=None, metavar="N",
                   help="M4 B1 fix round 1: with --expect-hold, each peer's distinct "
                        "tuples after each resume frame, and before each hold frame "
                        "when that frame exceeds N, must exceed N; otherwise each "
                        "peer's total must exceed N")
    p.add_argument("--expect-no-hold", action="store_true",
                   help="M4 B1 fix round 1 (negative control): implies sync; no hold, "
                        "held or resume line on either peer")
    p.add_argument("--host-script-ticks", type=int, default=None, metavar="N",
                   help="M4 B1 fix round 1: the host's scripted pad 0 plays the seed-a "
                        "records for the first N submits, then stays hands-off")
    p.add_argument("--join-script-ticks", type=int, default=None, metavar="N",
                   help="M4 B1 fix round 1: the same for the joiner (seed b); 0 = "
                        "hands-off from the first submit")
    p.add_argument("--token", type=str, default=None, metavar="HEX64",
                   help="M4 B2: reuse this run token (SESSION and FINGERPRINT); default random, printed")
    p.add_argument("--run-name", type=str, default="run",
                   help="M4 B2: run dirs out/host/NAME and out/join/peer/NAME (the campaigns stay "
                        "out/campaign and out/join/campaign)")
    p.add_argument("--join-campaign-mode", type=str, default="keep",
                   help="M4 B2: keep | clear | foreign:<file.sav> | newer (see the module docstring)")
    a = p.parse_args(argv)
    if a.token is not None and not re.match(r"^[0-9a-f]{64}$", a.token):
        raise SystemExit("--token wants 64 lowercase hex characters")
    if not re.match(r"^[A-Za-z0-9._-]+$", a.run_name):
        raise SystemExit("--run-name wants a plain folder name")
    if a.expect_hold is not None or a.expect_no_hold:
        a.expect = "sync"
    stale_window = None
    if a.host_stale_window:
        try:
            st0, _, dur = a.host_stale_window.partition(":")
            stale_window = (float(st0), float(st0) + float(dur))
        except ValueError:
            raise SystemExit("--host-stale-window wants START:SECONDS")

    out = a.out.resolve()
    # Slice lane (issue #880): each peer gets a per-peer campaign dir.
    # pc_randomizer.cpp derives it as bootstrap-grandparent + "campaign", so
    # the run dirs sit at different depths to force different grandparents:
    # parent^2(out/host/run) is out, while parent^2(out/join/peer/run) is
    # out/join. Day-end writes a
    # card save and a campaign checkpoint from inside the sim on both peers
    # at the same tick; sharing one campaign dir makes the two writers
    # collide on the same checkpoint file (filesystem rename error, the loser
    # terminates and the pair disconnects). In production the peers are on
    # different machines; per-peer dirs are the faithful layout, and they
    # make the post-test save comparison meaningful.
    host_run = out / "host" / a.run_name
    join_run = out / "join" / "peer" / a.run_name
    host_run.mkdir(parents=True, exist_ok=True)
    join_run.mkdir(parents=True, exist_ok=True)
    # B2: prepare the joiner's derived campaign dir (parent^2(join run)).
    campaign_note = prepare_join_campaign(a.join_campaign_mode, out / "campaign", out / "join" / "campaign")
    print(f"run_pair: join campaign: {campaign_note}")

    token = a.token if a.token is not None else uuid.uuid4().hex * 2
    print(f"run_pair: token {token}")
    token_join = token
    host_boot = host_run / "bootstrap.txt"
    template = None
    if a.bootstrap_template is not None:
        template = a.bootstrap_template.read_text().replace("{TOKEN}", token)
        host_boot.write_text(template)
    else:
        write_bootstrap(host_boot, token, a.profile, a.flarlic)
    join_boot = join_run / "bootstrap.txt"
    if template is not None and a.bootstrap_b is None:
        join_boot.write_text(template)
    elif a.bootstrap_b is not None:
        shutil.copyfile(str(a.bootstrap_b.resolve()), str(join_boot))
        if not a.no_restamp_session:
            # Re-stamp the per-run SESSION token to this run's token (the
            # handshake hash strips SESSION lines by design, so the manifest
            # difference under test is preserved while state.txt admission,
            # which compares the session token, keeps working).
            lines = join_boot.read_text().splitlines()
            lines = [f"SESSION {token}" if ln.startswith("SESSION ") else ln for ln in lines]
            join_boot.write_text("\n".join(lines) + "\n")
        else:
            # m8: keep --bootstrap-b bytes verbatim AND stamp the joiner's
            # state.txt with the join bootstrap's own SESSION, so local
            # admission passes on both sides while the handshake must still
            # succeed on the stripped hash (SESSION differs, all else same).
            for ln in join_boot.read_text().splitlines():
                if ln.startswith("SESSION "):
                    token_join = ln[len("SESSION "):].strip()
                    break
    else:
        write_bootstrap(join_boot, token, a.profile, a.flarlic)
    if a.session_only_difference:
        # m8 positive test: same manifest, different per-run SESSION token.
        # The joiner's state.txt (below) uses token_join so local admission
        # passes; the handshake strips SESSION, so it must succeed.
        token_join = uuid.uuid4().hex * 2
        assert token_join != token
        join_boot.write_text(
            f"PIKMIN_RANDOMIZER 5\nSESSION {token_join}\nFINGERPRINT {token}\n"
            f"PROFILE {a.profile}\nCATALOG gameplay-checks-v5\nPLACEMENT identity-v1\n"
            f"GOAL 25\nDAYS repeat-day29-v1\nCOLOR red\nSTARTING_FLARLIC 10\nEND\n"
        )

    link_assets(host_run, a.assets)
    link_assets(join_run, a.assets)

    overrides = parse_kv(a.config_overrides, "config-overrides")
    if overrides:
        for run in (host_run, join_run):
            with open(run / "pikmin_settings.conf", "w") as f:
                for key, value in sorted(overrides.items()):
                    f.write(f"{key} = {value}\n")
    join_extra_cfg = parse_kv(a.config_overrides_join, "config-overrides-join")
    if join_extra_cfg:
        with open(join_run / "pikmin_settings.conf", "a") as f:
            for key, value in sorted(join_extra_cfg.items()):
                f.write(f"{key} = {value}\n")

    for run in (host_run, join_run):
        (run / "save").mkdir(exist_ok=True)
    if a.host_session_json is not None:
        # B1: the host reads the runner ledger at parent^2(run)/session.json.
        shutil.copyfile(str(a.host_session_json.resolve()), str(host_run.parent.parent / "session.json"))
    if a.host_state_missing_until is not None:
        try:
            (host_run / "state.txt").unlink()
        except FileNotFoundError:
            pass

    # Per-peer scripted local inputs (brief: two different seeds).
    tag = "" if a.run_name == "run" else a.run_name + "_"
    host_inputs = out / f"{tag}host_inputs.pkni"
    join_inputs = out / f"{tag}join_inputs.pkni"
    gen_inputs(a.ticks + 50, a.seed_a, host_inputs)
    gen_inputs(a.ticks + 50, a.seed_b, join_inputs)
    for who, keep, path in (("host", a.host_script_ticks, host_inputs),
                            ("join", a.join_script_ticks, join_inputs)):
        if keep is not None:
            records, changed = neutralize_after(path, keep)
            print(f"run_pair: {who} inputs: first {min(keep, records)} of {records} records "
                  f"scripted, {changed} hands-off")

    host_hash = host_run / "hashes.txt"
    join_hash = join_run / "hashes.txt"
    host_log = host_run / "native.log"
    join_log = join_run / "native.log"

    impair = {
        "PIKMIN_NETPLAY_TEST_LATENCY_MS": str(a.latency_ms),
        "PIKMIN_NETPLAY_TEST_JITTER_MS": str(a.jitter_ms),
        "PIKMIN_NETPLAY_TEST_LOSS_PCT": str(a.loss_pct),
        "PIKMIN_NETPLAY_TEST_SEED": str(a.impair_seed),
    }
    base_extra = {
        "PIKMIN_NETPLAY_SEED": str(a.netplay_seed),
        "PIKMIN_NETPLAY_DELAY": str(a.delay),
        "PIKMIN_NETPLAY_EXIT_AFTER_TICKS": str(a.ticks),
        "PIKMIN_NETPLAY_HANDSHAKE_TIMEOUT_MS": str(a.handshake_timeout_ms),
        "PIKMIN_STATE_HASH_LOG": "",  # replaced per peer below
    }
    base_extra.update(impair)
    base_extra.update(parse_kv(a.env, "env"))

    host_extra = dict(base_extra)
    host_extra["PIKMIN_STATE_HASH_LOG"] = str(host_hash)
    host_extra["PIKMIN_NETPLAY_LOCAL_INPUT_FILE"] = str(host_inputs.resolve())
    if a.delay_host is not None:
        host_extra["PIKMIN_NETPLAY_DELAY"] = str(a.delay_host)
    host_extra.update(parse_kv(a.env_host, "env-host"))
    join_extra = dict(base_extra)
    join_extra["PIKMIN_STATE_HASH_LOG"] = str(join_hash)
    join_extra["PIKMIN_NETPLAY_LOCAL_INPUT_FILE"] = str(join_inputs.resolve())
    if a.delay_join is not None:
        join_extra["PIKMIN_NETPLAY_DELAY"] = str(a.delay_join)
    join_extra.update(parse_kv(a.env_join, "env-join"))

    join_exe = a.exe_b.resolve() if a.exe_b is not None else a.exe.resolve()
    host_args = ["--netplay-host", str(a.host_port)] + list(a.exe_args)
    join_args = ["--netplay-join", f"127.0.0.1:{a.host_port}"] + list(a.exe_args)

    stop = threading.Event()

    def default_sched(tok):
        return [(0.0, f"PIKMIN_STATE 5 {tok} 1 0 127 0 0 END\n")]

    sched_host = load_state_script(a.host_state_script, token) if a.host_state_script else default_sched(token)
    sched_join = load_state_script(a.join_state_script, token_join) if a.join_state_script else default_sched(token_join)

    t0 = time.time()

    def refresh_sched(run, sched, host=False):
        while not stop.is_set():
            el = time.time() - t0
            if host and a.host_state_missing_until is not None and el < a.host_state_missing_until:
                stop.wait(0.1)  # B1: no host state.txt at all yet
                continue
            if host and stale_window is not None and stale_window[0] <= el < stale_window[1]:
                stop.wait(0.1)  # B1: stale link window, nothing written
                continue
            cur = sched[0][1]
            for ft, line in sched:
                if ft <= el:
                    cur = line
                else:
                    break
            pending = run / "state.tmp"
            try:
                pending.write_text(cur)
                os.replace(pending, run / "state.txt")
            except OSError:
                pass
            stop.wait(0.1)

    threads = [threading.Thread(target=refresh_sched, args=(host_run, sched_host, True)),
               threading.Thread(target=refresh_sched, args=(join_run, sched_join, False))]
    for t in threads:
        t.start()

    rc_host, rc_join = 1, 1
    host_proc = join_proc = None
    host_out = join_out = None
    start = time.time()
    try:
        host_proc, host_out = launch(a.exe, host_run, host_boot, host_args, host_extra, host_log,
                                       unthrottled=not a.throttled)
        # Stagger the joiner slightly so the host's socket is bound first.
        time.sleep(1.0)
        join_proc, join_out = launch(join_exe, join_run, join_boot, join_args, join_extra, join_log,
                                     unthrottled=not a.throttled)
        if a.expect == "disconnect":
            time.sleep(a.kill_joiner_after)
            if join_proc.poll() is None:
                print(f"run_pair: killing joiner pid {join_proc.pid} for disconnect test")
                join_proc.kill()
            try:
                rc_join = join_proc.wait(timeout=30)
            except subprocess.TimeoutExpired:
                rc_join = 124
            try:
                rc_host = host_proc.wait(timeout=a.timeout)
            except subprocess.TimeoutExpired:
                host_proc.kill()
                rc_host = 124
        else:
            try:
                rc_host = host_proc.wait(timeout=a.timeout)
            except subprocess.TimeoutExpired:
                host_proc.kill()
                try:
                    rc_host = host_proc.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    rc_host = 124
                print(f"run_pair: host timeout after {a.timeout}s, killed pid {host_proc.pid}")
            try:
                rc_join = join_proc.wait(timeout=60)
            except subprocess.TimeoutExpired:
                join_proc.kill()
                try:
                    rc_join = join_proc.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    rc_join = 124
                print("run_pair: joiner timeout, killed pid "
                      f"{join_proc.pid if join_proc else '?'}")
    finally:
        stop.set()
        for t in threads:
            t.join()
        for f in (host_out, join_out):
            try:
                if f is not None:
                    f.close()
            except OSError:
                pass
        for proc in (host_proc, join_proc):
            try:
                if proc is not None and proc.poll() is None:
                    proc.kill()
            except OSError:
                pass
    secs = time.time() - start

    n_host = count_lines(host_hash)
    n_join = count_lines(join_hash)
    cmp_rc, cmp_tail = None, ""
    if a.expect == "sync":
        r = subprocess.run([sys.executable, str(CMP), str(host_hash), str(join_hash)],
                           capture_output=True, text=True)
        cmp_rc = r.returncode
        cmp_tail = (r.stdout + r.stderr).strip().splitlines()
        cmp_tail = cmp_tail[-1] if cmp_tail else ""
    des_host = grep(host_log, "desync detected")
    des_join = grep(join_log, "desync detected")
    ref_host = grep(host_log, "handshake refused")
    ref_join = grep(join_log, "handshake refused")
    dis_host = grep(host_log, "disconnected:")
    dis_join = grep(join_log, "disconnected:")
    # M4a same-frame apply pair: the canonical randstate apply lines must be
    # identical on both peers (same gens at the same frames).
    app_host = apply_lines(host_log)
    app_join = apply_lines(join_log)

    print(f"run_pair: expect={a.expect} ticks={a.ticks} time={secs:.1f}s")
    print(f"run_pair: host exit={rc_host} hashes={n_host} log={host_log}")
    print(f"run_pair: join exit={rc_join} hashes={n_join} log={join_log}")
    if a.expect == "sync":
        print(f"run_pair: compare exit={cmp_rc}: {cmp_tail}")
    print(f"run_pair: desync lines host={len(des_host)} join={len(des_join)}")
    print(f"run_pair: refused lines host={len(ref_host)} join={len(ref_join)}")
    print(f"run_pair: disconnected lines host={len(dis_host)}")
    print(f"run_pair: randstate applies host={len(app_host)} join={len(app_join)}")
    for ln in app_host[:12]:
        print(f"run_pair: apply host: {ln}")
    for ln in app_join[:12]:
        print(f"run_pair: apply join: {ln}")
    # M4 lane B1 summary: hold/resume lines, gameplay proof, run-dir files.
    hh = hold_lines(host_log)
    hj = hold_lines(join_log)
    print(f"run_pair: disconnected lines join={len(dis_join)}")
    for who, (hold, held, resume, ms) in (("host", hh), ("join", hj)):
        for ln in hold + held + resume:
            print(f"run_pair: {who}: {ln}")
        if ms:
            print(f"run_pair: {who}: held_ms={','.join(f'{v:.0f}' for v in ms)}")
    # Frozen time (held at -> resume, all holds) from the exit line's held=<ms>.
    for who, log in (("host", host_log), ("join", join_log)):
        vals = [ln.split(" held=")[1].split("ms")[0] for ln in grep(log, " held=")
                if " holds=" in ln]
        if vals:
            print(f"run_pair: {who}: frozen held={vals[-1]}ms (exit line)")
    tuple_fail = []
    for who, log, hashes, run in (("host", host_log, host_hash, host_run),
                                  ("join", join_log, join_hash, join_run)):
        starts = len(grep(log, "START_STAGE"))
        rand = len(grep(log, "[Pikmin Randomizer]"))
        tuples = hash_tuples(hashes)
        files = [n for n in ("checks.txt", "deaths.txt", "emperor.txt", "benefits-used.txt",
                             "mirror-events.txt") if (run / n).exists()]
        print(f"run_pair: {who}: START_STAGE={starts} randomizer_lines={rand} "
              f"distinct_tuples={tuples} files={','.join(files) if files else '-'}")
        if a.min_tuples is not None and not hh[0] and tuples <= a.min_tuples:
            tuple_fail.append(f"{who} distinct tuples {tuples} <= {a.min_tuples}")
        for hold, resume in zip(hh[0], hh[2]):
            hf, rf = frame_of(hold), frame_of(resume)
            if hf is not None and rf is not None:
                # tick = frame + 1: ticks <= H ran before the hold frame,
                # ticks > R ran from the resume frame on.
                pre = hash_tuples(hashes, hi=hf)
                post = hash_tuples(hashes, lo=rf + 1)
                print(f"run_pair: {who}: tuples before hold frame {hf}={pre} "
                      f"after resume frame {rf}={post}")
                # Before the hold only when the hold frame leaves room for
                # more than N distinct tuples (a missing-state HOLD at frame
                # 4 cannot have any gameplay before it).
                pre_due = a.min_tuples is not None and hf > a.min_tuples
                if a.min_tuples is not None and ((pre_due and pre <= a.min_tuples) or post <= a.min_tuples):
                    tuple_fail.append(f"{who} tuples before {hf}={pre} / after {rf}={post} "
                                      f"not both > {a.min_tuples}")

    # B2 summary: checkpoint decision, transfer, save barrier, resume, reseed.
    b2_needles = ("[netplay] checkpoint", "[netplay] transfer", "[netplay] save barrier",
                  "CAMPAIGN_RESUMED", "CAMPAIGN_SAVED", "[netplay] local campaign checkpoint is stale",
                  "[netplay] set aside", "[netplay] local checkpoint:", "reseed day=", "START_STAGE",
                  "handshake refused", "[netplay] bulk impairment", "[netplay] test:", "[netplay] desync",
                  "[netplay] p2 ", "[netplay] sidecars")
    for who, log, hashes in (("host", host_log, host_hash), ("join", join_log, join_hash)):
        try:
            text = Path(log).read_text(errors="replace").splitlines()
        except OSError:
            text = []
        for ln in text:
            if any(n in ln for n in b2_needles):
                print(f"run_pair: {who}: {ln.strip()}")
        for ln in text:
            m = re.search(r"reseed day=(\d+) .*tick=(\d+)", ln)
            if m:
                after = hash_tuples(hashes, lo=int(m.group(2)) + 1)
                print(f"run_pair: {who}: distinct tuples after the day-{m.group(1)} reseed tick "
                      f"{m.group(2)}: {after}")

    ok = True
    if a.expect == "sync":
        if rc_host != 0 or rc_join != 0:
            ok = False
        if cmp_rc != 0:
            ok = False
        if des_host or des_join:
            ok = False
        if app_host != app_join:
            print("run_pair: FAIL: randstate apply lines differ between peers")
            ok = False
        if n_host != a.ticks or n_join != a.ticks:
            print(f"run_pair: FAIL: hash lines {n_host}/{n_join} != requested {a.ticks}")
            ok = False
        if a.expect_hold is not None:
            n = a.expect_hold
            for who, (hold, held, resume, _ms) in (("host", hh), ("join", hj)):
                if len(hold) != n or len(held) != n or len(resume) != n:
                    print(f"run_pair: FAIL: {who} hold/held/resume counts "
                          f"{len(hold)}/{len(held)}/{len(resume)} != {n}")
                    ok = False
            if hh[:3] != hj[:3]:
                print("run_pair: FAIL: hold/held/resume lines differ between peers")
                ok = False
            if dis_host or dis_join:
                print("run_pair: FAIL: disconnected lines present")
                ok = False
            if a.expect_held_ms is not None:
                for who, (_h, _d, _r, ms) in (("host", hh), ("join", hj)):
                    if not ms or min(ms) < a.expect_held_ms:
                        print(f"run_pair: FAIL: {who} held_ms {ms} not all >= {a.expect_held_ms:.0f}")
                        ok = False
        if a.expect_no_hold:
            for who, (hold, held, resume, _ms) in (("host", hh), ("join", hj)):
                if hold or held or resume:
                    print(f"run_pair: FAIL: {who} has hold/held/resume lines "
                          f"({len(hold)}/{len(held)}/{len(resume)}) in a negative control")
                    ok = False
        for msg in tuple_fail:
            print(f"run_pair: FAIL: {msg}")
            ok = False
    elif a.expect == "refuse":
        if rc_host != 4 or rc_join != 4:
            print(f"run_pair: FAIL: expected both exit 4, got {rc_host}/{rc_join}")
            ok = False
        if not ref_host or not ref_join:
            print("run_pair: FAIL: expected refused lines on both peers")
            ok = False
    elif a.expect == "disconnect":
        if rc_host != 0:
            print(f"run_pair: FAIL: expected host exit 0, got {rc_host}")
            ok = False
        if not dis_host:
            print("run_pair: FAIL: expected a disconnected line on the host")
            ok = False
    elif a.expect == "desync":
        # B2 fix round 1 (C1): a barrier desync must end BOTH peers with exit 5.
        if rc_host != 5 or rc_join != 5:
            print(f"run_pair: FAIL: expected both exit 5, got {rc_host}/{rc_join}")
            ok = False
        for who, log in (("host", host_log), ("join", join_log)):
            if not [ln for ln in grep(log, "[netplay] save barrier:") if "mismatch" in ln]:
                print(f"run_pair: FAIL: no save barrier mismatch line on the {who}")
                ok = False
    elif a.expect == "barrier-timeout":
        # B2 fix round 1 (C2): the host dies at its day-end save (test knob,
        # exit 7); the joiner's barrier times out (exit 6) and retracts.
        if rc_host != 7 or rc_join != 6:
            print(f"run_pair: FAIL: expected host exit 7 and joiner exit 6, got {rc_host}/{rc_join}")
            ok = False
        if not grep(join_log, "[netplay] save barrier timeout"):
            print("run_pair: FAIL: no save barrier timeout line on the joiner")
            ok = False
    print(f"run_pair: {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
