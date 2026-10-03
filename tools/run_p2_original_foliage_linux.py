"""Bounded Linux native foliage runtime, isolated evidence and owned process group."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import time

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--exe', type=Path, required=True)
p.add_argument('--exe-sha256', required=True)
p.add_argument('--source-pin', required=True)
p.add_argument('--run-dir', type=Path, required=True)
p.add_argument('--evidence', type=Path, required=True)
p.add_argument('--mode', choices=['diagnostic', 'walk', 'refusal', 'captain-down'], default='diagnostic')
a = p.parse_args()
exe, run, evidence = a.exe.resolve(strict=True), a.run_dir.resolve(strict=True), a.evidence.resolve()
actual = hashlib.sha256(exe.read_bytes()).hexdigest()
if actual != a.exe_sha256:
    raise ValueError('Fixture hash differs from qualified build')
if evidence.exists():
    raise ValueError('Fresh runtime evidence directory required')
evidence.mkdir(parents=True)
env = os.environ.copy()
for key in ('P2_ORIGINAL_FOLIAGE_WALK','P2_ORIGINAL_FOLIAGE_REFUSE_RESOURCES','P2_ORIGINAL_FOLIAGE_FORCE_CAPTAIN_DOWN','P2_ORIGINAL_FOLIAGE_HUMAN'):
    env.pop(key, None)
env.update(SDL_AUDIODRIVER='dummy', PIKMIN_P2_ROOM_WINDOW='960x540',
           NECTAR_SAVE_DIR=str(evidence/'cards'), PIKMIN_SETTINGS_PATH=str(evidence/'settings.conf'))
mode_keys = dict(walk='P2_ORIGINAL_FOLIAGE_WALK', refusal='P2_ORIGINAL_FOLIAGE_REFUSE_RESOURCES',
                 **{'captain-down':'P2_ORIGINAL_FOLIAGE_FORCE_CAPTAIN_DOWN'})
if a.mode in mode_keys:
    env[mode_keys[a.mode]] = '1'
command = ['xvfb-run','-a','-s','-screen 0 1280x720x24',str(exe),'--experimental-pikmin2-surface','tutorial']
inputs = dict(native=a.source_pin, exe_sha256=actual, exe=str(exe), cwd=str(run), mode=a.mode,
              command=command, timeout=60, starting_pikmin=20, window='960x540 centered',
              full_course=False, save_resume=False, initialized_placement=True,
              natural_input=a.mode=='walk')
(evidence/'run-inputs.json').write_text(json.dumps(inputs,indent=2)+'\n')
start = time.monotonic()
timed_out = False
with (evidence/'native.log').open('wb') as log:
    child = subprocess.Popen(command,cwd=run,env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
    try:
        code = child.wait(timeout=60)
    except subprocess.TimeoutExpired:
        timed_out = True
        os.killpg(child.pid,signal.SIGTERM)
        try:
            code = child.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(child.pid,signal.SIGKILL)
            code = child.wait(timeout=5)
log = (evidence/'native.log').read_text(errors='replace')
markers = dict(diagnostic='PASS ORIGINAL_FOLIAGE sources=', walk='PASS ORIGINAL_FOLIAGE_WALK',
               refusal='PASS ORIGINAL_FOLIAGE_RESOURCE_REFUSAL', **{'captain-down':'P2_FIXTURE_CAPTAIN_DOWN'})
expected_code = 86 if a.mode=='captain-down' else 0
passed = not timed_out and code == expected_code and markers[a.mode] in log
if a.mode=='captain-down':
    passed = passed and 'PASS ORIGINAL_FOLIAGE' not in log
result = dict(passed=passed, returncode=code, timed_out=timed_out, marker=markers[a.mode],
              elapsed_seconds=time.monotonic()-start, mode=a.mode)
(evidence/'run-result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
raise SystemExit(0 if passed else 1)
