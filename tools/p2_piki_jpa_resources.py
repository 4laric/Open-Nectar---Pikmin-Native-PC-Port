"""Extract genuine bounded Piki JPAC inputs; raw bytes remain private.
Resource import is not emitter, sound or gameplay qualification.
"""
import argparse
import hashlib
import json
from pathlib import Path
from experimental.pikmin2_assets import disc_files

ARCHIVE = 'user/Ebisawa/effect/game.jpc'
ARCHIVE_SHA = 'ebf889b4e5df0391662bd531e220b93d148ab56fdec96362386b9cd1e734302c'
RESOURCES = (
    ('piki-0169.jpa', 205468, 304, 'f57796cd2b1aa7dd9554293595ba16a048ce6ff32cc6ecedd45780b7b6df4562'),
    ('IP2_ringhalo_i.tex1', 482880, 4160, '82d8e6c36cdafad0d1392b9b902aa94c9f72616ff9f8ef6dbb5fd0ea1a5054f9'),
    ('piki-016a.jpa', 205772, 304, '26eaf40991e8bc7b16825073bbd5f6ba7435bb225d76cd4381284fa776b50d8e'),
    ('piki-0281.jpa', 206076, 304, 'f0a3c4e84720b6eda09ec111ad936e64602c6337d11a3267303f7447122509f4'),
    ('piki-016b.jpa', 206380, 304, 'fe10a9bebdcc09fc0b3f072b660b3782362944381ed72a3a0ceaca16381e097e'),
    ('piki-016c.jpa', 206684, 304, '87cf8317855c777fa40aeb4ed7fe3e4c0a616df56f3f4074fa3e3a647ed8621a'),
    ('piki-016d.jpa', 206988, 304, 'ed18b8d93e185ce077de496f6216818b6ec8f6c0199e23e0e2ce7dcbee7d0df9'),
    ('piki-0172.jpa', 208348, 276, 'be9276aa8b8c951ba480705841326f5b6e1fc804362152d065d8c22549fdb3b9'),
    ('IP2_firemsk1_ia.tex1', 297632, 1088, '802c5354db1f6d10bf0a6dc6c9dd44ecf488cb755b82f169022f59af4f66ef11'),
    ('IP2_ami2_i.tex1', 445952, 1088, 'deb6a406cf184b327fe036e2bf4637d1b06b37d73095fbc72f24f1fc76122083'),
    ('piki-0173.jpa', 208624, 276, 'ae1fc60271854b306d62d47e3d97dfa3a2fb768625989c0655c026bbada064be'),
    ('piki-0174.jpa', 208900, 276, '1004a7e1ea1d01ea2eb53409a36fc94d86c1f5b7ff587189cbb44b7de97d1c7b'),
    ('piki-0175.jpa', 209176, 276, '971fd4c1284d5d1eb19ee5f4f6be684d071a8afbe289b27395047645a263588c'),
    ('piki-0176.jpa', 209452, 276, '2b2051166d1b1a8c373d32ed234179ef9e187aa69c7c9267b57a67efc6e33cb2'),
    ('piki-0177.jpa', 209728, 316, '2aa39e477cf5a404ec2224f6f83cd1d3f08975763d07edbe07acbf582da2c262'),
    ('IP2_star5_i.tex1', 264032, 4160, 'bdd22ded1e6467a6b1918fe3befd9808ea7839d2100d4314a26d6d685f1decf1'),
)

def extract(iso, out):
    offset, size = disc_files(iso)[ARCHIVE]
    with iso.open('rb') as source:
        source.seek(offset)
        archive = source.read(size)
    if len(archive) != size or hashlib.sha256(archive).hexdigest() != ARCHIVE_SHA:
        raise ValueError('GPVE01 game.jpc source digest mismatch')
    checked = {}
    for name, start, length, digest in RESOURCES:
        data = archive[start:start + length]
        if len(data) != length or hashlib.sha256(data).hexdigest() != digest:
            raise ValueError('Piki JPA member digest mismatch: ' + name)
        checked[name] = data
    # Validate every existing member before any output is replaced.
    for name, data in checked.items():
        target = out / name
        if target.exists() and target.read_bytes() != data:
            raise ValueError('Existing output changed: ' + name)
    out.mkdir(parents=True, exist_ok=True)
    for name, data in checked.items():
        (out / name).write_bytes(data)
    receipt = {'disc_id': 'GPVE01', 'source_archive': ARCHIVE,
               'archive_sha256': ARCHIVE_SHA, 'archive_iso_offset': offset, 'archive_bytes': size, 'effects': [361,362,363,364,365,370,371,372,373,374,375,641],
               'selected_role_contract': 'piki-jpa-gpve01-v1', 'native_renderer_qualified': False,
               'gameplay': False,
               'roles': [{'role': name, 'member_offset': start, 'bytes': length, 'sha256': digest}
                         for name, start, length, digest in RESOURCES],
               'files': {name: hashlib.sha256(data).hexdigest() for name, data in checked.items()}}
    (out / 'piki-jpa-source-receipt.json').write_text(
        json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    return receipt

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--iso', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(extract(args.iso, args.out)))
