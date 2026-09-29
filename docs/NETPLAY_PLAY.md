# Netplay co-op: how to play (one copy-paste code each way)

Two players on two PCs can play one co-op session by exchanging one code in
each direction: the host sends an **offer code**, the joiner sends back an
**answer code**. Nobody sets environment variables or copies files. One
player can also test alone with two windows on one PC (see below). There is
no hosted matchmaking or relay service: the codes are the whole connection
setup.

## What both players need

- The **same netplay build** of `nectar.exe` (a build configured with
  `-DPIKMIN_NETPLAY_BUILD=ON`). The handshake hashes the running exe, so
  even a one-byte difference refuses with `[netplay] handshake refused: exe`.
  A build tree's `bin\nectar.exe` also needs the MinGW runtime DLLs
  (`SDL2.dll`, `libstdc++-6.dll`, `libgcc_s_seh-1.dll`,
  `libwinpthread-1.dll`) next to it or on `PATH`; an installed game folder
  already has them.
- **Their own game data.** The game reads `assets\` (the game data) and
  `pikmin_settings.conf` (your settings) from the folder you start it from,
  its *working directory*. Start the exe from your game folder (the one
  holding `assets\`), or put a junction named `assets` pointing at your
  game data into the folder you start from. The launcher never changes the
  working directory, except for seeds with P2 enemies (see below).
- A network path between the two PCs (see Troubleshooting).

## What the game does for you

When you start with `--netplay-host-ice` or `--netplay-join-ice`, the game
sets the whole session up before anything else loads:

- It creates a **private run folder** for this session and this player:
  `netplay\run-<date>-<time>-<host|join>-pid<id>\` next to the exe (or
  `%LOCALAPPDATA%\Nectar\netplay\` when the exe folder is read-only). Every
  session gets a new one; it is never reused. Inside it:
  - `session\runs\<token>\bootstrap.txt`: the session's seed file, with this
    player's own `SESSION` line, plus the game's `state.txt`/`hello.txt`;
  - `session\campaign\`: this session's memory card (`card\card0`) and
    day-end campaign files;
  - `save\`: the private save folder (`NECTAR_SAVE_DIR`, used for the
    shader cache);
  - `launch.txt` (what the session used), `settings-at-start.conf` (a copy
    of your settings file as it was), and `offer.txt` or `answer.txt` (the
    code this player produced).
- Your own memory card and your saves are **never used or written**: the
  session plays on the run folder's card.
- **Settings.** The host's sim-relevant settings (the ones that change how
  the game plays: mods such as chain actions or no tripping, the Pikmin
  limit, day length, and so on) are the session's. The joiner adopts them
  for the session only. During the session those settings are locked (an
  F1 change to them reverts when the menu closes), and your
  `pikmin_settings.conf` always keeps your own values for them: F1 still
  saves presentation changes (window size, graphics, bindings), and a save
  with no such change leaves the file untouched. With the randomizer
  running, some settings are fixed by the randomizer anyway (day length,
  health, speeds).
- **Seed.** Without `--bootstrap`, the host starts the default new game:
  Forest of Hope day 2, 25 ship parts, red Pikmin, 10 Flarlic (the
  `tools/netplay/run_pair.py` profile). There is no Archipelago in a netplay
  session: the game gives itself a fixed ready state (the unlock mask the
  pair tools use, 127; no parts, no Flarlic, no received items).

## Host steps

1. Open a console (Command Prompt or PowerShell) in your game folder and
   run:
   `nectar.exe --netplay-host-ice`
   - To play a randomizer seed, add its bootstrap file:
     `nectar.exe --netplay-host-ice --bootstrap C:\path\to\bootstrap.txt`.
     Seeds with P2 enemies (`ENEMY_P2` in the file) work too: host from the
     seed's own folder (the one holding the seed's `assets\` overlay and its
     `p2-*.txt` / `sarai-*.txt` files); see "Seeds with P2 enemies".
2. The game prints a one-line **offer code** (it starts with `NPIX2-`) and
   copies it to the clipboard.
3. Send the offer code to the joiner (chat, DM, anything).
4. When the joiner sends the **answer code** back (it starts with `NPIX1-`),
   paste it into the host's console and press Enter. A bad paste prints
   why and asks again; it does not end the session.
   (Scripts can pass `--netplay-answer-in <file>` instead: the game waits
   for a valid answer in that file.)
5. Both games print `[netplay] ice completed in ...ms` and start. The input
   delay is measured automatically.

## Joiner steps

1. Copy the host's whole offer code, open a console in your game folder,
   and run one of:
   - `nectar.exe --netplay-join-ice @clipboard` (reads the clipboard: the
     easiest, and it has no length limit),
   - `nectar.exe --netplay-join-ice @C:\path\to\offer.txt` (a file holding
     the code),
   - `nectar.exe --netplay-join-ice NPIX2-...` (the code itself; a big
     offer can exceed the console's command-line limit, 8,191 characters in
     Command Prompt, so prefer `@clipboard` or `@file` for large seeds).
   You need no file from the host: the offer carries the seed file, the
   netplay seed and the host's sim settings.
2. The game prints a one-line **answer code** and copies it to the
   clipboard. Send it to the host. (`--netplay-code-out <file>` also writes
   it to a file, on either side.)
   For a seed with P2 enemies, also pass `--netplay-p2-assets <folder>`:
   your own copy of that seed's P2 assets overlay (see "Seeds with P2
   enemies"); without it the joiner refuses before creating anything.
3. Wait for `[netplay] ice completed in ...ms`; the session starts.

Controls: by default each game takes input the usual way. To pin a device,
add `--netplay-input keyboard` (keys and mouse only, every gamepad
ignored), `--netplay-input gamepad` or `gamepad:N` (the first or the N-th
gamepad only, every key ignored; the pad keeps working while another window
has focus, and a pad plugged in later is picked up), or
`--netplay-input auto` (the default behaviour). In a session the debug
hotkeys (F5, F6, F9) do nothing: they would change one player's game only.

Keep the console open: it shows the codes and the session log. To keep a
log file, start the game with `> host.log 2>&1` added (the code is still
copied to the clipboard and written to the run folder).

## The netplay HUD

During a session a small box on the right, just below the day counter,
shows how the connection is doing:

```
NETPLAY  good
ping 42 ms  jitter 3 ms
delay 2 (67 ms)  stalls 0 / 10 s
```

- **NETPLAY good / fair / poor** (green / yellow / red; `measuring` in grey
  until the first ping): *good* is a ping of at most 100 ms, jitter of at
  most 15 ms and no stall in the last 10 s; *fair* is a ping of at most
  200 ms, jitter of at most 40 ms and at most 3 stalls in the last 10 s;
  anything worse is *poor*.
- **ping**: the round trip to the other game (the average of the last 10
  measurements); **jitter**: how much it changes between measurements.
- **delay**: the input delay in frames (one frame is 33 ms at 30 Hz). Your
  own captain moves this many frames after you press, so both games can
  apply every input on the same frame.
- **stalls / 10 s**: how often, in the last 10 seconds, this game had to
  wait at least one frame for the other game's input (the picture pauses
  briefly). Stage loads and Archipelago pauses are not counted.

**F4** shows or hides the box; on a gamepad press **both sticks in (L3 +
R3)** together. F4 does nothing while you have bound it to a game action in
the controls; a keyboard-only game (`--netplay-input keyboard`) ignores the
pad chord. The box is hidden while the F1 menu is open. It never changes
the game: the other player's game is not told about it. The same numbers go
to the log every 30 s (`[netplay] link: ...`).

## When a session ends (desync, lost connection, quit)

If the two games stop agreeing (a desync), the connection is lost, or the
other player closes their game, the session ends. The game then shows a
banner for up to 10 seconds (any key or button closes it early) and prints
the same message in the console, for example:

```
[netplay] ==== netplay session ended ====
[netplay] CONNECTION LOST: no data from the other game for too long (it may have crashed or lost its network).
[netplay] Last saved day: day 3 (the campaign continues from the start of day 3); checkpoint 1.
[netplay] To carry on from that day: run host.bat and answer Y to "Continue last campaign?", or run:
[netplay]   nectar.exe --netplay-host-ice --continue
[netplay] Your partner joins as usual (join.bat); your saved day is sent to them automatically.
```

- **DESYNC** (exit code 5): the games disagreed about the game state.
- **DESYNC AT THE DAY-END SAVE** (exit code 5) / **SAVE NOT AGREED** (exit
  code 6): the day-end save did not finish the same way on both games, so
  that day does not count; the campaign continues from the day before.
- **CONNECTION LOST**: no data from the other game for 15 s (60 s during a
  stage load).
- **THE OTHER PLAYER LEFT**: the other game was closed. A game that is
  closed mid-session tells the other one at once, so it no longer waits
  15 s.

Nothing is lost except the day in progress: the campaign continues from the
last day that ended with the day-end save on both games (see below). If no
day has ended yet, the message says so, and the next session starts a new
campaign.

## Local two-window test (one PC, one player)

```
tools\netplay\play_local.bat -Exe C:\path\to\netplay\nectar.exe
```

It starts a host window (keyboard) and a joiner window (first gamepad) on
this PC and moves the codes between them through files, so there is nothing
to paste. The keyboard window is the host; the gamepad drives the joiner
even while the host window has focus (connect it before you start; a pad
plugged in later is also picked up). Close both game windows (or press Ctrl+C in the console) to finish.

- Each window gets its own working folder under `-OutDir` (default
  `%LOCALAPPDATA%\Nectar\netplay-local\host` and `...\join`), with an
  `assets` junction and its own settings file; each writes its console log
  there (`native.log`).
- Game data: `-Assets <folder>`, else `assets\` next to the exe, else the
  installed game data `%APPDATA%\PikminRandomizer\game-data\assets`.
- It uses this PC only (loopback, no STUN server), and on a build tree it
  puts `C:\msys64\mingw64\bin` on `PATH` for the two games when `SDL2.dll`
  is not next to the exe.
- Both windows open windowed at 960x540, centred on the screen: drag one
  aside, and click the host window before you use the keyboard (keys only
  reach the window that has focus; the gamepad does not need focus).
  `-WindowSize WxH` changes the size, `-WindowSize off` keeps each
  window's own settings.
- `-Bootstrap <file>` plays a seed; `-HostInput`/`-JoinInput` change the
  devices. `-Hidden` is the automated test mode (hidden, bounded).

## Seeds with P2 enemies

A seed with `ENEMY_P2` (or any `P2_...` word) reads extra per-run files from
the working directory: the P2 sidecars (`p2-*.txt`, `sarai-*.txt`,
`demon-*.txt`, `damagumo-*.json` and `p2_bigtreasure_events.txt`, next to
the seed's `bootstrap.txt`) and a P2 `assets\` overlay (the room models in
`assets\dataDir\courses\pikmin2room\`, about 35 MB, the stage files in
`assets\dataDir\stages\`, `assets\config.ini` and `assets\p2-*.txt`).
For such a seed the launcher:

- makes `<run folder>\play\` the working directory before anything loads,
  with `play\assets` a junction to the overlay (the host's is the seed
  folder's `assets\`; the joiner's is its `--netplay-p2-assets` folder);
  your `pikmin_settings.conf` stays the one in the folder you started from;
- copies the host's sidecars into its `play\`, and sends them to the joiner
  before the session starts (the joiner's `play\` receives them);
- never sends the overlay: it is game content. Each player brings their own
  copy (a full copy, or one with junctioned folders: only the content
  counts), and the handshake compares a digest of both (`handshake refused:
  p2assets` when they differ). The sidecars are checked against the host's
  digest too (`handshake refused: sidecars`).

With the low-level switches the joiner's working folder receives the host's
sidecars the same way; its own sidecar files that differ are moved to
`sidecar-set-aside-<time>\` (never deleted). The host's P2 receipt ledgers
(`p2-*-receipts.txt`) are sidecars too, so they travel with the set.

`p2-binding-receipt.json`, the `*-install.json` files and
`overlay-manifest.json` are not needed at run time and are not sent.

## Two sessions in a row (resume)

At the end of a day both games save at the same moment. The host writes the
real campaign checkpoint (`session\campaign\<generation>.sav`) and the
card; the joiner writes the same files as a mirror; the two games then agree
on the host's result before either continues (a `[netplay] save barrier`
line in both logs). The joiner's mirror is written as `<generation>.sav.pending`
and only becomes `<generation>.sav` once the host reports success. If the
other game disconnects there (no network traffic for 15 s), or is still
connected but has not reached the save within 60 s, the day is abandoned as
it would be without netplay (exit 6); if the two saves differ, both games stop
with exit 5 (a desync). Either way the joiner renames its unconfirmed mirror
to `*.sav.unconfirmed` (never deleted), so the next session sees the last
day both games agreed on and simply receives the host's checkpoint.

When a session starts, the two games compare their newest checkpoints:
the same checkpoint on both sides plays on; a joiner that is behind (or has
none) gets the host's checkpoint and card before the session starts
(`[netplay] checkpoint adopted`); a joiner that is ahead of the host, or has
a different checkpoint of the same day, refuses (`handshake refused:
checkpoint`). A joiner checkpoint from another seed or a damaged one is set
aside (renamed `*.sav.stale-<time>`, never deleted) and replaced.

### Continue the campaign (`--continue`)

The one-command launcher starts every session in a new run folder. Without
`--continue` that is a new campaign. To carry on with the last campaign,
the **host** adds `--continue`:

```
nectar.exe --netplay-host-ice --continue
```

(the playtest `host.bat` asks `Continue last campaign? [Y/n]` and adds it
for you). The joiner does nothing different: it joins with the usual offer
code, and the host's saved day reaches it at the handshake (`[netplay]
checkpoint adopted`).

- `--continue` picks the newest host run folder (under `netplay\` next to
  the exe, or `%LOCALAPPDATA%\Nectar\netplay\`) whose campaign has a
  day-end save **both** games agreed on, and says which:
  `[netplay] launch: --continue: continuing the campaign of ...\run-...:
  checkpoint 1 (day 3)`. Both games then start that day from its
  beginning.
- It never continues a half-saved day: a day-end save that did not finish
  on both games (exit 5 or 6 at the save, or a game that crashed during
  it) is skipped, and the day before it is used. Each run folder keeps a
  small `campaign-record.txt` for this (which saves both games agreed on).
- `--continue <run folder>` continues that run folder instead (for example
  an older campaign, or a run where you were the joiner: its campaign is the
  same, so either player can host the next session).
- `--continue` with `--bootstrap <seed file>` continues the newest campaign
  **of that seed**.
- The seed file and the netplay seed come from the continued run; the
  settings are this session's, as always.
- Nothing is moved or deleted: the saved day, the memory card and the
  campaign's other files are **copied** into the new run folder, and the
  old run folder stays exactly as it was.
- No saved day yet (the first day never ended with a save): `--continue`
  says so (`--continue: no saved day yet ...`) and offers a new campaign
  instead (`Start a new campaign instead? [Y/n]`; without a console it
  starts one).

Seeds with P2 enemies continue too: the continued run's `play\` folder
(its sidecars and P2 receipt ledgers) and its P2 assets overlay are used
again; if that overlay was moved, pass `--netplay-p2-assets <folder>`.

Resuming through the pair tools (`tools/netplay/run_pair.py --run-name ...
--token ...`, which keep both campaign folders) keeps working as before.

Every session needs a new run folder on both sides (the game refuses a run
folder that was already used), and the joiner's `mirror-events.txt` starts
again at frame 0 in each one. A runner that ingests the joiner's mirror must
therefore treat each session as its own run (a new peer token and run
folder), not append the second evening to the first evening's run.

## Troubleshooting

- `[netplay] handshake refused: exe` or `protocol`: the builds differ. Use
  the same `nectar.exe` on both PCs.
- `[netplay] handshake refused: config`, `bootstrap` or `seed`: the two
  games did not agree on the session setup. With `--netplay-host-ice` /
  `--netplay-join-ice` the offer carries it, so this means different builds.
  The low-level switches `--netplay-ice-host` / `--netplay-ice-join` (note
  the word order) use each side's own settings, seed file and seed and do
  refuse here; the game prints a hint when that is the likely cause.
- `[netplay] launch: ...` at start-up: the launcher refused before creating
  anything, and says why (for example a P2 offer without
  `--netplay-p2-assets`, a pasted answer code where
  the offer belongs, an offer from an older build, or a switch that cannot
  be combined with the launcher).
- `[netplay] handshake refused: checkpoint`: the two campaigns cannot be
  reconciled (the joiner is ahead of the host, or both saved different
  worlds). `sidecars` / `p2assets`: the P2 files differ (see "Seeds with P2
  enemies").
- `[netplay] ice setup failed: bad ICE code: ...`: the code was cut off or
  belongs to the other direction (offers start `NPIX2-`, answers
  `NPIX1-`). Copy the whole single line.
- `[netplay] ice setup failed: ...timed out`: no network path between the
  PCs. Behind symmetric NATs, play over a VPN mesh (Tailscale, ZeroTier) so
  both PCs see each other's VPN address, forward a UDP port range
  (`PIKMIN_NETPLAY_ICE_PORT_BEGIN`/`_END`), or bring your own TURN server
  (`PIKMIN_NETPLAY_TURN=host:port:user:pass` on both sides).
- `[netplay] input: gamepad #0 is not connected yet`: the pad was not
  plugged in at start; it is picked up as soon as it connects.
