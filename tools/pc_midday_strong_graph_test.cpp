#include "pc_midday_strong_graph.h"
#include <cstdlib>
#include <iostream>
using namespace pc_midday;
int checks=0;void check(bool good){++checks;if(!good){std::cerr<<"strong graph check failed "<<checks<<"\n";std::exit(1);}}
struct Resolver:LogicalResolver {
 bool identify(const char*,RefKind,const void*,LogicalRef&,std::string&)override{return false;}
 bool validate(const char*,RefKind,const LogicalRef&,std::string&)const override{return false;}
 bool resolve(const char*,RefKind,const LogicalRef&,void*&,std::string&)override{return false;}
 bool identifyHandle(const char*,RefKind,u32,LogicalRef&,std::string&)override{return false;}
 bool resolveHandle(const char*,RefKind,const LogicalRef&,u32&,std::string&)override{return false;}
 bool validateTyped(const FieldSchema& s,const LogicalRef& r,std::string& e)const override{
  bool valid=s.category==FieldCategory::Reference&&s.reference==RefKind::Creature&&s.targetType=="Creature"&&!r.resource&&!r.slot&&
   ((r.owner==1||r.owner==2)||(!r.owner&&s.nullable));if(!valid)e="fixture typed root refused";return valid;
 }
};
ActorField count(int value){ActorField f;f.scalar=ScalarKind::S32;f.bits=u32(value);return f;}
ActorField ref(uint64_t owner){ActorField f;f.category=FieldCategory::Reference;f.reference=RefKind::Creature;f.target.owner=owner;return f;}
FieldSchema strong(const char* key){return FieldSchema::ref(key,RefKind::Creature,true,"Creature",ReferenceOwnership::AnyLive,"",ReferenceStrength::StrongCreature);}
int main(){
 Resolver resolver;std::string e;ActorFields one{{"creature.referenceCount",count(1)},{"first",ref(2)},{"alias",ref(2)},{"distinct",ref(2)},{"weak",ref(1)}};
 ActorFields two{{"creature.referenceCount",count(2)},{"back",ref(1)}};ActorFields global{{"nullable",ref(0)}};
 std::vector<FieldSchema> schemaOne{FieldSchema::value("creature.referenceCount",ScalarKind::S32),strong("first"),strong("alias"),strong("distinct"),FieldSchema::ref("weak",RefKind::Creature,true,"Creature")};
 std::vector<FieldSchema> schemaTwo{FieldSchema::value("creature.referenceCount",ScalarKind::S32),strong("back")};std::vector<FieldSchema> schemaGlobal{strong("nullable")};
 constexpr uint32_t Jobs=0x6818;
 std::vector<StrongPayload> payloads{{ReferenceOwner::Actor,1,&one,&schemaOne,&resolver},{ReferenceOwner::Actor,2,&two,&schemaTwo,&resolver},{ReferenceOwner::Global,Jobs,&global,&schemaGlobal,&resolver}};
 auto storage=[](ReferenceOwner kind,uint64_t owner,const FieldSchema& field,StrongStorage& out,std::string& error){
  out.kind=kind;out.owner=owner;out.member="target";
  if(field.key=="first"||field.key=="alias")out.slot=7;
  else if(field.key=="distinct")out.slot=8;
  else if(field.key=="back")out.slot=0;
  else if(field.key=="nullable")out.slot=3;
  else if(field.key=="weak")out.slot=9;
  else {error="unknown canonical storage slot";return false;}return true;
 };
 std::vector<StrongReference> output;std::map<uint64_t,int32_t> observed{{1,1},{2,2}},counts;
 check(extractStrongGraph(payloads,storage,observed,{Jobs},output,counts,e));
 check(counts==observed&&output.size()==4);
 check(output[0].owner==1&&output[0].field==1&&output[0].target==2&&output[1].owner==1&&output[1].field==2&&output[1].target==2);
 check(output[3].ownerKind==ReferenceOwner::Global&&output[3].owner==Jobs&&!output[3].target);
 for(int fault=0;fault<13;++fault){
  auto badOne=one;auto badTwo=two;auto badSchema=schemaOne;auto badPayload=payloads;
  badPayload[0].fields=&badOne;badPayload[0].schema=&badSchema;badPayload[1].fields=&badTwo;
  StrongStorageResolver selected=storage;
  switch(fault){
  case 0:badOne["alias"]=ref(1);break;
  case 1:selected={};break;
  case 2:badSchema[1].strength=ReferenceStrength(999);break;
  case 3:badOne["first"].target.resource=99;break;
  case 4:badOne.erase("weak");break;
  case 5:badPayload.erase(badPayload.begin()+1);break;
  case 6:badOne["creature.referenceCount"]=count(3);break;
  case 7:badPayload.pop_back();break;
  case 8:selected=[&](ReferenceOwner k,uint64_t o,const FieldSchema& s,StrongStorage& r,std::string& err){if(!storage(k,o,s,r,err))return false;r.owner=2;return true;};break;
  case 9:badOne["first"].target.slot=1;break;
  case 10:badSchema[4].strength=ReferenceStrength::StrongCreature;break;
  case 11:badPayload.push_back(badPayload.front());break;
  case 12:selected=[](ReferenceOwner,uint64_t,const FieldSchema&,StrongStorage&,std::string& err){err="missing actual factory storage identity";return false;};break;
  }
  output={{ReferenceOwner::Actor,777,777,777}};counts={{777,777}};
  check(!extractStrongGraph(badPayload,selected,observed,{Jobs},output,counts,e)&&output.size()==1&&output.front().owner==777&&counts==std::map<uint64_t,int32_t>{{777,777}});
 }
 // Same null physical slot may be visited twice but contributes no invented
 // count. A new, separate null storage still gets its own canonical field.
 global["nullableAlias"]=ref(0);schemaGlobal.push_back(strong("nullableAlias"));
 auto aliasStorage=[&](ReferenceOwner k,uint64_t o,const FieldSchema& s,StrongStorage& r,std::string& err){if(s.key=="nullableAlias"){r={k,o,3,"target"};return true;}return storage(k,o,s,r,err);};
 check(extractStrongGraph(payloads,aliasStorage,observed,{Jobs},output,counts,e)&&output.size()==4&&counts==observed);
 auto distinctNull=[&](ReferenceOwner k,uint64_t o,const FieldSchema& s,StrongStorage& r,std::string& err){if(s.key=="nullableAlias"){r={k,o,4,"target"};return true;}return storage(k,o,s,r,err);};
 check(extractStrongGraph(payloads,distinctNull,observed,{Jobs},output,counts,e)&&output.size()==5&&counts==observed);
 std::cout<<checks<<" canonical strong storage graph controls PASS\n";
}
