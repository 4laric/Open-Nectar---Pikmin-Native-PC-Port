"""Stage private source91/88 native fixtures from a verified20Red tutorial baseline.
Legal files stay local. No shared assets or source course files are changed.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import sys


def digest(data):
    return hashlib.sha256(data).hexdigest()


def overlay(source, dest, overrides):
    dest.mkdir(exist_ok=False)
    for name in sorted({p.name for p in source.iterdir()} | {p.split('/')[0] for p in overrides}):
        src, dst = source / name, dest / name
        if name in overrides:
            dst.write_bytes(overrides[name])
            continue
        children = {k[len(name) + 1:]: v for k, v in overrides.items() if k.startswith(name + '/')}
        if children:
            if src.is_dir():
                overlay(src, dst, children)
            else:
                dst.mkdir()
                for key, data in children.items():
                    target = dst / key
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(data)
        elif src.is_dir():
            if os.name == 'nt':
                import _winapi
                _winapi.CreateJunction(str(src), str(dst))
            else:
                dst.symlink_to(src, target_is_directory=True)
        else:
            # Assets are immutable under this fixture; saves/settings/logs use
            # a separate fresh run-specific directory supplied by supervisor.
            os.link(src, dst)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--baseline', type=Path, required=True)
    p.add_argument('--bank', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    baseline = a.baseline.resolve(strict=True)
    bank = a.bank.resolve(strict=True)
    out = a.output.resolve()
    if out.exists():
        raise ValueError('Fresh staging directory required; preserve prior attempts')
    path = 'dataDir/stages/p2_tutorial/default.gen'
    original = (baseline / 'assets' / path).read_bytes()
    data = bytearray(original)
    if data[:4] != b'1.0v' or len(list(re.finditer(b'ikip', data))) != 20:
        raise ValueError('Expected actual20-Pikmin binary tutorial baseline')
    teki = list(re.finditer(b'iket', data))
    if len(teki) != 1:
        raise ValueError('Expected exactly one resource-only native Teki row')
    offset = teki[0].start()
    start = offset - 72
    if start < 24 or data[start+4:start+8] != b'0.0v' or data[offset+4:offset+8] != b'\x0a\x00\x00\x00' or data[offset+8] != 4:
        raise ValueError('Source baseline chassis framing/identity changed')
    end = data.find(b'    0.0v', offset)
    tail = bytes(data[offset:end if end >= 0 else len(data)])
    count_key = tail.rfind(b'p00\x04')
    if count_key < 0 or struct.unpack_from('>I', tail, count_key+4)[0] != 0:
        raise ValueError('Native chassis row must be resource-only countzero')
    data[offset+8] = 7  # literal P1 TEKI_Palm allocation/cleanup chassis only
    overrides = {path: bytes(data)}
    mods = sorted(bank.glob('*/*.mod'))
    if len(mods) != 24:
        raise ValueError('Expected24 genuine source91/88 pose models')
    for mod in mods:
        overrides['dataDir/courses/pikmin2room/' + mod.name] = mod.read_bytes()
    out.mkdir(parents=True)
    overlay(baseline / 'assets', out / 'assets', overrides)
    for name in ('foliage-bank.txt', 'foliage.json', 'sha256.json'):
        (out / name).write_bytes((bank / name).read_bytes())
    receipt = dict(baseline=str(baseline), source_default_sha256=digest(original),
                   overrides={name:digest(data) for name,data in overrides.items()},
                   foliage_bank_sha256=digest((out/'foliage-bank.txt').read_bytes()),
                   starting_pikmin=20, chassis_id=7, chassis_count=0,
                   window='960x540 centered', ordinary_gameplay=False,
                   original_positions=False, whole_course=False,
                   argv=['--experimental-pikmin2-surface', 'tutorial'])
    (out / 'stage-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps({'run':str(out), 'models':len(mods), 'chassis':7, 'live_baseline':20}))


if __name__ == '__main__':
    main()


