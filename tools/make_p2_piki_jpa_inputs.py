"""Stage genuine Piki JPA roles as a fresh private selected-input supplement.

The descriptor owner merges these roles into a successor. Extraction and this
publisher provide no native Scene, runtime session, renderer or gameplay grant.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re

SOURCE_PIN = "2a4952a02b8f19755914df9fbf88660dc4b7d18c"
ARCHIVE = "user/Ebisawa/effect/game.jpc"
ARCHIVE_SHA = "ebf889b4e5df0391662bd531e220b93d148ab56fdec96362386b9cd1e734302c"
ARCHIVE_BYTES = 554688
PREFIX = "p2-original/piki-jpa/"
ROOT_INPUTS = frozenset(('p2-treasure-placements.txt', 'p2-treasure-catalog.txt', 'p2-pelplant-resources.txt', 'p2-chappy-bank.txt', 'p2-frog.txt', 'p2-uji-bank.txt', 'p2-ground-bank.txt', 'p2-original-red-bank.txt', 'p2-kochappy-profile.txt', 'p2-original-tank-bank.txt', 'foliage-bank.txt', 'p2-aquatic-bank.txt', 'p2-snagret-bank.txt', 'p2-flying-bank.txt', 'p2-hanachirashi-joints.txt', 'p2-original-cannon-bank.txt', 'p2-original-cannon-attach.txt', 'p2-original-stone-bank.txt', 'p2-original-gas-bank.txt', 'p2-original-egg-bank.txt', 'p2-original-wisp-bank.txt', 'p2-original-honey-bank.txt', 'size1-bank.txt', 'size1-joints.txt', 'size5-bank.txt', 'size5-joints.txt', 'size10-bank.txt', 'size10-joints.txt', 'size20-bank.txt', 'size20-joints.txt'))
REQUIRED_PARENT = frozenset(("p2-original/campaign.p2pk", "p2-original/calendar.p2sc", "p2-original/stages.txt",
    "p2-original/tutorial.p2c", "p2-original/forest.p2c", "p2-original/yakushima.p2c", "p2-original/last.p2c"))
# Exact source ResourceRole contract, never arbitrary prefix stripping.
ROLES = (
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


def digest(data):
    return hashlib.sha256(data).hexdigest()


def bounded(path, maximum):
    if path.stat().st_size > maximum:
        raise ValueError("input size bound: " + str(path))
    data = path.read_bytes()
    if len(data) > maximum:
        raise ValueError("input changed size: " + str(path))
    return data


def full_sha(value):
    return isinstance(value, str) and re.fullmatch(r"[0-9a-f]{64}", value) is not None


def generate(selected, bank, output):
    resolved_output = output.resolve()
    for source in (selected.resolve(), bank.resolve()):
        if resolved_output == source or source in resolved_output.parents:
            raise ValueError("output must preserve parent and source bank")
    descriptor = bounded(selected / "p2-original-session.txt", 16 * 1024 * 1024)
    lines = descriptor.decode("ascii").splitlines()
    header = lines[0].split() if lines else []
    if (len(header) != 4 or header[:2] != ["P2_ORIGINAL_SESSION", "1"] or
            not full_sha(header[2]) or not header[3].isdigit() or lines[-1] != "END"):
        raise ValueError("selected parent framing")
    count = int(header[3])
    if not 5 <= count <= 65536 or len(lines) != count + 2:
        raise ValueError("selected parent census")
    parent = {}
    total = 0
    previous = ""
    for line in lines[1:-1]:
        words = line.split()
        if len(words) != 2:
            raise ValueError("selected parent role framing")
        role, sha = words
        parts = PurePosixPath(role)
        if (not (role.startswith(("p2-original/", "assets/")) or role in ROOT_INPUTS) or
                len(role) > 512 or re.fullmatch(r"[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*", role) is None or
                str(parts) != role or any(p in (".", "..") for p in parts.parts) or
                not full_sha(sha) or role in parent or (previous and role <= previous)):
            raise ValueError("selected parent role invalid")
        data = bounded(selected / role, 64 * 1024 * 1024)
        total += len(data)
        if total > 2 * 1024 * 1024 * 1024 or digest(data) != sha:
            raise ValueError("selected parent bytes differ: " + role)
        parent[role] = sha
        previous = role
    if not REQUIRED_PARENT.issubset(parent):
        raise ValueError("selected parent required roles absent")
    receipt_data = bounded(bank / "piki-jpa-source-receipt.json", 64 * 1024)
    receipt = json.loads(receipt_data)
    expected = [{"role": name, "member_offset": offset, "bytes": size, "sha256": sha}
                for name, offset, size, sha in ROLES]
    if (receipt.get("disc_id") != "GPVE01" or receipt.get("source_archive") != ARCHIVE or
            receipt.get("archive_sha256") != ARCHIVE_SHA or receipt.get("archive_bytes") != ARCHIVE_BYTES or
            receipt.get("selected_role_contract") != "piki-jpa-gpve01-v1" or
            receipt.get("roles") != expected):
        raise ValueError("JPA receipt differs from exact source role contract")
    checked = {}
    for name, offset, size, sha in ROLES:
        if offset < 0 or offset + size > ARCHIVE_BYTES:
            raise ValueError("JPA member bounds")
        data = bounded(bank / name, size)
        if len(data) != size or digest(data) != sha:
            raise ValueError("JPA genuine bytes differ: " + name)
        role = PREFIX + name
        if role in parent:
            raise ValueError("JPA role already selected; choose an unmodified parent")
        checked[role] = data
    if "p2-original/piki-jpa-provenance.json" in parent:
        raise ValueError("JPA provenance already selected")
    provenance = {
        "contract": "piki-jpa-gpve01-v1", "source_pin": SOURCE_PIN,
        "source_archive": ARCHIVE, "archive_sha256": ARCHIVE_SHA, "archive_bytes": ARCHIVE_BYTES,
        "source_receipt_sha256": digest(receipt_data), "campaign_sha256": header[2],
        "parent_descriptor_sha256": digest(descriptor), "parent_roles": count,
        "prefix": PREFIX, "roles": expected,
        "runtime_identity": "actual owner must bind current campaign, packet fingerprint and exact session",
        "native_scene_installed": False, "gameplay": False, "save_resume": False,
    }
    checked["p2-original/piki-jpa-provenance.json"] = (json.dumps(provenance, indent=2) + "\n").encode("ascii")
    # Every refusal above precedes creation; selected parent is never written.
    output.mkdir(parents=True, exist_ok=False)
    for role, data in checked.items():
        target = output / role
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    roles = {role: digest(data) for role, data in checked.items()}
    (output / "selected-roles.txt").write_text("".join(f"{r} {h}\n" for r, h in sorted(roles.items())), encoding="ascii")
    (output / "receipt.json").write_text(json.dumps(provenance, indent=2) + "\n", encoding="ascii")
    print(f"PIKI_JPA_INPUTS_PASS source_roles=16 selected_roles={len(roles)} parent_roles={count}")
    return roles


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("selected", "bank", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    generate(args.selected, args.bank, args.output)
