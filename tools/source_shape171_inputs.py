"""Verify the private selected bank, then describe exact constructor buffers.

This does not admit a scene or manufacture selected SDK authority. The receipt
and textual starting-bank contract are existing owner outputs; no assets are
copied into source, and the generated TSV belongs under ignored output.
"""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("receipt", type=Path)
    parser.add_argument("bank_contract", type=Path)
    parser.add_argument("rgb04", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    receipt = json.loads(args.receipt.read_text())
    inputs = receipt["inputs"]
    assert receipt["descriptor_roles"] == 1327
    lines = args.bank_contract.read_text().splitlines()
    assert lines[0] == "P2_RETAIL_STARTING_PIKI_1 20"
    assert "bank red 364" in lines
    members = [line.split() for line in lines if line.startswith("p2-original/piki-bodies/red/")]
    assert len(members) == 364 and len({role for role, _ in members}) == 364
    models = []
    total = 0
    for role, expected in members:
        entry = inputs[role]
        path = Path(entry["source"]).resolve()
        data = path.read_bytes()
        assert len(data) == entry["bytes"] and len(data) <= 8 * 1024 * 1024
        assert hashlib.sha256(data).hexdigest() == expected == entry["sha256"]
        original = args.rgb04 / Path(role).name
        assert hashlib.sha256(original.read_bytes()).hexdigest() == expected
        total += len(data)
        if role.endswith(".mod"):
            assert len(data) >= 32
            models.append((role, expected, len(data), path.as_posix()))
    assert total <= 128 * 1024 * 1024 and len(models) == 171
    assert set(p.name for p in args.rgb04.glob("*.mod")) == {Path(role).name for role, *_ in models}
    header = "SHAPE171_CONSTRUCTOR_INPUTS\t1\t" + receipt["fingerprint"]
    text = header + "\n" + "\n".join("\t".join(map(str, row)) for row in sorted(models)) + "\n"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(text, encoding="utf-8", newline="\n")
    print(json.dumps({"bank_roles": 364, "models": 171, "bank_bytes": total,
                      "packet_fingerprint": receipt["fingerprint"],
                      "constructor_input_sha256": hashlib.sha256(text.encode()).hexdigest(),
                      "stage_or_factory_admitted": False}))


if __name__ == "__main__":
    main()
