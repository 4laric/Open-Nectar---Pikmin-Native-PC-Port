"""Install the actual source-6 shared Pom bank into a private gameplay stage.

Consumes the existing disc converter's output; writes no actor/proxy sidecar.
Materials and pose sampling remain presentation approximations. Core conversion,
source collision/events, population and SAVE belong to the Purple mechanic.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

SOURCE_HASHES = {
    "enemy/data/Pom/anim.szs": "370b945e2f29461b476ae148f58462f0635c209075fae9a830a6f89f0fcf50b3",
    "enemy/data/Pom/model.szs": "881ddbd4b0809ffdbe17f07b565f1c9ee12df9921a61ae64cbfb70b85fce6088",
    "enemy/parm/enemyParms.szs": "3618455a8561f1e1b0aad0253a75a69fae1fe3a47160d1c1efa294b0ddeb2a84",
}
CLIPS = [("wait", 1), ("dead", 40), ("type1", 30), ("type2", 30), ("type3", 40), ("type4", 20)]


def install(imported: Path, stage: Path):
    report = json.loads((imported / "flora.json").read_text())
    if report["disc_id"] != "GPVE01" or report["source_sha256"] != SOURCE_HASHES:
        raise ValueError("BlackPom actual disc resource identity mismatch")
    species = report["species"]["BlackPom"]
    if species["enemy_id"] != 6 or species["resource"] != "Pom":
        raise ValueError("BlackPom source family mismatch")
    entries = {clip["name"]: clip for clip in species["clips"]}
    pending = []
    lines = ["P2_ORIGINAL_BLACKPOM_BANK_1 6"]
    for name, duration in CLIPS:
        clip = entries[name]
        frames = [pose["frame"] for pose in clip["poses"]]
        if (clip["status"] != "converted" or clip["source_frames"] != duration
                or not frames or frames[0] != 0 or frames[-1] != duration - 1
                or any(a >= b for a, b in zip(frames, frames[1:]))):
            raise ValueError("BlackPom source clip incomplete")
        for index, pose in enumerate(clip["poses"]):
            filename = f"flora_BlackPom_{name}_{index:02}.mod"
            if pose["file"] != filename:
                raise ValueError("BlackPom pose filename mismatch")
            src = imported / "BlackPom" / filename
            if hashlib.sha256(src.read_bytes()).hexdigest() != pose["sha256"]:
                raise ValueError("BlackPom converted pose hash mismatch")
            pending.append((src, filename))
        lines += [f"clip {name} {len(frames)} {duration} flora_BlackPom_{name}",
                  "frames " + " ".join(map(str, frames))]
    # All identity/hash checks precede stage writes. Never copy legal resources
    # into a tracked source tree or another owner's active stage.
    target = stage / "assets/dataDir/courses/pikmin2room"
    if (stage / "p2-original-blackpom-bank.txt").exists():
        raise ValueError("BlackPom stage already installed")
    target.mkdir(parents=True, exist_ok=True)
    for src, filename in pending:
        if (target / filename).exists():
            raise ValueError("BlackPom pose destination already exists")
    for src, filename in pending:
        shutil.copyfile(src, target / filename)
    (stage / "p2-original-blackpom-bank.txt").write_text("\n".join(lines) + "\n")
    (stage / "blackpom-source-resource.json").write_text(json.dumps(species, indent=2) + "\n")
    return {"source": 6, "poses": len(pending), "clips": 6,
            "gameplay_qualified": False, "presentation": "sampled poses, approximate materials"}


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--imported", type=Path, required=True)
    p.add_argument("--stage", type=Path, required=True)
    args = p.parse_args()
    print(json.dumps(install(args.imported, args.stage)))
