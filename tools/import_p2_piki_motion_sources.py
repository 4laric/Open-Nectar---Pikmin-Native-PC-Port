"""Retain all 67 genuine registered Piki animations for a species.

This is a raw, bounded source-motion closure. It generates no sampled MODs,
normal destinations, live animator, selected Scene or gameplay authority.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
from import_p2_piki_source_bank import MODELS


def require(value):
    if not value:
        raise ValueError("genuine full Piki motion source contract differs")


def scan_bca(data):
    require(len(data) >= 72 and data[:8] == b"J3D1bca1" and
            struct.unpack_from(">I", data, 8)[0] == (len(data) + 31) // 32 * 32)
    block = data[32:]; require(block[:4] == b"ANF1")
    duration, count = struct.unpack_from(">HH", block, 10)
    require(count == 11 and 1 <= duration <= 10000)
    table, scales, rotations, translations = struct.unpack_from(">4I", block, 20)
    require(table >= 36 and table + count * 36 <= len(block))
    zero = []
    for joint in range(count):
        for axis in range(3):
            for component, (offset, fmt, size) in enumerate(((scales, "f", 4), (rotations, "h", 2), (translations, "f", 4))):
                length, index = struct.unpack_from(">HH", block, table + joint * 36 + axis * 12 + component * 4)
                require(1 <= length <= 10000 and offset >= 36 and offset + (index + length) * size <= len(block))
                for sample in range(length):
                    value = struct.unpack_from(">" + fmt, block, offset + (index + sample) * size)[0]
                    require(math.isfinite(value))
                    if component == 0 and value == 0:
                        zero.append({"joint": joint, "axis": axis, "track_sample": sample})
    return {"duration": duration, "joints": count, "authored_zero_scale_samples": zero}


def generate(iso, species, output):
    from experimental.pikmin2_assets import disc_files, archive_files
    catalog = disc_files(iso); members = {}; provenance = {}
    expected = {"user/Kando/piki/pikis.szs": "913a01d6f9c77a7e7b2de604c2a708ba256de9aaf890702b0b8e1b6bf038eef3",
                "user/Kando/piki/texts.szs": "04b8911efe66ec18cc83733e74aa855ed7215fb48c7dbc2544a643bf865d46d5",
                "user/Abe/piki/pikiParms.txt": "f22ae88fade54bf8f142ecc5aae4ce0c82078e6aed448d029f16b75e5a3d7996"}
    with iso.open("rb") as source:
        for name, sha in expected.items():
            offset, size = catalog[name]; require(0 < size <= 8 * 1024 * 1024)
            source.seek(offset); data = source.read(size)
            require(len(data) == size and hashlib.sha256(data).hexdigest() == sha)
            members[name] = data; provenance[name] = {"offset": offset, "bytes": size, "sha256": sha}
    archive = archive_files(members["user/Kando/piki/pikis.szs"])
    registry = archive_files(members["user/Kando/piki/texts.szs"])["animmgr.txt"]
    require(hashlib.sha256(registry).hexdigest() == "0e27792e523f5f4ad0af512481ada4c37fd078e30fb946c4fc363a490bf4ca27")
    entries = re.findall(r"\{([^{}]+)\}", registry.decode("shift_jis")); require(len(entries) == 67)
    model, model_sha, source_species = MODELS[species]; raw_model = archive["piki_model/" + model]
    require(hashlib.sha256(raw_model).hexdigest() == model_sha)
    checked = {species + ".bmd": raw_model, "pikiParms.txt": members["user/Abe/piki/pikiParms.txt"], "animmgr.txt": registry}
    clips = []
    for source_id, row in enumerate(entries):
        tokens = row.split(); require(len(tokens) >= 3 and tokens[-1] == "-1")
        name = tokens[1]; require(re.fullmatch(r"[a-z0-9_]+\.bca", name) is not None and name not in checked)
        data = archive["motion/" + name]; facts = scan_bca(data)
        keys = [[int(tokens[i]), int(tokens[i + 1])] for i in range(2, len(tokens) - 1, 2)]
        require(all(0 <= frame < facts["duration"] and 0 <= kind < 1000 for frame, kind in keys))
        clips.append(dict(facts, source_id=source_id, name=name, source_sha256=hashlib.sha256(data).hexdigest(), source_keys=keys))
        checked[name] = data
    contract = {"schema": "P2_SOURCE_PIKI_MOTIONS_1", "species": species, "source_species": source_species,
                "source_model_member": "piki_model/" + model, "source_model_sha256": model_sha,
                "source": provenance, "clips": clips, "registered_clips": 67,
                "claims": {"raw_sources_only": True, "native_animator": False, "normal_history": False,
                           "selected_scene": False, "gameplay": False, "save": False}}
    checked["motion-sources.json"] = (json.dumps(contract, indent=2) + "\n").encode("ascii")
    # Validate the entire raw closure before creating fresh private output.
    output.mkdir(parents=True, exist_ok=False)
    for name, data in checked.items():
        (output / name).write_bytes(data)
    receipt = dict(contract, files={name: {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()} for name, data in checked.items()})
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="ascii")
    print(json.dumps({"species": species, "source_clips": 67, "roles": len(checked),
                      "zero_scale_clips": [c["source_id"] for c in clips if c["authored_zero_scale_samples"]], "native_animator": False}))
    return receipt


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--iso", type=Path, required=True); parser.add_argument("--species", choices=MODELS, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(); generate(args.iso, args.species, args.output)
