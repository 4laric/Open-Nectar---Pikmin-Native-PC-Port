"""Import genuine Purple/White counterparts of the qualified Red171 bank.

Run with the root repository on PYTHONPATH. Legal assets and generated output
must remain private. This baseline has fourteen motion clips; it does not claim
complete source FSM/effects, selected Scene authority or gameplay acceptance.
"""
import argparse
from pathlib import Path
import hashlib,json,struct,math,re
MODELS={
 "purple":("piki_p2_black.bmd","d54d31dbf568d476861a3f02b6392eb0d405b0cb75d66101921fd074e9474770",3),
 "white":("piki_p2_white.bmd","a971c6ac48e04a6f99333cacc1f57faf1ebfa13db71c5a31e93c6ce293bb376d",4),
}

MOTION_SOURCE = {'wait': {'sha256': 'a6d37d0f572e21b0016e235e7b398b4bc6b0a30a276cd1b9bc7d15001baec2e3', 'duration': 20}, 'run2': {'sha256': 'b78ba9eadbac17892f462114a71b6eff3af0780741bad62f1bf3722349714b28', 'duration': 40}, 'hang': {'sha256': 'fc893412ba75d62a951f1f2924bc6952ba21b803f97bb2c61baca6e29bb1419b', 'duration': 20}, 'rolljmp': {'sha256': 'c89360d460b26d1efe04983597f609d50086d68c836ead0d7fbe8c9c18c454b1', 'duration': 14}, 'kizuku': {'sha256': '289c9a0ef48956ca9031609be1f69f9f1e23bf554135baf36dd5aed75ce8b0bc', 'duration': 16}, 'walk': {'sha256': 'a7213c06444fd702de2503573b6c67691f90d0be80b1dd61c7651deaa307eb93', 'duration': 40}, 'asibumi': {'sha256': '4684254de3f04edb758cfe0d1552650b239ff758fe919d2f45216d8922f95734', 'duration': 40}, 'nigeru': {'sha256': '9561d0cd8001e117d11e76b0e86cc4160a20a549990492461d6b539c0287e617', 'duration': 40}, 'akubi': {'sha256': '351eba93892d58f1d0566b6737c1ba06b4019c0f7e755232a57a715b70375a3b', 'duration': 120}, 'chatting': {'sha256': 'aacef9fa8b7a00172c5c56eba2b12d49bdf4a6c0ea7406807cfb90bd363f0f31', 'duration': 130}, 'sagasu2': {'sha256': 'f15ee13d4f583f57c48f5e1993d8e1ea44833bedf9f580af029635ae41af6ed3', 'duration': 60}, 'iraira': {'sha256': '063d0ff7c1dbde6463e11af6fde757c21a7fbec59455ebc28e6866ace39cccd8', 'duration': 80}, 'suwaru': {'sha256': 'f346ff0ceb03ebe288fb694ae6ccbe6aae0835c9a9bddd30e0c674c6cb601e9b', 'duration': 90}, 'neru': {'sha256': '270d94c806ac7124139dbd93c1b7ec6980ab251b3c1171f3395c3006b9e2a980', 'duration': 90}}

def require(value):
 if not value:raise ValueError("genuine species bank source/closure mismatch")

