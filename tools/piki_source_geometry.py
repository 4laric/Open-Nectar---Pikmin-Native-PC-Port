"""Capture genuine Piki geometry before pose baking; no native draw authority.

Private adapter snapshot of experimental.pikmin2_convert.decode, with the
final bake omitted. All primitive/material validation remains unchanged.
Snapshot function SHA-256: 95b8a23adf2f401d322da90ec3b082f1b1f592daf887c69add1159201b89da03
The shared strict converter is unchanged. This adapter accepts only the two
pinned genuine species models, and requires their actual complete draw table.
"""
import hashlib, math, struct
from experimental.pikmin2_convert import blocks, diffuse_slot, pixel_state, u16, u32, unpack
from experimental.pikmin2_skinning import draw_matrices
from import_p2_piki_source_bank import MODELS

def _capture(data, approximate_materials=False, bake_rigid=False, pose=None, draw_matrices=None,
           missing_normals='error', singular_normal='error'):
    from experimental.pikmin2_rigid import MISSING_NORMAL_MODES, SINGULAR_NORMAL_MODES
    if missing_normals not in MISSING_NORMAL_MODES:
        raise ValueError(f'Unsupported missing normals mode {missing_normals!r}')
    if singular_normal not in SINGULAR_NORMAL_MODES:
        raise ValueError(f'Unsupported singular normal mode {singular_normal!r}')
    b=blocks(data); j=b['JNT1']; d=b['DRW1']
    if draw_matrices is not None:
        if not bake_rigid or pose is not None:
            raise ValueError('Explicit draw matrices require baking without a joint pose')
        if len(draw_matrices)!=u16(d,8) or not draw_matrices:
            raise ValueError('Draw matrix count mismatch')
        if any(len(m)!=3 or any(len(row)!=4 or not all(math.isfinite(v) for v in row) for row in m) for m in draw_matrices):
            raise ValueError('Invalid explicit draw matrix')
    elif u16(b['EVP1'],8)!=0: raise ValueError('Skinned envelopes not supported')
    if not bake_rigid and (u16(j,8)!=1 or u16(d,8)!=1 or d[u32(d,12)]!=0 or u16(d,u32(d,16))!=0): raise ValueError('Only one rigid joint supported')
    jo=u32(j,12)
    if not bake_rigid and (unpack(j,'3f',jo+4)!=(1.,1.,1.) or unpack(j,'3h',jo+16)!=(0,0,0) or unpack(j,'3f',jo+24)!=(0.,0.,0.)): raise ValueError('Non-identity joint transform')
    if bake_rigid:
        from experimental.pikmin2_rigid import joint_matrices
        if draw_matrices is not None:
            matrices=draw_matrices
            draw_joints=list(range(len(matrices)))
        else:
            matrices=joint_matrices(b,pose)
            draw_joints=[u16(d,u32(d,16)+2*i) for i in range(u16(d,8))]
            if any(d[u32(d,12)+i]!=0 or joint>=len(matrices) for i,joint in enumerate(draw_joints)):
                raise ValueError('Non-rigid draw matrix')
    v=b['VTX1']; formats={}; at=u32(v,8)
    while u32(v,at)!=255:
        attr,count,kind=unpack(v,'III',at); formats[attr]=(count,kind,v[at+12]); at+=16
    offsets={9:u32(v,12),10:u32(v,16),11:u32(v,24),12:u32(v,28),
             **{13+i:u32(v,32+4*i) for i in range(8)}}
    arrays={}
    for attr,(count,kind,shift) in formats.items():
        if attr not in offsets or (attr not in (9,10,11,13) and not approximate_materials):
            raise ValueError(f'Unsupported vertex attribute {attr}')
        start=offsets[attr]; end=min([x for x in offsets.values() if x>start]+[len(v)])
        if attr in (11,12):
            if kind!=5: raise ValueError('Only RGBA8 colors supported')
            stride=4; values=[tuple(v[x:x+4]) for x in range(start,end-stride+1,stride)]
        else:
            dim=3 if attr in (9,10) else 2
            if kind not in (3,4): raise ValueError('Only S16/F32 vertex data supported')
            fmt=('h' if kind==3 else 'f')*dim; stride=struct.calcsize('>'+fmt)
            values=[tuple(z/(2**shift) if kind==3 else z for z in unpack(v,fmt,x)) for x in range(start,end-stride+1,stride)]
        arrays[attr]=values
    s=b['SHP1']; shapes=[]
    # Weighted Groink draw packets also carry TEX2MTXIDX. Matrix-selected
    # texture animation is omitted in the explicit material approximation.
    texture_matrix_attrs=range(1,9) if draw_matrices is not None else (1,)
    discarded_matrix_attrs=set()
    for si in range(u16(s,8)):
        rec=u32(s,12)+u16(s,u32(s,16)+2*si)*40
        if s[rec]!=0 and not (bake_rigid and s[rec]==3): raise ValueError('Unsupported shape matrix type')
        groups,desc,mi,di=unpack(s,'4H',rec+2); attrs=[]; at=u32(s,24)+desc
        while u32(s,at)!=255:
            attr,kind=unpack(s,'II',at); at+=8
            if (attr not in (0,9,10,11,13) and not (approximate_materials and attr in (12,*range(14,21))) and not (bake_rigid and approximate_materials and attr in texture_matrix_attrs)) or kind not in (1,2,3): raise ValueError('Unsupported display-list attribute')
            if kind==1 and attr not in ((0,*texture_matrix_attrs) if bake_rigid else (0,)): raise ValueError('Direct non-matrix attribute unsupported')
            attrs.append((attr,kind))
        triangles=[];matrix_slots={}
        for gi in range(groups):
            if bake_rigid:
                _,matrix_count,first=unpack(s,'HHI',u32(s,36)+(mi+gi)*8)
                for slot in range(matrix_count):
                    draw=u16(s,u32(s,28)+2*(first+slot))
                    if draw!=65535:
                        if draw>=len(draw_joints): raise ValueError('Invalid rigid draw reference')
                        matrix_slots[slot]=draw_joints[draw]
            size,off=unpack(s,'II',u32(s,40)+(di+gi)*8); at=u32(s,32)+off; end=at+size
            while at<end:
                op=s[at]; at+=1
                if op==0: continue
                if op not in (0x80,0x90,0x98,0xA0): raise ValueError(f'Unsupported primitive {op:#x}')
                count=u16(s,at); at+=2; verts=[]
                for _ in range(count):
                    vv={};matrix_slot=0
                    for attr,kind in attrs:
                        value=s[at] if kind in (1,2) else u16(s,at); at+=1 if kind in (1,2) else 2
                        if attr==0:
                            if bake_rigid:
                                if value%3: raise ValueError('Invalid matrix slot')
                                matrix_slot=value//3
                            elif value!=0: raise ValueError('Non-root matrix reference')
                        elif attr in texture_matrix_attrs and bake_rigid:
                            discarded_matrix_attrs.add(attr)
                        else:
                            if value>=len(arrays[attr]): raise ValueError('Vertex index out of range')
                            vv[attr]=value
                    if bake_rigid:
                        if matrix_slot not in matrix_slots: raise ValueError('Missing rigid matrix slot')
                        vv[0]=matrix_slots[matrix_slot]
                    verts.append(vv)
                if op==0x90:
                    if count%3: raise ValueError('Partial triangle')
                    triangles.extend(verts[i:i+3] for i in range(0,count,3))
                elif op==0x80:
                    if count%4: raise ValueError('Partial quad')
                    for i in range(0,count,4): triangles.extend(([verts[i],verts[i+1],verts[i+2]],[verts[i],verts[i+2],verts[i+3]]))
                elif op==0x98:
                    triangles.extend([verts[i+(i%2)],verts[i+1-(i%2)],verts[i+2]] for i in range(count-2))
                else: triangles.extend([verts[0],verts[i],verts[i+1]] for i in range(1,count-1))
            if at!=end: raise ValueError('Primitive packet overrun')
        shapes.append(triangles)
    hierarchy=b['INF1']; at=u32(hierarchy,20); mat=0; mapping={}; order=[]
    while True:
        typ,idx=unpack(hierarchy,'HH',at); at+=4
        if typ==0: break
        if typ==0x11: mat=idx
        if typ==0x12:
            mapping[idx]=mat;order.append(idx)
    if sorted(order)!=list(range(len(shapes))): raise ValueError('Expected each shape once in draw hierarchy')
    b['_draw_order']=order
    m=b['MAT3']; materials=[]; states=[]
    for i in range(u16(m,8)):
        r=u32(m,12)+u16(m,u32(m,16)+2*i)*332
        if not approximate_materials and m[u32(m,88)+m[r+4]]!=1: raise ValueError('Only single-stage materials supported')
        if not approximate_materials:
            indirect=u32(m,24)
            if indirect and (m[indirect+i*312] or m[indirect+i*312+1]): raise ValueError('Indirect textures unsupported')
            if any(u16(m,r+132+2*k)!=65535 for k in range(1,8)): raise ValueError('Multiple texture inputs unsupported')
        slot=diffuse_slot(m,r) if approximate_materials else 0
        tex=u16(m,r+132+2*slot); tex=-1 if tex==65535 else u16(m,u32(m,72)+tex*2)
        materials.append(tex)
        states.append(pixel_state(m,r))
    b['_render_states']=[states[mapping[i]] for i in range(len(shapes))]
    if bake_rigid:
        from experimental.pikmin2_rigid import bake
        # Primitive strips/fans share vertex dictionaries; bake each reference independently.
        shapes=[[[dict(v) for v in tri] for tri in shape] for shape in shapes]
        # Preserve authenticated source draw indices and vertex/normal references.
        b['_missing_normals']=missing_normals
        b['_singular_normal']=singular_normal
    # Array blocks carry alignment padding; only referenced entries are vertices.
    for attr in arrays:
        used=[v[attr] for tris in shapes for tri in tris for v in tri if attr in v]
        arrays[attr]=arrays[attr][:max(used)+1] if used else []
    if draw_matrices is not None:
        b['_discarded_matrix_attributes']=sorted(discarded_matrix_attrs)
    return b,arrays,shapes,[materials[mapping[i]] for i in range(len(shapes))]


def capture(model, species):
    if species not in MODELS or hashlib.sha256(model).hexdigest()!=MODELS[species][1]:
        raise ValueError("genuine source geometry model binding differs")
    skeleton=blocks(model)
    result=_capture(model,True,bake_rigid=True,draw_matrices=draw_matrices(skeleton))
    _,arrays,shapes,_=result
    for shape in shapes:
        for triangle in shape:
            for vertex in triangle:
                if not all(key in vertex for key in (0,9,10)) or not 0<=vertex[0]<9:
                    raise ValueError("genuine Piki used geometry draw contract differs")
    return result
