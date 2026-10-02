#include "pc_midday_animation_context.h"
#include <cmath>
#include <cstring>
namespace pc_midday {
namespace {
constexpr const char* Data="context.data";
u32 bits(float f){u32 n;std::memcpy(&n,&f,4);return n;}
float number(u64 n){const u32 b=u32(n);float f;std::memcpy(&f,&b,4);return f;}
bool valid(const AnimationContextRecord& r,const LogicalResolver& resolver,const AnimationContentCheck& content,std::string& e){
 if(!std::isfinite(r.frame)||!std::isfinite(r.speed)){e="nonfinite animation context";return false;}
 if(!resolver.validateTyped(animationContextSchema()[0],r.data,e))return false;
 if(!content){e="animation content/phase validation unavailable";return false;}
 if(!content(r.data,r.frame,e)){if(e.empty())e="animation content/phase refused";return false;}
 return true;
}
}
bool AnimationContentIndex::declare(const LogicalRef& id,u32 frames,std::string& e){
 if(id.owner||!id.resource||id.slot||!frames||frames>1000000||frames_.size()>=262144||!frames_.emplace(id.resource,frames).second){e="invalid/duplicate animation content descriptor";return false;}
 e.clear();return true;
}
bool AnimationContentIndex::validate(const LogicalRef& id,float frame,std::string& e)const{
 if(!std::isfinite(frame)){e="nonfinite animation frame";return false;}
 if(!id.owner&&!id.resource&&!id.slot){if(frame==0){e.clear();return true;}e="uninitialized animation context has nonzero frame";return false;}
 auto i=frames_.find(id.resource);
 // An ended Pani motion may sit exactly at its terminal boundary. Do not wrap
 // or recompute that state; the installed clip bounds it inclusively.
 if(id.owner||id.slot||i==frames_.end()||frame<0||frame>float(i->second)){e="frame outside registered AnimData descriptor";return false;}
 e.clear();return true;
}
std::vector<FieldSchema> animationContextSchema(){
 return {FieldSchema::ref(Data,RefKind::Animation,true,"AnimData",ReferenceOwnership::Content),
  FieldSchema::value("context.frame",ScalarKind::F32),FieldSchema::value("context.speed",ScalarKind::F32)};
}
bool encodeAnimationContext(const AnimationContextRecord& r,const LogicalResolver& resolver,const AnimationContentCheck& content,ActorBytes& out,std::string& e){
 if(!valid(r,resolver,content,e))return false;
 ActorFields f;
 ActorField data;data.category=FieldCategory::Reference;data.reference=RefKind::Animation;data.target=r.data;f[Data]=data;
 ActorField value;value.scalar=ScalarKind::F32;value.bits=bits(r.frame);f["context.frame"]=value;
 value.bits=bits(r.speed);f["context.speed"]=value;
 return encode_actor_fields(f,out,e);
}
bool decodeAnimationContext(const ActorBytes& bytes,const LogicalResolver& resolver,const AnimationContentCheck& content,AnimationContextRecord& out,std::string& e){
 ActorFields f;if(!decode_actor_fields(bytes,f,e)||!validate_actor_fields(f,animationContextSchema(),resolver,e))return false;
 AnimationContextRecord r{f.at(Data).target,number(f.at("context.frame").bits),number(f.at("context.speed").bits)};
 if(!valid(r,resolver,content,e))return false;
 out=r;e.clear();return true;
}
}
