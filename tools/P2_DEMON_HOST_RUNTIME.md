# Private Demon host runtime

This private PC host loads the converted `demon0.mod` with the staged two-mouth pose
profile, uses the source-backed continuous Attack capture window (frame 17 in the
fixture), and hands a captured captain to the registered Demon receiver. END chooses
CatchFly when the actual receiver owns the captain. Forced release delegates the real
registered damaging drop with a caller-provided 10 damage; host teardown revokes only
its own token.

`tools/p2_demon_host_runtime.cpp` is an injected host/receiver scenario. It places a
captain at the supplied mouth, explicitly advances the continuous attack frame to 17,
and explicitly issues END. It is not a natural Sarai approach, movement, full FSM, or
authored key-event parity claim. The converted pose bank provides translated mouth
positions; the host writes translated joint matrices but does not yet replay full
per-frame mouth basis rotation or scale. Rendering and capture alignment therefore
remain a pose-position approximation.

Evidence from provenance-verified `output/demon-host-fixture-03`:

- `output/demon-host-session-drop-02/run.log`: capture -> CatchFly -> native bounce,
  registered animation delivery, exactly one accepted 10-HP loss (100 to 90), and
  recovery to Walk.
- `output/demon-host-session-teardown-01/run.log`: own-token teardown releases the
  real mouth relation.

The session overlay supplies `demon0.mod` and `demon-mouths.txt` from the converted
asset output. Missing unrelated `chal0` generator files are logged by preview startup
and do not prevent room readiness or either fixture PASS.