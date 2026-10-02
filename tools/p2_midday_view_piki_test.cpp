#include "pc_midday_view_piki.h"
#include <cstdlib>
#include <iostream>
using namespace pc_midday;
struct Resolver : LogicalResolver {
 bool identify(const char*,RefKind,const void*,LogicalRef&,std::string&) override{return false;}
 bool validate(const char*,RefKind,const LogicalRef&,std::string&)const override{return true;}
 bool validateTyped(const FieldSchema& s,const LogicalRef& r,std::string&)const override {
  if(s.ownership!=ReferenceOwnership::Content||s.reference!=RefKind::Shape)return false;
  if(!r.owner&&!r.resource&&!r.slot)return s.nullable;
  return !r.owner&&!r.slot&&((s.targetType=="PikiShapeObject"&&r.resource==11)||(s.targetType=="Shape"&&r.resource==22));
 }
 bool resolve(const char*,RefKind,const LogicalRef&,void*&,std::string&) override{return false;}
 bool identifyHandle(const char*,RefKind,u32,LogicalRef&,std::string&)override{return false;}
 bool resolveHandle(const char*,RefKind,const LogicalRef&,u32&,std::string&)override{return false;}
};
int main(){
 int checks=0; auto check=[&](bool v){++checks;if(!v){std::cerr<<"ViewPiki control failed "<<checks<<'\n';std::exit(1);}};
 Resolver r; ActorFields f; std::string e; std::vector<FieldSchema>s;
 f["viewPiki.version"].scalar=ScalarKind::U32;f["viewPiki.version"].bits=1;
 check(view_piki_schema(f,s,e));
 for(const auto& d:s){if(f.count(d.key))continue;ActorField v;v.category=d.category;v.scalar=d.scalar;v.reference=d.reference;f[d.key]=v;}
 f["viewPiki.pikiShape"].target.resource=11;
 auto valid=[&](const ActorFields& fields){ActorBytes b;std::string error;return encode_actor_fields(fields,b,error)&&validate_view_piki(b,r,error);};
 check(valid(f)); // Null happa is legitimate, including Kinoko.
 auto g=f;g["viewPiki.happaModel"].target.resource=22;check(valid(g));
 ActorBytes b;ActorFields round;check(encode_actor_fields(g,b,e)&&decode_actor_fields(b,round,e)&&valid(round));
 g=f;g["viewPiki.pikiShape"].target={};check(!valid(g));
 g=f;g["viewPiki.pikiShape"].target.resource=22;check(!valid(g));
 g=f;g["viewPiki.happaModel"].target.resource=11;check(!valid(g));
 g=f;g["viewPiki.pikiShape"].target.owner=5;check(!valid(g));
 g=f;g["viewPiki.version"].bits=2;check(!valid(g));
 g=f;g.erase("viewPiki.lastEffectPosition.z");check(!valid(g));
 g=f;g["viewPiki.lastEffectPosition.x"].bits=0x7fc00000;check(!valid(g));
 g=f;g["viewPiki.lastEffectPosition.y"].scalar=ScalarKind::U32;check(!valid(g));
 g=f;g["unexpected"]=ActorField{};check(!valid(g));
 std::cout<<checks<<" ViewPiki extension schema controls PASS (synthetic; no engine restore claim)\n";
}
