"""Independent exact Fraction binary64 FMA/RNE oracle; no host FMA.

Only bit decoding, rational multiplication/addition, integer quotient/remainder
rounding and bit encoding are used. Reproducible seed 1261. No hardware claim.
"""
from fractions import Fraction
from pathlib import Path
import sys
import random

MASK = (1 << 52) - 1
def decode(b):
    e = (b >> 52) & 2047
    if e == 2047:
        return None
    m = (b & MASK) + ((1 << 52) if e else 0)
    power = e - 1075 if e else -1074
    x = Fraction(m) * Fraction(2) ** power
    return -x if b >> 63 else x

def encode(x, negative_zero=False):
    if not x:
        return (1 << 63) if negative_zero else 0
    sign = (1 << 63) if x < 0 else 0
    x = abs(x)
    power = x.numerator.bit_length() - x.denominator.bit_length()
    if x < Fraction(2) ** power:
        power -= 1
    unit = max(power - 52, -1074)
    q = x / Fraction(2) ** unit
    whole, rem = divmod(q.numerator, q.denominator)
    if 2 * rem > q.denominator or (2 * rem == q.denominator and whole & 1):
        whole += 1
    if whole >= 1 << 53:
        whole >>= 1
        unit += 1
    if whole >= 1 << 52:
        exponent = unit + 1075
        if exponent >= 2047:
            return None
        return sign | (exponent << 52) | (whole - (1 << 52))
    return sign | whole

def oracle(a, b, c):
    x, y, z = decode(a), decode(b), decode(c)
    if x is None or y is None or z is None:
        return None
    nz = (not x or not y) and not z and ((a >> 63) ^ (b >> 63)) and c >> 63
    return encode(x * y + z, nz)

ONE = 0x3ff0000000000000
HALF = 0x3fe0000000000000
MAX = 0x7fefffffffffffff
NEG = 1 << 63
cases = []
def add(a,b,c):
    cases.append((a,b,c))
# Exact zero sign combinations, halfway parity and tiny sticky residues.
for a in (0, NEG):
    for b in (ONE, NEG | ONE, 0, NEG):
        for c in (0, NEG): add(a,b,c)
for sign in (0, NEG):
    for a in (ONE, ONE+1, ONE+2, 1, 2, 3, 0x0010000000000000, MAX):
        for b in (ONE, HALF, ONE+1, ONE-1):
            for c in (0, 1, NEG|1, 0x3ca0000000000000, NEG|0x3ca0000000000000):
                add(a|sign,b,c)
# Product overflow rescued by actual finite addend; cancellation down to zero.
for sign in (0,NEG):
    for a,b,c in ((MAX,0x4000000000000000,MAX|NEG),
                  (0x7fe0000000000000,0x4000000000000000,MAX|NEG),
                  (ONE+1,ONE-1,ONE|NEG),
                  (ONE,ONE,ONE|NEG)):
        add(a|sign,b,c^sign)
# Overflow halfway: max + 2^970 (half an ULP at max) is overflow tie.
for sign in (0,NEG):
    for c in (0x7c8fffffffffffff,0x7c90000000000000,0x7c90000000000001):
        add(MAX|sign,ONE,c|sign)
# Stratify all finite exponent fields; mantissas/signs independently varied.
rng=random.Random(1261)
def finite():
    return (rng.getrandbits(1)<<63)|(rng.randrange(2047)<<52)|rng.getrandbits(52)
for exponent in range(2047):
    add((rng.getrandbits(1)<<63)|(exponent<<52)|rng.getrandbits(52),finite(),finite())
for _ in range(12000): add(finite(),finite(),finite())
# Close cancellation, exact rounded product negated; sticky residuals survive.
for _ in range(2000):
    a,b=finite(),finite()
    prod=encode(decode(a)*decode(b))
    if prod is not None:
        add(a,b,prod^NEG)
        add(a,b,(prod^NEG)^1)
for bad in (0x7ff0000000000000,0xfff0000000000000,0x7ff8000000000001):
    add(bad,ONE,0);add(ONE,bad,0);add(ONE,ONE,bad)
destination=Path(sys.argv[1]) if len(sys.argv)>1 else Path('goldens.tsv')
with destination.open('w',encoding='ascii') as stream:
    for a,b,c in cases:
        expected=oracle(a,b,c)
        stream.write(f'{a:016x} {b:016x} {c:016x} '+('refuse' if expected is None else f'{expected:016x}')+'\n')
print(f'Fraction/RNE oracle controls={len(cases)} seed=1261 path={destination}')
