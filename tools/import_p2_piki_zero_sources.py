"""Export exact authenticated raw zero-scale tracks and unbaked Piki geometry.

This supplies source data for the actual Body owner's per-viewport normal
history. It generates no normals or MODs and grants no rendering/Scene authority.
"""
import argparse, hashlib, json, struct
from pathlib import Path
from import_p2_piki_source_bank import MODELS
from import_p2_piki_motion_sources import scan_bca

CLIPS={19:("grow_up2", "af93277a42109dfec96bebc8e99a34fdb798b261c1ebcd8d89cdcd58ce016401",35),
       63:("suwareru", "964f9682a0fd3c249b78b2249d089a30e7304752f7254b85b41f9896d1c3edf0",70)}


def raw_frame(data, source_id, frame):
    if source_id not in CLIPS:
        raise ValueError("unsupported zero-scale source ID")
    name, sha, duration=CLIPS[source_id]
    if hashlib.sha256(data).hexdigest()!=sha:
        raise ValueError("zero-scale raw source digest differs")
    facts=scan_bca(data)
    if facts["duration"]!=duration or type(frame)!=int or not 0<=frame<duration:
        raise ValueError("zero-scale raw source frame differs")
    block=data[32:];table,scales,rotations,translations=struct.unpack_from(">4I",block,20)
    joints=[]
    for joint in range(11):
        values={"scale":[],"rotation":[],"translation":[]}
        for axis in range(3):
            for component,(key,offset,fmt,size) in enumerate((("scale",scales,"f",4),("rotation",rotations,"h",2),("translation",translations,"f",4))):
                length,index=struct.unpack_from(">HH",block,table+joint*36+axis*12+component*4)
                values[key].append(struct.unpack_from(">"+fmt,block,offset+(index+min(frame,length-1))*size)[0])
        joints.append(values)
    return joints


def generate(source_bank,species,output):
    # Lazy converter imports permit raw-frame inspection independently.
    from piki_source_geometry import capture
    model=(source_bank/(species+".bmd")).read_bytes()
    blocks,arrays,shapes,materials=capture(model,species)
    drw=blocks["DRW1"];count=struct.unpack_from(">H",drw,8)[0]
    flags,indices=struct.unpack_from(">2I",drw,12)
    geometry={"schema":"P2_SOURCE_PIKI_GEOMETRY_1","species":species,
              "model_sha256":MODELS[species][1],"draw_entries":[{"flag":drw[flags+i],"index":struct.unpack_from(">H",drw,indices+2*i)[0]} for i in range(count)],
              "arrays":arrays,"shapes":shapes,"material_texture_indices":materials,
              "claim":"original unbaked source references; no native normal/draw authority"}
    # The actual basic calculator uses the authored hierarchy, not joint order.
    hierarchy=blocks["INF1"];at=struct.unpack_from(">I",hierarchy,20)[0]
    stack=[];current=None;parents={}
    while True:
        kind,index=struct.unpack_from(">HH",hierarchy,at);at+=4
        if kind==0:break
        if kind==1:stack.append(current)
        elif kind==2:current=stack.pop()
        elif kind==0x10:parents[index]=stack[-1] if stack else None;current=index
    joint=blocks["JNT1"];records,remap=struct.unpack_from(">2I",joint,12)
    bind=[]
    for index in range(11):
        record=struct.unpack_from(">H",joint,remap+2*index)[0] if remap else index
        offset=records+64*record
        bind.append({"parent":parents[index],"scale":struct.unpack_from(">3f",joint,offset+4),"rotation":struct.unpack_from(">3h",joint,offset+16),"translation":struct.unpack_from(">3f",joint,offset+24)})
    geometry["joint_bind"]=bind
    geometry["model_information_flags"]=struct.unpack_from(">H",hierarchy,8)[0]
    checked={"source-geometry.json":json.dumps(geometry,sort_keys=True).encode("ascii")}
    for source_id,(name,sha,duration) in CLIPS.items():
        data=(source_bank/(name+".bca")).read_bytes()
        frames=[raw_frame(data,source_id,frame) for frame in range(duration)]
        checked[name+"-raw-frames.json"]=json.dumps({"schema":"P2_SOURCE_PIKI_RAW_FRAMES_1","source_id":source_id,"name":name,"source_sha256":sha,"duration":duration,"joints":11,"frames":frames,"claim":"exact decoded source samples; no native clock or normal history"},sort_keys=True).encode("ascii")
        checked[name+".bca"]=data
    receipt={"species":species,"model_sha256":MODELS[species][1],"files":{name:{"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()} for name,data in checked.items()},
             "normal_history":False,"native_draw":False,"selected_scene":False,"gameplay":False}
    output.mkdir(parents=True,exist_ok=False)
    for name,data in checked.items():(output/name).write_bytes(data)
    (output/"receipt.json").write_text(json.dumps(receipt,indent=2)+"\n",encoding="ascii")
    print("SOURCE_ZERO_DATA_PASS",species,"frames",105,"triangles",sum(map(len,shapes)),"roles",len(checked))
    return receipt

if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source-bank",type=Path,required=True);p.add_argument("--species",choices=MODELS,required=True);p.add_argument("--output",type=Path,required=True)
    a=p.parse_args();generate(a.source_bank,a.species,a.output)
