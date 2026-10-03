"""Focused synthetic math/layout tests; optional read-only legal ISO checks."""
import argparse
import dataclasses
import importlib.util
import json
import math
from pathlib import Path
import struct
import sys
import unittest
from p2_original_number_animation import Key, Track, _parse_ank1, parse, parse_verified


def synthetic():
    # Three axis tables. Shared scale1/rotation0/translation0 constant arrays.
    block = bytearray(128)
    block[:4] = b"ANK1"
    struct.pack_into(">I", block, 4, len(block))
    struct.pack_into(">BBh4H", block, 8, 2, 0, 41, 3, 1, 1, 1)
    struct.pack_into(">4i", block, 20, 36, 92, 96, 100)
    for axis in range(3):
        for component in range(3):
            struct.pack_into(">3H", block, 36+axis*18+component*6, 1, 0, 0)
    struct.pack_into(">f", block, 92, 1)
    data = bytearray(32) + block
    data[:8] = b"J3D1bck1"
    struct.pack_into(">II", data, 8, len(data), 1)
    return bytes(data)


def keyed_rotation(values,count,tangent):
    block=bytearray(synthetic()[32:])
    block.extend(bytes(64))
    struct.pack_into(">I",block,4,len(block))
    struct.pack_into(">H",block,16,len(values))
    struct.pack_into(">4i",block,20,36,92,96,176)
    struct.pack_into(">3H",block,42,count,0,tangent)
    struct.pack_into(">"+"h"*len(values),block,96,*values)
    data=bytearray(synthetic()[:32])+block
    struct.pack_into(">I",data,8,len(data))
    return bytes(data)


class Tests(unittest.TestCase):
    def test_type0_fractional_hermite(self):
        track = Track("translation", 0, 2, 0, 0, (Key(0, 0, 1, 1), Key(2, 2, 1, 1)), 0)
        self.assertAlmostEqual(track.sample(.25), .25)
        self.assertEqual(track.sample(-1), 0)
        self.assertEqual(track.sample(3), 2)

    def test_type1_distinct_tangents(self):
        track = Track("translation", 0, 2, 0, 1, (Key(0, 0, 99, 4), Key(2, 0, -2, 99)), 0)
        self.assertAlmostEqual(track.sample(1), 1.5)

    def test_parsed_key_layout_and_order(self):
        one=parse(keyed_rotation((0,0,1,2,2,1),2,0))
        two=parse(keyed_rotation((0,0,99,4,2,0,-2,99),2,1))
        self.assertEqual(one.transform(.5)["rotation"][0],0)
        self.assertEqual(two.transform(1)["rotation"][0],1)
        self.assertEqual(two.tracks[1].keys[0].tangent_in,99)
        self.assertEqual(two.tracks[1].keys[0].tangent_out,4)
        for values in ((0,0,1,0,2,1),(-1,0,1,2,2,1),(0,0,1,42,2,1)):
            with self.assertRaises(ValueError):parse(keyed_rotation(values,2,0))

    def test_signed_angles_scale_translation(self):
        clip = _parse_ank1(synthetic())
        tracks = list(clip.tracks)
        tracks[0] = dataclasses.replace(tracks[0], constant=2)
        tracks[7] = dataclasses.replace(tracks[7], constant=-16384)
        tracks[2] = dataclasses.replace(tracks[2], constant=3)
        matrix = dataclasses.replace(clip, tracks=tuple(tracks)).sample(.25)
        self.assertAlmostEqual(matrix[0][0], 0, places=12)
        self.assertAlmostEqual(matrix[1][0], -2)
        self.assertEqual(matrix[0][3], 3)

    def test_rotation_truncation_after_fractional_interpolation(self):
        clip = _parse_ank1(synthetic()); tracks = list(clip.tracks)
        tracks[1] = Track("rotation", 0, 2, 0, 0, (Key(0, -2, 0, 0), Key(2, 1, 0, 0)), 0)
        value = dataclasses.replace(clip, rotation_scale=1, tracks=tuple(tracks)).transform(1)["rotation"][0]
        self.assertEqual(value, 0)  # -0.5 truncates toward zero before shift.

    def test_frame_refusals_and_identity(self):
        clip = _parse_ank1(synthetic())
        self.assertEqual(clip.sample(0), [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0]])
        for frame in (-1, 41, math.inf, math.nan, True, "0"):
            with self.assertRaises(ValueError): clip.sample(frame)
        clip.sample(40.5)

    def test_hash_boundary(self):
        parse(synthetic())
        with self.assertRaises(ValueError): parse_verified(synthetic())
        with self.assertRaises(ValueError): parse(b"x"*4097)

    def test_descriptor(self):
        descriptor=parse(synthetic()).descriptor()
        self.assertEqual(len(descriptor["tracks"]),9)
        self.assertEqual(descriptor["tracks"][0]["name"],"sx")
        self.assertEqual(descriptor["tracks"][0]["keys"][0]["value"],1)
        self.assertEqual(descriptor["tracks"][1]["source_keys"][0]["value"],0)

    def test_malformed_layout(self):
        for offset, fmt, value in ((8,"I",0),(12,"I",2),(32,"4s",b"ANF1"),(36,"I",0),
                                   (40,"B",0),(41,"B",16),(42,"h",40),(44,"H",6),
                                   (52,"i",-1),(56,"i",36),(68,"H",65),(70,"H",99),
                                   (72,"H",2),(124,"f",math.nan)):
            data = bytearray(synthetic());struct.pack_into(">"+fmt,data,offset,value)
            with self.subTest(offset=offset), self.assertRaises(ValueError): _parse_ank1(bytes(data))
        with self.assertRaises(ValueError): _parse_ank1(synthetic()[:-1])


