"""Stage an explicit development20 input successor; never edit a selected packet.

Generated legal assets stay private. Merge selected-roles.txt through the actual
session descriptor owner before use. These inputs grant no gameplay authority.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct


def sha(data):
    return hashlib.sha256(data).hexdigest()


def text(value):
    value = value.encode("ascii")
    return struct.pack("<I", len(value)) + value


def generate(selected, bank, baseline, output):
    descriptor = (selected / "p2-original-session.txt").read_text().splitlines()
    header = descriptor[0].split()
    if len(header) != 4 or header[:2] != ["P2_ORIGINAL_SESSION", "1"]:
        raise ValueError("unsupported selected session")
    if descriptor[-1] != "END":
        raise ValueError("selected session framing")
    roles = dict(line.split() for line in descriptor[1:-1])
    if len(roles) != int(header[3]):
        raise ValueError("selected role census differs")
    def retained(role):
        data = (selected / role).read_bytes()
        if sha(data) != roles[role]:
            raise ValueError("selected bytes differ: " + role)
        return data
    floor_lines = retained("p2-original/development-floor.p2d").decode("ascii").splitlines()
    tag, cave, floor = floor_lines[0].split()
    if tag != "P2_RETAIL_DEVELOPMENT_FLOOR_1" or cave != "tutorial_1" or floor not in ("1", "2"):
        raise ValueError("unsupported development floor")
    bindings = dict(line.split() for line in floor_lines[1:])
    prefix = f"p2-original/retail-caves/{cave}/floor{floor}/"
    files = {"plan": "floor.p2f", "geometry": "geometry.mod", "routes": "routes.ini",
             "start": "start.json", "pool": "unit-pool.txt", "layout": "start-layout.txt"}
    for kind, name in files.items():
        if sha(retained(prefix + name)) != bindings[kind]:
            raise ValueError("floor binding differs: " + kind)
    start = json.loads(retained(prefix + "start.json"))
    baseline_sha = sha(baseline.read_bytes())
    provenance = (f"P2_DEVELOPMENT_STARTUP20_1\nbaseline {baseline_sha}\n"
                  f"plan {bindings['plan']}\nstart {bindings['start']}\n"
                  "fixture 20 red Free-Bore\ngrid 2 10 8 -36 32 30\n"
                  "placement absolute-map-start-xz-slot-y-plus30\n"
                  "ground validate-dry-support-refuse-no-relocation\n"
                  "claim engineered-not-story-acquisition\n").encode("ascii")
    provenance_sha = sha(provenance)
    records = []
    for index in range(20):
        key = f"development/startup20.txt#{index}"
        # External catalog identity, never installed as a native Generator UID.
        uid = 0x52000000 | int.from_bytes(hashlib.sha256(key.encode()).digest()[:3], "big")
        position = (start["map_start"][0] - 36 + (index % 10) * 8,
                    start["slot"]["global_position"][1] + 30,
                    start["map_start"][2] + 32 + (index // 10) * 8)
        row = (text(key) + provenance_sha.encode() + b"v0.30001" +
               struct.pack("<7I6f", uid, 1, 1, 0, 0, 0, 0xffffffff, *position, 0, 0, 0))
        records.append((key, row))
    rows = b"".join(row for _, row in sorted(records))
    catalog = sha(rows)
    manifest = b"P2PK1" + header[2].encode() + catalog.encode() + struct.pack("<I", 20) + rows
    manifest += hashlib.sha256(manifest).digest()
    receipt = json.loads((bank / "receipt-with-registry.json").read_text())
    bank_files = receipt["files"]
    motions = {"akubi": 0, "asibumi": 1, "chatting": 3, "iraira": 21,
               "nigeru": 28, "run2": 29, "walk": 30, "wait": 31, "kizuku": 32,
               "rolljmp": 35, "hang": 36, "sagasu2": 54, "suwaru": 56, "neru": 57}
    registry = json.loads((bank / "motion-registry.json").read_text())
    if (len(bank_files) != 364 or set(receipt["motions"]) != set(motions) or
            {name: facts["source_id"] for name, facts in registry["clips"].items()} != motions or
            receipt["source_model_sha256"] != "4ad910ab6dec0722180c117539d3467b705b94dbe322eae40809e3345a44e358"):
        raise ValueError("requires genuine RGB04 fourteen-motion closure")
    members = {}
    for name, facts in bank_files.items():
        if Path(name).name != name or name in (".", ".."):
            raise ValueError("unsafe bank member")
        data = (bank / name).read_bytes()
        if sha(data) != facts["sha256"] or len(data) != facts["bytes"]:
            raise ValueError("bank receipt differs: " + name)
        members["p2-original/piki-bodies/red/" + name] = facts["sha256"]
    contract = ("P2_RETAIL_STARTING_PIKI_1 20\n" + f"campaign {header[2]}\nfloor {cave} {floor}\n"
                f"plan {bindings['plan']}\nstart {bindings['start']}\nbaseline {baseline_sha}\n"
                f"provenance {provenance_sha}\nmanifest {sha(manifest)}\ncatalog {catalog}\nbank red 364\n" +
                "".join(f"{role} {digest}\n" for role, digest in sorted(members.items())) + "END\n").encode()
    # All verification precedes creating a fresh output directory.
    output.mkdir(parents=True, exist_ok=False)
    generated = {"p2-original/development-starting.p2ps": contract,
                 "p2-original/development-starting.p2pk": manifest,
                 "p2-original/development-starting.txt": provenance}
    for role, data in generated.items():
        target = output / role
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        members[role] = sha(data)
    for name in bank_files:
        target = output / "p2-original/piki-bodies/red" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(bank / name, target)
    (output / "selected-roles.txt").write_text("".join(f"{r} {h}\n" for r, h in sorted(members.items())), encoding="ascii")
    (output / "receipt.json").write_text(json.dumps({"campaign": header[2], "catalog": catalog,
        "manifest_sha256": sha(manifest), "selected_roles": len(members), "count": 20, "species": 1,
        "provenance_sha256": provenance_sha, "baseline_sha256": baseline_sha,
        "floor_plan_sha256": bindings["plan"], "activated": False, "gameplay": False,
        "save_resume": False, "claim": "explicit engineered development20; no story acquisition"}, indent=2))
    print(f"STARTING_PIKI_INPUTS_PASS count=20 species=1 roles={len(members)} catalog={catalog}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("selected", "bank", "baseline", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    generate(args.selected, args.bank, args.baseline, args.output)
