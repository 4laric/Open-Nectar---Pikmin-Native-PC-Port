"""Verify a genuine Purple/White baseline and describe exact native buffers.

This diagnostic input is source-bank provenance, not a selected session or Scene.
The unchanged Red selected-packet tool remains source_shape171_inputs.py.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path
from import_p2_piki_source_bank import MODELS, MOTION_SOURCE


def require(value):
    if not value:
        raise ValueError("genuine species constructor input contract differs")


def generate(bank, species, output):
    model, model_sha, source_species = MODELS[species]
    receipt_bytes = (bank / "receipt-with-registry.json").read_bytes()
    require(len(receipt_bytes) <= 1024 * 1024)
    receipt = json.loads(receipt_bytes)
    require(receipt["species"] == species and receipt["source_species"] == source_species and
            receipt["source_model_member"] == "piki_model/" + model and
            receipt["source_model_sha256"] == model_sha and receipt["joints"] == 11)
    expected_source = {
        "user/Kando/piki/pikis.szs": "913a01d6f9c77a7e7b2de604c2a708ba256de9aaf890702b0b8e1b6bf038eef3",
        "user/Kando/piki/texts.szs": "04b8911efe66ec18cc83733e74aa855ed7215fb48c7dbc2544a643bf865d46d5",
        "user/Abe/piki/pikiParms.txt": "f22ae88fade54bf8f142ecc5aae4ce0c82078e6aed448d029f16b75e5a3d7996",
    }
    require({k: v["sha256"] for k, v in receipt["source"].items()} == expected_source)
    motions = {"akubi": 0, "asibumi": 1, "chatting": 3, "iraira": 21, "nigeru": 28,
               "run2": 29, "walk": 30, "wait": 31, "kizuku": 32, "rolljmp": 35,
               "hang": 36, "sagasu2": 54, "suwaru": 56, "neru": 57}
    registry = json.loads((bank / "motion-registry.json").read_text())
    require({k: v["source_id"] for k, v in registry["clips"].items()} == motions and
            set(receipt["motions"]) == set(motions))
    # Receipt metadata cannot substitute for joining the real raw bytes.
    require(hashlib.sha256((bank / (species + ".bmd")).read_bytes()).hexdigest() == model_sha)
    require(hashlib.sha256((bank / "pikiParms.txt").read_bytes()).hexdigest() == expected_source["user/Abe/piki/pikiParms.txt"])
    raw_registry = (bank / "animmgr.txt").read_bytes()
    registry_sha = hashlib.sha256(raw_registry).hexdigest()
    require(registry_sha == "0e27792e523f5f4ad0af512481ada4c37fd078e30fb946c4fc363a490bf4ca27" and
            registry["source_registry_sha256"] == registry_sha)
    entries = re.findall(r"\{([^{}]+)\}", raw_registry.decode("shift_jis"))
    require(len(entries) == 67)
    for name, source_id in motions.items():
        tokens = entries[source_id].split()
        require(tokens[1] == name + ".bca")
        keys = [[int(tokens[i]), int(tokens[i + 1])] for i in range(2, len(tokens) - 1, 2)]
        source = MOTION_SOURCE[name]; clip = registry["clips"][name]; motion = receipt["motions"][name]
        raw_clip = (bank / (name + ".bca")).read_bytes()
        require(hashlib.sha256(raw_clip).hexdigest() == source["sha256"] and
                clip["source_sha256"] == source["sha256"] == motion["source_sha256"] and
                clip["duration"] == source["duration"] == motion["duration"] and
                clip["source_registry_keys"] == keys)
    rows = []; total = 0
    require(len(receipt["files"]) == 365)
    for name, facts in receipt["files"].items():
        require(Path(name).name == name and name not in (".", ".."))
        path = (bank / name).resolve()
        require(path.stat().st_size <= 8 * 1024 * 1024)
        data = path.read_bytes(); total += len(data)
        require(len(data) == facts["bytes"] and hashlib.sha256(data).hexdigest() == facts["sha256"])
        if name.endswith(".mod"):
            require(name.startswith(species + "_") and len(data) >= 32)
            rows.append(("p2-original/piki-bodies/" + species + "/" + name,
                         facts["sha256"], len(data), path.as_posix()))
    require(total <= 128 * 1024 * 1024 and len(rows) == 171 and
            set(p.name for p in bank.glob("*.mod")) == {Path(r[0]).name for r in rows})
    header = "SHAPE171_SOURCE_BANK_INPUTS\t1\t" + species + "\t" + hashlib.sha256(receipt_bytes).hexdigest()
    text = header + "\n" + "\n".join("\t".join(map(str, row)) for row in sorted(rows)) + "\n"
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf-8", newline="\n") as file:
        file.write(text)
    print(json.dumps({"species": species, "bank_roles": 365, "models": 171, "bank_bytes": total,
                      "input_sha256": hashlib.sha256(text.encode()).hexdigest(), "selected_scene_admitted": False}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bank", type=Path, required=True)
    parser.add_argument("--species", choices=MODELS, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(); generate(args.bank, args.species, args.output)