def generate(iso,species,out,coverage="baseline"):
 from experimental.pikmin2_assets import disc_files,archive_files
 from experimental.pikmin2_convert import blocks,decode,write_model,convert
 from experimental.pikmin2_purple import bca_pose
 from experimental.pikmin2_skinning import draw_matrices
 from experimental.pikmin2_rigid import joint_matrices
 model_name,model_sha,source_species=MODELS[species]
 files=disc_files(iso);key='user/Kando/piki/pikis.szs';offset,size=files[key]
 with iso.open('rb') as f:
  f.seek(offset);raw=f.read(size);pkey='user/Abe/piki/pikiParms.txt';po,ps=files[pkey];f.seek(po);params=f.read(ps)
  tkey='user/Kando/piki/texts.szs';to,ts=files[tkey];f.seek(to);texts=f.read(ts)
 require(hashlib.sha256(raw).hexdigest()=='913a01d6f9c77a7e7b2de604c2a708ba256de9aaf890702b0b8e1b6bf038eef3')
 require(hashlib.sha256(params).hexdigest()=='f22ae88fade54bf8f142ecc5aae4ce0c82078e6aed448d029f16b75e5a3d7996')
 require(hashlib.sha256(texts).hexdigest()=='04b8911efe66ec18cc83733e74aa855ed7215fb48c7dbc2544a643bf865d46d5')
 archive=archive_files(raw);text_members=archive_files(texts)
 registry=text_members['animmgr.txt'];require(hashlib.sha256(registry).hexdigest()=='0e27792e523f5f4ad0af512481ada4c37fd078e30fb946c4fc363a490bf4ca27')
 rawmodel=archive['piki_model/'+model_name];require(hashlib.sha256(rawmodel).hexdigest()==model_sha)
 skeleton=blocks(rawmodel);joints=struct.unpack_from('>H',skeleton['JNT1'],8)[0];require(joints==11)
 out.mkdir(parents=True,exist_ok=False)
 model=out/(species+'.bmd');model.write_bytes(rawmodel);(out/'pikiParms.txt').write_bytes(params)
 receipt={'activated':False,'gameplay':False,'source_species':source_species,'species':species,'baseline':'Red171 fourteen-motion counterpart; not complete FSM',
  'source':{key:{'offset':offset,'bytes':size,'sha256':hashlib.sha256(raw).hexdigest()},pkey:{'offset':po,'bytes':ps,'sha256':hashlib.sha256(params).hexdigest()},tkey:{'offset':to,'bytes':ts,'sha256':hashlib.sha256(texts).hexdigest()}},
  'source_model_member':'piki_model/'+model_name,'source_model_sha256':model_sha,'joints':joints,'motions':{},'files':{}}
 entries=re.findall(r'\{([^{}]+)\}',registry.decode('shift_jis'));require(len(entries)==67)
 names=['wait','run2','hang','rolljmp','kizuku','walk','asibumi','nigeru','akubi','chatting','sagasu2','iraira','suwaru','neru']
 if coverage=="registered":
  names=[row.split()[1][:-4] for id,row in enumerate(entries) if id not in (19,63)]
  receipt['baseline']='65 strict converted registered clips; authored-zero 19/63 raw only, not complete animator'
  receipt['unconverted_source_clips']=[{'id':id,'name':entries[id].split()[1][:-4],'reason':'authored zero-scale requires actual retail render normal destinations'} for id in (19,63)]
 receipt['coverage']=coverage
 lines=['P2_SOURCE_PIKI_BANK_1 '+model_sha+' '+hashlib.sha256(params).hexdigest()];anchors={}
 if species=="purple" and coverage=="registered":
  # Actual untextured Purple body base comes from its TEV register0. This
  # preserves its source hue in the existing simplified material pipeline.
  material=skeleton['MAT3'];names_at=struct.unpack_from('>I',material,20)[0]
  labels=[material[names_at+struct.unpack_from('>H',material,names_at+6+4*i)[0]:].split(b'\0',1)[0] for i in range(struct.unpack_from('>H',material,names_at)[0])]
  require(labels==[b'body1',b'eye1'])
  body_color=struct.unpack_from('>4h',material,struct.unpack_from('>I',material,80)[0]);require(all(0<=v<=255 for v in body_color))
  material_colors=[body_color,(255,255,255,255)];receipt['source_body_color']=list(body_color)
 else:material_colors=None
 for name in names:
  clip=archive['motion/'+name+'.bca'];(out/(name+'.bca')).write_bytes(clip);duration,_=bca_pose(clip,0,joints,True);frames=sorted(set(round(i*(duration-1)/min(11,duration-1)) for i in range(min(12,duration))));motion={'duration':duration,'frames':frames,'source_sha256':hashlib.sha256(clip).hexdigest(),'happa':[]};lines.append(f'clip {name} {duration} {motion["source_sha256"]} {len(frames)} '+' '.join(map(str,frames)))
  anchors[name]=[]
  for index,frame in enumerate(frames):
   _,pose=bca_pose(clip,frame,joints,True);matrices=draw_matrices(skeleton,pose);target=out/f'{species}_{name}_{index:02}.mod';write_model(decode(model.read_bytes(),True,bake_rigid=True,draw_matrices=matrices),target,str(model),material_colors=material_colors);world=joint_matrices(skeleton,pose);motion['happa'].append(world[8]);anchors[name].append({'frame':frame,'joints':world})
  for index,matrix in enumerate(motion['happa']):lines.append('happa '+name+' '+str(index)+' '+' '.join(format(v,'.9g') for row in matrix for v in row))
  receipt['motions'][name]=motion;print(name,duration,len(frames),flush=True)
 for growth,name in enumerate(['leaf','bud','flower']):
  source=out/(name+'.bmd');source.write_bytes(archive['happa_model/'+name+'.bmd']);convert(source,out/f'{species}_happa_{growth}.mod',True,bake_rigid=True)
 (out/'joint-anchors.json').write_text(json.dumps({'schema':1,'source_model_sha256':model_sha,'joints':11,'clips':anchors,'claim':'sampled source joint matrices; no live bone/Scene grant'},sort_keys=True))
 for file in out.iterdir():
  if file.is_file():receipt['files'][file.name]={'bytes':file.stat().st_size,'sha256':hashlib.sha256(file.read_bytes()).hexdigest()}
 (out/'bank.txt').write_text('\n'.join(lines)+'\n');receipt['files']['bank.txt']={'bytes':(out/'bank.txt').stat().st_size,'sha256':hashlib.sha256((out/'bank.txt').read_bytes()).hexdigest()};(out/'receipt.json').write_text(json.dumps(receipt,indent=2));print('ACTUAL_SOURCE_SPECIES_CONVERSION_PASS',joints,len(receipt['files']),flush=True)
 
 (out/'animmgr.txt').write_bytes(registry)
 entries=re.findall(r'\{([^{}]+)\}',registry.decode('shift_jis'));require(len(entries)==67)
 source={}
 for id,row in enumerate(entries):
  tokens=row.split();name=tokens[1][:-4]
  if name in receipt['motions']:
   keys=[(int(tokens[i]),int(tokens[i+1])) for i in range(2,len(tokens)-1,2)]
   require(all(0<=f<receipt['motions'][name]['duration'] for f,t in keys))
   source[name]={'source_id':id,'source_registry_keys':keys,'source_sha256':receipt['motions'][name]['source_sha256'],'duration':receipt['motions'][name]['duration']}
 require(len(source)==len(receipt['motions']))
 (out/'motion-registry.json').write_text(json.dumps({'schema':1,'activated':False,'source_registry_sha256':hashlib.sha256(registry).hexdigest(),'clips':source},indent=2))
 for name in ['animmgr.txt','motion-registry.json']:
  file=out/name;receipt['files'][name]={'bytes':file.stat().st_size,'sha256':hashlib.sha256(file.read_bytes()).hexdigest()}
 (out/'receipt-with-registry.json').write_text(json.dumps(receipt,indent=2))
 print('AUTHENTIC_REGISTRY_PASS',len(source),flush=True)
 if coverage=="registered":
  for id in (19,63):
   name=entries[id].split()[1];data=archive['motion/'+name];(out/name).write_bytes(data)
   receipt['files'][name]={'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
  receipt['registered_clips']=67;receipt['converted_clips']=65;receipt['model_count']=len(list(out.glob('*.mod')))
  (out/'receipt-with-registry.json').write_text(json.dumps(receipt,indent=2))
 else:require(len(receipt['files'])==365 and len(list(out.glob('*.mod')))==171)
 return receipt

if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--coverage',choices=['baseline','registered'],default='baseline');parser.add_argument('--iso',type=Path,required=True);parser.add_argument('--species',choices=MODELS,required=True);parser.add_argument('--output',type=Path,required=True)
 args=parser.parse_args();generate(args.iso,args.species,args.output,args.coverage)