def legal(iso, repo_root, output):
    sys.path.insert(0, str(repo_root))
    from experimental.pikmin2_assets import disc_files, archive_files
    files = disc_files(iso)
    with iso.open("rb") as disc:
        offset, length = files["user/Abe/Pellet/us/pellet.szs"]
        disc.seek(offset); members = archive_files(disc.read(length))
    records = []
    # Independent expected signed source angle values, not sampler output.
    expected = {1: {0:(0,0),10:(546,546),20:(546,-546),30:(546,546),40:(0,15)},
                5: {0:(0,0),10:(389,389),20:(389,396),30:(389,389),40:(0,11)}}
    for number, filename in ((1,"pellet1.bck"),(5,"pellet2.bck")):
        clip = parse_verified(members[filename]);samples = {}
        assert clip.number == number
        for frame, (x,z) in expected[number].items():
            transform = clip.transform(frame)
            assert transform == dict(scale=[1,1,1],rotation=[x,0,z],translation=[0,0,0]), (number,frame,transform)
            a,b = x*math.pi/32768,z*math.pi/32768
            matrix = [[math.cos(b),-math.sin(b)*math.cos(a),math.sin(b)*math.sin(a),0],
                      [math.sin(b),math.cos(b)*math.cos(a),-math.cos(b)*math.sin(a),0],
                      [0,math.sin(a),math.cos(a),0]]
            actual = clip.sample(frame)
            assert all(abs(actual[r][c]-matrix[r][c])<1e-12 for r in range(3) for c in range(4))
            samples[str(frame)] = actual
        assert clip.sample(9.5) != clip.sample(9) and clip.sample(9.5) != clip.sample(10)
        assert all(t.count==1 and t.constant==1 for t in clip.tracks if t.component=="scale")
        assert all(t.count==1 and t.constant==0 for t in clip.tracks if t.component=="translation")
        assert all(t.keys[-1].frame==41 for t in clip.tracks if t.count>1)
        assert all(clip.transform(40.5)["translation"][i]==0 for i in range(3))
        with unittest.TestCase().assertRaises(ValueError):clip.sample(41)
        records.append(dict(number=number,source_sha256=clip.source_sha256,duration=clip.duration,
                            attribute=clip.attribute,rotation_scale=clip.rotation_scale,
                            tracks=[dataclasses.asdict(t) for t in clip.tracks],samples=samples,
                            descriptor=clip.descriptor(), source_key_endpoint=41,
                            loop_start=10,loop_end=30))
    if output:
        output = output.resolve();private = repo_root.resolve()/"output"
        if not output.is_relative_to(private) or output==private:raise ValueError("Legal evidence must remain under repo-root/output")
        output.mkdir(parents=True,exist_ok=False)
        (output/"receipt.json").write_text(json.dumps(dict(records=records,native_player=False,
            limitations=["Python float/trig results do not claim PPC bit equivalence.","No native animation/gameplay/SAVE acceptance."]),indent=2)+"\n")
    print("legal Number BCK: pinned hashes, frame0/10/20/30/40 matrices, source key tracks and fractional9.5 PASS")


if __name__ == "__main__":
    parser = argparse.ArgumentParser();parser.add_argument("--iso",type=Path);parser.add_argument("--repo-root",type=Path);parser.add_argument("--output",type=Path)
    args = parser.parse_args()
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Tests))
    if not result.wasSuccessful():raise SystemExit(1)
    if args.iso:
        if not args.repo_root:parser.error("--iso requires --repo-root")
        legal(args.iso,args.repo_root,args.output)
