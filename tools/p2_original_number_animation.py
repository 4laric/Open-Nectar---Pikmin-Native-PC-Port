"""Bounded original Number BCK reader; fractional key sampling, no player/events.

Layout: J3DAnmTransformKeyData/J3DAnmKeyTableBase and
J3DAnmKeyLoader_v15::setAnmTransform. Interpolation: J3DAnimation.cpp
J3DGetKeyFrameInterpolation and calcTransform. Matrix: J3DTransform.cpp
J3DGetTranslateRotateMtx (Z*Y*X). Python arithmetic is not a claim of PPC
instruction/float/trigonometric-table bit equivalence.
"""
from dataclasses import dataclass
import bisect
import hashlib
import math
import struct

SOURCE_HASHES = {
    1: "c5041fdfbd8fa31b30f057eeb7db7450be63eba8e90d6329b93e053ade0d9c26",
    5: "e84fac4a1aa46d78525a558a1ade1834c7199e03fd3f672e2f9516127f2855e1",
}
MAX_BYTES = 4096
MAX_KEYS = 64


@dataclass(frozen=True)
class Key:
    frame: float
    value: float
    tangent_in: float
    tangent_out: float


@dataclass(frozen=True)
class Track:
    component: str
    axis: int
    count: int
    value_index: int
    tangent_type: int
    keys: tuple
    constant: float

    def sample(self, frame):
        if not math.isfinite(frame):
            raise ValueError("Non-finite sample frame")
        if self.count < 2:
            return self.constant
        if frame < self.keys[0].frame:
            return self.keys[0].value
        if frame >= self.keys[-1].frame:
            return self.keys[-1].value
        index = bisect.bisect_right([k.frame for k in self.keys], frame) - 1
        left, right = self.keys[index:index + 2]
        width = right.frame - left.frame
        t = (frame - left.frame) / width
        t2, t3 = t * t, t * t * t
        # Type0 stores one shared in/out tangent; type1 stores separate tangents.
        return ((2*t3 - 3*t2 + 1)*left.value + (t3 - 2*t2 + t)*width*left.tangent_out
                + (-2*t3 + 3*t2)*right.value + (t3 - t2)*width*right.tangent_in)


@dataclass(frozen=True)
class NumberAnimation:
    number: int
    source_sha256: str
    duration: int
    attribute: int
    rotation_scale: int
    tracks: tuple

    def descriptor(self):
        """Nine normalized tracks plus raw tracks needed for J3D quantization.

        Rotation interpolation is truncated to s32 BEFORE angleScale, then
        narrowed to s16. A radians-only player must not omit that operation.
        """
        def f32(value):
            try:
                result = struct.unpack(">f", struct.pack(">f", value))[0]
            except (OverflowError, struct.error) as error:
                raise ValueError("Descriptor value exceeds finite f32") from error
            if not math.isfinite(result):
                raise ValueError("Descriptor value exceeds finite f32")
            return result
        tracks = []
        factor = (1 << self.rotation_scale)*math.pi/32768
        for track in self.tracks:
            multiplier = factor if track.component == "rotation" else 1.0
            keys = track.keys if track.count >= 2 else (Key(0, track.constant, 0, 0),)
            tracks.append(dict(name={"scale":"s","rotation":"r","translation":"t"}[track.component]+"xyz"[track.axis],
                               source_count=track.count, source_index=track.value_index,
                               source_tangent_type=track.tangent_type,
                               keys=[dict(time=f32(k.frame),value=f32(k.value*multiplier),
                                          inTangent=f32(k.tangent_in*multiplier),outTangent=f32(k.tangent_out*multiplier)) for k in keys],
                               source_keys=[dict(time=f32(k.frame),value=f32(k.value),
                                                 inTangent=f32(k.tangent_in),outTangent=f32(k.tangent_out)) for k in keys]))
        return dict(duration=self.duration, joints=1, attribute=self.attribute,
                    # Audited paired AnimMgr text, not a loop inferred from BCK.
                    source_animmgr_loop=dict(start=10,end=30) if self.number in (1,5) else None,
                    rotation_scale=self.rotation_scale, rotation_units="radians",
                    rotation_quantization="interpolate source_keys; truncate to s32; shift angleScale; narrow signed16",
                    matrix_order="Z*Y*X; scale columns", tracks=tracks)

    def transform(self, frame):
        if isinstance(frame, bool) or not isinstance(frame, (int, float)) or not math.isfinite(frame) or not 0 <= frame < self.duration:
            raise ValueError("Number frame must be finite and in [0,41)")
        values = {name: [0.0]*3 for name in ("scale", "rotation", "translation")}
        for track in self.tracks:
            value = track.sample(frame)
            if not math.isfinite(value):
                raise ValueError("Non-finite interpolated transform")
            if track.component == "rotation":
                # calcTransform casts keyed interpolation to s32, shifts the
                # angle multiplier, then stores in J3DTransformInfo's s16.
                value = ((int(value) << self.rotation_scale) + 32768) % 65536 - 32768
            values[track.component][track.axis] = value
        return values

    def sample(self, frame):
        """Return one root 3x4 SRT matrix; retain fractional input frames."""
        values = self.transform(frame)
        rx, ry, rz = [v*math.pi/32768 for v in values["rotation"]]
        sx, cx, sy, cy, sz, cz = math.sin(rx), math.cos(rx), math.sin(ry), math.cos(ry), math.sin(rz), math.cos(rz)
        matrix = [[cz*cy, cz*sy*sx-sz*cx, cz*sy*cx+sz*sx, values["translation"][0]],
                  [sz*cy, sz*sy*sx+cz*cx, sz*sy*cx-cz*sx, values["translation"][1]],
                  [-sy, cy*sx, cy*cx, values["translation"][2]]]
        for row in matrix:
            for column in range(3):
                row[column] *= values["scale"][column]
        return matrix


