# Netplay co-op: how to play (one copy-paste code each way)

Two humans can play a co-op session with one copy-paste code in each
direction and no environment variables. One human testing alone can do the
same with two windows on one PC (see below). There is no hosted signalling
or TURN service: the owner decision is *connection codes only*.

## What both people need

- The **same netplay build** (`nectar.exe`). If the builds differ, the
  handshake refuses with `[netplay] handshake refused: exe` on both sides.
  (The check hashes the running exe, so even a one-byte difference refuses.)
- Their **own game files**. The exe finds them relative to the directory
  you start it from (its working directory):
  - `assets/` — the game data (pikmin assets). Point it at your install's
    assets (a junction/symlink named `assets` works: it only reads).
  - `pikmin_settings.conf` — your settings, read from the working
    directory. Only the host's *sim-relevant* settings matter for the
    session (see below); window size, audio volume and keybinds always stay
    local to each player.
  - Saves: each launcher run gets a **private** memory card under its run
    dir (see below), so your own saves are never touched.
- A network path between the two PCs. Direct connections use ICE host
  candidates; if both sides are behind symmetric NATs, see Troubleshooting.

## Host steps

1. Start the game from the folder with your `assets/`:
   `nectar.exe --netplay-host-ice`
   - With no `--bootstrap`, the host starts a default new-game setup (the
     `tools/netplay/run_pair.py` profile `foh-day2`). To play a real
     randomizer seed, pass its bootstrap file:
     `nectar.exe --netplay-host-ice --bootstrap <seed-bootstrap.txt>`
2. The game creates a private run dir `netplay/run-<date>-pid<id>/` next to
   the exe (bootstrap copy, settings copy, private `save/`, logs of both
   codes) and prints a one-line **offer code**, which it also copies to the
   clipboard.
3. Send that offer code to the joiner (chat, DM, anything).
4. Paste the joiner's **answer code** into the game window's console
   (standard input) and press Enter. (Alternative for scripted setups: set
   `PIKMIN_NETPLAY_ICE_ANSWER_IN` to a file holding the answer, or pass
   `--netplay-answer-in <file>`.)
5. Both sides wait for `[netplay] ice completed in ...ms`, then the session
   starts. Delay defaults to `auto` (measured from the handshake).

## Joiner steps

1. Start the game from the folder with your `assets/`:
   `nectar.exe --netplay-join-ice <offer-code>`
   - The code can also come from a file (`--netplay-join-ice @file.txt`)
     or straight from the clipboard (`--netplay-join-ice @clipboard`).
   - **You need no file from the host.** The offer carries the session
     setup: the netplay seed, the host's sim-relevant settings (adopted for
     this session only — your `pikmin_settings.conf` on disk is never
     changed), and the host's bootstrap bytes (written into your private
     run dir with a fresh per-run SESSION line, exactly as the pair tools
     do). Window size, volume and keybinds stay yours.
2. The game prints a one-line **answer code** (also copied to the
   clipboard) and, with `--netplay-code-out <file>`, to a file. Send it
   back to the host.
3. Wait for `[netplay] ice completed in ...ms`; the session starts.

## Local two-window test (one PC, one human)

Visible run (you play it):

```
tools\netplay\play_local.bat -Exe <path\to\nectar.exe>
```

It starts a visible host (keyboard) and a visible joiner (first gamepad) on
loopback over ICE host candidates, moving the codes between them
automatically through files. Close both windows to finish. With
`-Bootstrap <file>` the host uses that seed's bootstrap. Input mapping:
host `--netplay-input keyboard`, joiner `--netplay-input gamepad:0`; see
`--netplay-input keyboard|gamepad[:N]|auto` (`auto` = today's behaviour,
a `keyboard` peer ignores every gamepad, a `gamepad` peer ignores every
key, and a gamepad peer keeps reading its pad while the other window has
focus).

## Troubleshooting

- `[netplay] handshake refused: protocol` — different netplay versions.
  Use the same build.
- `[netplay] handshake refused: exe` — different exes. Use the same build.
- `[netplay] handshake refused: config` — sim-relevant settings differ
  (old build, or a v1 offer without the session bundle). With the current
  build the joiner adopts the host's settings automatically; if you see
  this, one side is out of date.
- `[netplay] handshake refused: bootstrap` — different randomizer setups
  (v1 offer path). Use the current build so the offer carries the setup.
- `[netplay] handshake refused: seed` — netplay seeds differ (v1 offer
  path). Use the current build so the offer carries the seed.
- `[netplay] ice setup failed: bad ICE code: ...` — the pasted code was
  truncated or belongs to the other direction (offers start `NPIX2-`,
  answers `NPIX1-`). Copy the full single line. Pasted offers are capped
  at ~16k characters (~12 KB of bootstrap); a bigger seed file must be
  passed with `--netplay-join-ice @file.txt` instead (stock bootstraps
  are ~300 B, so paste always fits them).
- `[netplay] ice setup failed: ICE connect timed out` — no network path.
  Behind symmetric NAT: play over a VPN mesh (Tailscale/ZeroTier) using the
  VPN addresses, set up port forwarding, or bring your own TURN server
  (`PIKMIN_NETPLAY_TURN=host:port:user:pass` on both sides).
- `TURN-only violated` — strict relay mode selected a direct candidate;
  your TURN server is unreachable or misconfigured.
- Where the logs are: each run's console output plus the private run dir
  `netplay/run-<date>-pid<id>/` next to the exe (bootstrap used, settings
  copy, `save/`, `offer.txt`, `answer.txt`).

## Current limits

- No Archipelago integration yet.
- No resume across sessions: quitting ends the run for both.
- A desync ends the session (`[netplay] desync detected`, exit 5).
- The low-level switches (`--netplay-host/--netplay-join`,
  `--netplay-ice-host/--netplay-ice-join`) and environment variables keep
  working; they are the test surface.
