"""Stage genuine source AIConstants and TimeMgr inputs as a fresh supplement.

The selected descriptor owner merges the two roles into an immutable successor.
No source gravity, clock state or runtime/Scene admission is manufactured here.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
from make_p2_piki_jpa_inputs import ROOT_INPUTS, REQUIRED_PARENT, bounded, full_sha

SOURCES=(
 ("user/Kando/aiConstants.txt","p2-original/system/aiConstants.txt",87,"0bf964d8d4c975ef021d83180a3c81e6264c9bdd18d75035af4a48995a5a41a5"),
 ("user/Abe/time/time.ini","p2-original/system/time.ini",482,"49e725e8b95bbf3a10e46c5df1e2d59b5e6ce3b1a0f97c3758bf56b945a9f1af"),
)

def sha(data):return hashlib.sha256(data).hexdigest()

def generate(iso, selected, output, *, sources=SOURCES, kind="SYSTEM"):
    from experimental.pikmin2_assets import disc_files
    if output.resolve()==selected.resolve() or selected.resolve() in output.resolve().parents:
        raise ValueError("system supplement must preserve selected parent")
    descriptor=bounded(selected/"p2-original-session.txt",16*1024*1024)
    lines=descriptor.decode("ascii").splitlines();header=lines[0].split() if lines else []
    if len(header)!=4 or header[:2]!=["P2_ORIGINAL_SESSION","1"] or not full_sha(header[2]) or not header[3].isdigit() or lines[-1]!="END":
        raise ValueError("system input selected parent framing")
    count=int(header[3])
    if not 5<=count<=65536 or len(lines)!=count+2:raise ValueError("system input selected parent census")
    parent={};total=0;previous=""
    for line in lines[1:-1]:
        words=line.split()
        if len(words)!=2:raise ValueError("system input parent role framing")
        role,digest=words;parts=PurePosixPath(role)
        if (not(role.startswith(("p2-original/","assets/")) or role in ROOT_INPUTS) or len(role)>512 or
            re.fullmatch(r"[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*",role) is None or str(parts)!=role or
            any(p in (".","..") for p in parts.parts) or not full_sha(digest) or role in parent or (previous and role<=previous)):
            raise ValueError("system input parent role invalid")
        data=bounded(selected/role,64*1024*1024);total+=len(data)
        if total>2*1024*1024*1024 or sha(data)!=digest:raise ValueError("system input parent bytes differ")
        parent[role]=digest;previous=role
    if not REQUIRED_PARENT.issubset(parent):raise ValueError("system input required parent roles absent")
    catalog=disc_files(iso);checked={};source_facts={}
    with iso.open("rb") as disc:
        for member,role,size,digest in sources:
            if role in parent:raise ValueError("system source role already selected")
            offset,actual_size=catalog[member]
            if actual_size!=size:raise ValueError("genuine system source size differs")
            disc.seek(offset);data=disc.read(size)
            if len(data)!=size or sha(data)!=digest:raise ValueError("genuine system source digest differs")
            checked[role]=data;source_facts[role]={"member":member,"offset":offset,"bytes":size,"sha256":digest}
    receipt={"contract":"P2_SOURCE_"+kind+"_INPUTS_1","campaign_sha256":header[2],"parent_descriptor_sha256":sha(descriptor),"parent_roles":count,
             "roles":source_facts,"native_system":False,"native_time_mgr":False,"selected_scene":False,"gameplay":False,"save":False}
    output.mkdir(parents=True,exist_ok=False)
    for role,data in checked.items():
        target=output/role;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    (output/"selected-roles.txt").write_text("".join(role+" "+sha(data)+"\n" for role,data in sorted(checked.items())),encoding="ascii")
    (output/"receipt.json").write_text(json.dumps(receipt,indent=2)+"\n",encoding="ascii")
    print("SOURCE_"+kind+"_INPUTS_PASS",len(checked),"parent_roles",count)
    return receipt

if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    for name in ("iso","selected","output"):p.add_argument("--"+name,type=Path,required=True)
    a=p.parse_args();generate(a.iso,a.selected,a.output)
