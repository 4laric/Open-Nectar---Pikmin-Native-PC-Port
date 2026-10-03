# Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
# Developed at SunSoft, a Sun Microsystems, Inc. business.
# Permission to use, copy, modify, and distribute this software is freely
# granted, provided that this notice is preserved.
# Independent exact-rational expression of original GPVE01 binary64 instructions.
# Sun fdlibm coefficient facts; original permissive notice retained in native module.
from fractions import Fraction as Q
import struct, hashlib, json, sys
from pathlib import Path
D=lambda h:struct.unpack('>d',bytes.fromhex(h))[0]
H=lambda x:struct.unpack('>Q',struct.pack('>d',x))[0]>>32
R=lambda x:float(x) # CPython Fraction conversion: exact nearest/even binary64.
A=lambda x,y:R(Q(x)+Q(y))
S=lambda x,y:R(Q(x)-Q(y))
M=lambda x,y:R(Q(x)*Q(y))
F=lambda x,y,z:R(Q(x)*Q(y)+Q(z))
MS=lambda x,y,z:F(x,y,-z)
NMS=lambda x,y,z:-MS(x,y,z)
s1,s2,s3,s4,s5,s6=map(D,['bfc5555555555549','3f8111111110f8a6','bf2a01a019c161d5','3ec71de357b1fe7d','be5ae5e68a2b9ceb','3de5d93a5acfd57c'])
c1,c2,c3,c4,c5,c6=map(D,['3fa555555555554c','bf56c16c16c15177','3efa01a019cb1590','be927e4f809c52ad','3e21ee9ebdb4b1c4','bda8fae9be8838d4'])
p1,t1,p2,t2,p3,t3,inv,tau=map(D,['3ff921fb54400000','3dd0b4611a626331','3dd0b4611a600000','3ba3198a2e037073','3ba3198a2e000000','397b839a252049c1','3fe45f306dc9c883','401921fb60000000'])
def ks(x,y,tail):
 if H(abs(x))<0x3e400000:return x
 z=M(x,x);v=M(z,x);r=F(z,F(z,F(z,F(s6,z,s5),s4),s3),s2)
 if not tail:return F(v,F(z,r,s1),x)
 return S(x,NMS(s1,v,MS(z,MS(.5,y,M(v,r)),y)))
def kc(x,y):
 ix=H(abs(x))
 if ix<0x3e400000:return 1.
 z=M(x,x);r=M(z,F(z,F(z,F(z,F(z,F(c6,z,c5),c4),c3),c2),c1));corr=MS(z,r,M(x,y))
 if ix<0x3fd33333:return S(1.,MS(.5,z,corr))
 qx=.28125 if ix>0x3fe90000 else D(f'{ix-0x00200000:08x}00000000')
 return S(S(1.,qx),S(MS(.5,z,qx),corr))
def reduce(x):
 ix=H(x)
 if ix<=0x3fe921fb:return 0,x,0.
 if ix<0x4002d97c:
  z=S(x,p1);tail=t1
  if ix==0x3ff921fb:z=S(z,p2);tail=t2
  y=S(z,tail);return 1,y,S(S(z,y),tail)
 n=int(F(inv,x,.5));r=NMS(p1,float(n),x);w=M(t1,float(n));y=S(r,w)
 if ix==[0x3ff921fb,0x400921fb,0x4012d97c,0x401921fb][n-1]:
  j=ix>>20
  if j-((H(y)>>20)&0x7ff)>16:
   old=r;prod=M(float(n),p2);r=S(old,prod);w=MS(t2,float(n),S(S(old,r),prod));y=S(r,w)
   if j-((H(y)>>20)&0x7ff)>49:
    old=r;prod=M(float(n),p3);r=S(old,prod);w=MS(t3,float(n),S(S(old,r),prod));y=S(r,w)
 return n,y,S(S(r,y),w)
def pair(i):
 x=R(Q(M(float(i),tau))/2048);n,y,t=reduce(x)
 if n==0:return ks(y,t,H(x)>0x3fe921fb),kc(y,t)
 if n==1:return kc(y,t),-ks(y,t,True)
 if n==2:return -ks(y,t,True),-kc(y,t)
 if n==3:return -kc(y,t),ks(y,t,True)
 return ks(y,t,True),kc(y,t)
# Direct Fraction -> binary32 rounding, avoiding double-rounding oracle output.
def f32(q):
 negative=q<0
 if not q:return 0
 if negative:q=-q
 e=q.numerator.bit_length()-q.denominator.bit_length()
 if q<Q(2)**e:e-=1
 scaled=q/(Q(2)**(e-23));m,r=divmod(scaled.numerator,scaled.denominator)
 if 2*r>scaled.denominator or (2*r==scaled.denominator and m&1):m+=1
 if m==1<<24:m>>=1;e+=1
 return (negative<<31)|((e+127)<<23)|(m&0x7fffff)
def word(d):
 if not d:return (struct.unpack('>Q',struct.pack('>d',d))[0]>>63)<<31
 return f32(Q(d))
b=bytearray()
for i in range(2048):
 for d in pair(i):b+=struct.pack('<I',word(d))
p=Path(sys.argv[1]) if len(sys.argv)>1 else Path('oracle-table.bin');p.write_bytes(b)
print(json.dumps({'pairs':2048,'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest(),'first':b[:8].hex(),'quarter512':b[4096:4104].hex(),'oracle':'Fraction exact binary64 instructions + independent Fraction to binary32 rounding'}))