def _parse_ank1(data, number=0, source_sha256=""):
    """Internal bounded layout decoder; public parse additionally pins hashes."""
    if not isinstance(data, bytes) or not 68 <= len(data) <= MAX_BYTES or data[:8] != b"J3D1bck1":
        raise ValueError("Expected bounded J3D1bck1")
    length, blocks = struct.unpack_from(">II", data, 8)
    if length != len(data) or blocks != 1 or data[32:36] != b"ANK1":
        raise ValueError("Unsupported BCK framing/block count")
    block = data[32:]
    if struct.unpack_from(">I", block, 4)[0] != len(block):
        raise ValueError("ANK1 block length mismatch")
    attribute, rotation_scale, duration, table_count, scales, rotations, translations = struct.unpack_from(">BBh4H", block, 8)
    if attribute != 2 or rotation_scale > 15 or duration != 41 or table_count != 3:
        raise ValueError("Expected Number one-joint, 41-frame ANK1")
    offsets = struct.unpack_from(">4i", block, 20)
    spans = []
    for offset, count, width in zip(offsets, (table_count, scales, rotations, translations), (18, 4, 2, 4)):
        if count > 1024 or offset < 36 or offset % (2 if width in (18, 2) else 4) or offset + count*width > len(block):
            raise ValueError("Invalid ANK1 table/value span")
        spans.append((offset, offset + count*width))
    if any(a[0] < b[1] and b[0] < a[1] for i, a in enumerate(spans) for b in spans[i+1:]):
        raise ValueError("Overlapping ANK1 tables")
    arrays = {}
    for name, count, offset, fmt in zip(("scale", "rotation", "translation"), (scales, rotations, translations), offsets[1:], ("f", "h", "f")):
        array = struct.unpack_from(">" + fmt*count, block, offset)
        if not all(math.isfinite(v) for v in array):
            raise ValueError("Non-finite ANK1 source value")
        arrays[name] = array
    tracks = []
    for axis in range(3):
        for component_index, name in enumerate(("scale", "rotation", "translation")):
            count, index, tangent = struct.unpack_from(">3H", block, offsets[0] + axis*18 + component_index*6)
            if count > MAX_KEYS or tangent not in (0, 1):
                raise ValueError("Unsupported ANK1 key count/tangent type")
            stride = 3 if tangent == 0 else 4
            span = count if count < 2 else count*stride
            if index + span > len(arrays[name]):
                raise ValueError("ANK1 key index/count exceeds value array")
            raw = arrays[name][index:index+span]
            keys = []
            constant = (1.0 if name == "scale" else 0.0) if count == 0 else raw[0]
            if count >= 2:
                for i in range(count):
                    key = raw[i*stride:(i+1)*stride]
                    if not 0 <= key[0] <= duration or (keys and key[0] <= keys[-1].frame):
                        raise ValueError("Unordered/out-of-range ANK1 key frames")
                    keys.append(Key(key[0], key[1], key[2], key[2] if tangent == 0 else key[3]))
            tracks.append(Track(name, axis, count, index, tangent, tuple(keys), constant))
    return NumberAnimation(number, source_sha256, duration, attribute, rotation_scale, tuple(tracks))


def parse(data):
    """Decode a bounded one-joint/41-frame ANK1; caller authenticates source."""
    if not isinstance(data, bytes) or len(data)>MAX_BYTES:
        raise ValueError("Number BCK input exceeds byte bound")
    digest=hashlib.sha256(data).hexdigest()
    number=next((n for n,expected in SOURCE_HASHES.items() if digest==expected),0)
    return _parse_ank1(data,number=number,source_sha256=digest)


def parse_verified(data):
    """Private fixture boundary for the two verified GPVE01rev0 resources."""
    if not isinstance(data, bytes) or len(data) > MAX_BYTES:
        raise ValueError("Number BCK input exceeds byte bound")
    digest = hashlib.sha256(data).hexdigest()
    for number, expected in SOURCE_HASHES.items():
        if digest == expected:
            return _parse_ank1(data, number, digest)
    raise ValueError("Unsupported/unverified Number BCK SHA256")