- `[netplay] disconnected: handle=...`: the other game stopped answering
  for 15 s (60 s while a stage is loading, from the load until 30 frames
  later), or it quit or crashed. Shader compiles, stage and file loads and
  the day-end save's card writes keep the connection alive while they run,
  and a stage load may take up to 60 s even as one uninterrupted step. A
  slow PC can still drop the session if one single step outside a stage
  load (for example one very slow disk write, or a driver hang) freezes the
  game for more than 15 s. `[netplay] long tick: ...` lines show any tick
  that blocked for more than 2 s and how often the network was polled during
  it. If a stage load freezes one PC just before a pause for a lost
  Archipelago link, a crashed partner is reported after 60 s instead of 15 s
  until the pause ends.
- `[netplay] save barrier timeout ...` (exit 6): at the end of a day both
  games save and compare the result. The other game disconnected there, or
  it did not reach the save within 60 s while still connected; that day is
  not saved, and the next session continues from the last day both games
  saved.
- `[netplay] launch: --continue: no saved day yet ...`: none of the host's
  run folders has a day that ended with the day-end save on both games (for
  example the first day never ended). Answer Y (or just press Enter) to
  start a new campaign; `--continue <run folder>` picks a specific run.
- Logs: the console output (or your `> file` redirect; `native.log` in the
  local test), plus the run folder `netplay\run-...\` next to the exe.

## Current limits

- No Archipelago: a netplay session plays a seed offline with a fixed
  ready state; nothing is sent or received.
- A desync, a lost connection or a quit ends the session for both players;
  nothing carries over but the last day both games saved, which the host
  continues with `--continue` (see "Continue the campaign"). There is no
  mid-session reconnect.
- The HUD's stall count is this game's own (a wait of at least one frame
  for the other game's input); it cannot show a stall while it happens,
  because the picture itself waits.
- Seeds with P2 enemies need each player's own copy of the seed's P2
  assets overlay (see "Seeds with P2 enemies").
- The low-level switches (`--netplay-host`/`--netplay-join`,
  `--netplay-ice-host`/`--netplay-ice-join`) and their environment
  variables keep working; they are the test surface.
