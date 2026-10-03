"""Independent original makeSRT and SDK concat translation Fraction/RNE oracle.
Input LUT is the independent original binary64-instruction oracle from the
initializer owner, never a production trig dump. No source-owner/hardware claim.
"""
from fractions import Fraction as Q
from pathlib import Path
import struct, hashlib, random, sys
table=Path(sys.argv[1]) if len(sys.argv)>1 else Path('oracle-table.bin')
raw=table.read_bytes()
assert hashlib.sha256(raw).hexdigest()=='2d2d4f4bfc49d8884ca357b7aebb6083394229d10ef32c46c14973e4045503d0'
pairs=list(struct.iter_unpack('<II',raw))
def val(b):
 e=(b>>23)&255
 if e==255:raise ValueError('nonfinite')
 m=(b&0x7fffff)+(0x800000 if e else 0)
 q=Q(m)*Q(2)**(e-150 if e else -149)
 return -q if b>>31 else q
def round32(q,negative_zero=False):
 if not q:return 0x80000000 if negative_zero else 0
 sign=0x80000000 if q<0 else 0;q=abs(q)
 e=q.numerator.bit_length()-q.denominator.bit_length()
 if q<Q(2)**e:e-=1
 unit=max(e-23,-149);scaled=q/Q(2)**unit;m,r=divmod(scaled.numerator,scaled.denominator)
 if 2*r>scaled.denominator or (2*r==scaled.denominator and m&1):m+=1
 if m>=1<<24:m>>=1;unit+=1
 if m>=1<<23:
  exponent=unit+150
  if exponent>=255:raise ValueError('overflow')
  return sign|(exponent<<23)|(m-(1<<23))
 return sign|m
def mul(a,b):return round32(val(a)*val(b),bool((a^b)>>31))
def add(a,b):return round32(val(a)+val(b),bool(a>>31 and b>>31))
def fm(a,b,c):
 x,y,z=val(a),val(b),val(c)
 return round32(x*y+z,bool((not x or not y) and not z and ((a^b)>>31) and c>>31))
def neg(a):return a^0x80000000
ONE=0x3f800000;SCALE=0x43a2f983
def lookup(face):
 negative=val(face)<0;scaled=mul(face&0x7fffffff,SCALE)
 i=int(val(scaled))&2047;s,c=pairs[i]
 return (neg(s) if negative else s),c,i
def matrix(face,pos):
 sx,cx,_=lookup(0);sy,cy,index=lookup(face);sz,cz,_=lookup(0)
 # Register schedule independently transcribed from DOL8042847c..80428530.
 f8=mul(cy,sz);f4=mul(sx,cy);f31=mul(cx,sz);f13=mul(sx,sy)
 f3=mul(sx,sz);f7=neg(sy);f2=mul(cx,cy)
 f5=mul(cy,cz);f12=mul(cx,cz);f0=mul(sx,cz)
 f3=fm(f12,sy,f3);f0=fm(f31,sy,neg(f0))
 f6=fm(f13,cz,neg(f31));f5b=fm(f13,sz,f12)
 return [mul(ONE,f5),mul(ONE,f6),mul(ONE,f3),pos[0],mul(ONE,f8),mul(ONE,f5b),mul(ONE,f0),pos[1],mul(ONE,f7),mul(ONE,f4),mul(ONE,f2),pos[2]],index
def world(m,v):
 # PSMTXConcat translation column: ps_muls0, ps_madds1, ps_madds0,
 # ps_madds1(Unit01). Not the PSMTXMultVec split-lane sum.
 return [fm(m[r+3],ONE,fm(m[r+2],v[2],fm(m[r+1],v[1],mul(m[r],v[0])))) for r in (0,4,8)]
def vector_phase(m,v):
 return [add(fm(m[r+2],v[2],mul(m[r],v[0])),fm(m[r+3],ONE,mul(m[r+1],v[1]))) for r in (0,4,8)]
cases=[]
def put(face,pos,local):
 m,index=matrix(face,pos)
 try:w=world(m,local);status='present'
 except ValueError:w=[0,0,0];status='refuse'
 cases.append((face,pos,local,m,status,w,index))
def number(q):return round32(Q(q))
pos=[number(Q(1234567,1024)),number(Q(-7654321,4096)),number(Q(13579,32))]
local=[0x3f123456,0xbf876543,0x40c23456]
for i in range(2048):
 face=round32(Q(i)/val(SCALE))
 put(face,pos,local);put(neg(face),pos,local)
 # Half-bin samples ensure all2048indices despite rounded face(i)*scale.
 put(round32(Q(2*i+1,2)/val(SCALE)),[neg(pos[0]),0x80000000,pos[2]],local)
for face in [0,0x80000000,0x3f800000,0xbf800000,0x40c90fdb,0xc0c90fdb,0x41234567,0xc1234567,0x4a000001]:
 put(face,pos,local);put(face,[0x80000000,0,0x80000000],[0,0x80000000,0])
 m,_=matrix(face,[0,0,0]);w=world(m,local);put(face,[neg(x) for x in w],local)
 put(face,[0x7f7fffff]*3,[0x7f7fffff]*3)
rng=random.Random(1261)
for _ in range(1000):
 face=number(Q(rng.randrange(-200000000,200000000),64))
 p=[number(Q(rng.randrange(-10000000,10000000),128)) for _ in range(3)]
 v=[number(Q(rng.randrange(-10000000,10000000),256)) for _ in range(3)]
 put(face,p,v)
# Actual pure-yaw schedule sign-zero phase controls, not a fabricated body.
differences=0
for face in [0,0x80000000,0x3f800000,0xbf800000,0x40490fdb,0xc0490fdb]:
 for ps in range(8):
  p=[0x80000000 if ps&(1<<j) else 0 for j in range(3)]
  for vs in range(8):
   v=[0x80000000 if vs&(1<<j) else 0 for j in range(3)]
   m,_=matrix(face,p)
   if world(m,v)!=vector_phase(m,v):
    differences+=1
    if differences==1:print('actual pure-yaw concat-vs-MultVec counterexample',*[f'{x:08x}' for x in [face,*p,*v,*world(m,v),*vector_phase(m,v)]])
   put(face,p,v)
dest=Path(sys.argv[2]) if len(sys.argv)>2 else Path('body-yaw-goldens.tsv')
with dest.open('w',encoding='ascii') as out:
 for face,p,v,m,status,w,index in cases:
  out.write(' '.join(f'{x:08x}' for x in [face,*p,*v,*m])+f' {status} '+' '.join(f'{x:08x}' for x in w)+'\n')
print(f'body yaw controls={len(cases)} uniqueIndices={len(set(c[-1] for c in cases))} concatVectorDifferences={differences} seed=1261 lutSHA={hashlib.sha256(raw).hexdigest()}')
