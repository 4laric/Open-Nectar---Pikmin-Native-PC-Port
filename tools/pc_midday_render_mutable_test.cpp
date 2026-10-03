// Header-layout/visitor control: no game loop, GL, file IO or native factory.
#include "pc_midday_render_mutable.h"
#include "Material.h"
#include <cstdio>
void Material::read(RandomAccessStream&){}
void Material::attach(){}
using namespace pc_midday;
namespace {int checks=0,failed=0;void check(bool b,const char*s){++checks;if(!b){++failed;std::printf("FAIL %s\n",s);}}
struct Resolver:LogicalResolver {
 std::map<const void*,LogicalRef> pointers;bool deny=false,wrongAlias=false,swapTexture=false,badTevSlot=false;int identifies=0,resolves=0;bool foreignResolved=false;
 bool identify(const char* k,RefKind,const void* p,LogicalRef& out,std::string& e)override{++identifies;if(!p){out={};return true;}auto it=pointers.find(p);if(it==pointers.end()){e="unregistered test pointer";return false;}out=it->second;if(wrongAlias&&std::string(k)=="tev")out.resource=99;if(badTevSlot&&std::string(k)=="tev")out.slot=1;if(swapTexture&&std::string(k).find("texture.")==0)out.slot=1-out.slot;return true;}
 bool validate(const char*,RefKind,const LogicalRef&,std::string&)const override{return !deny;}
 bool validateTyped(const FieldSchema& s,const LogicalRef&r,std::string&)const override{if(deny||s.targetType.empty())return false;if(!r.owner&&!r.resource&&!r.slot)return s.nullable;for(const auto& p:pointers)if(p.second.resource==r.resource&&p.second.slot==r.slot)return true;return false;}
 bool resolve(const char*,RefKind,const LogicalRef& r,void*& out,std::string& e)override{++resolves;if(!r.owner&&!r.resource&&!r.slot){out=nullptr;return true;}for(const auto& p:pointers)if(p.second.resource==r.resource&&p.second.slot==r.slot){out=foreignResolved?reinterpret_cast<void*>(uintptr_t(99)):const_cast<void*>(p.first);return true;}e="unknown exact test resource";return false;}
 bool identifyHandle(const char*,RefKind,u32,LogicalRef&,std::string&)override{return false;}
 bool resolveHandle(const char*,RefKind,const LogicalRef&,u32&,std::string&)override{return false;}
};}
int main(){Material materials[2];PVWTevInfo tev[2];PVWTextureData textures[2];
 for(auto& t:textures){t.mSourceAttrIndex=0;t.mTextureAttribute=nullptr;t.mAnimationFactor=255;t.mScaleX=t.mScaleY=1;t.mRotationZ=t.mTranslationX=t.mTranslationY=t.mPivotX=t.mPivotY=0;t.mTotalFrameCount=0;t.mAnimSpeed=0;}
 for(auto& t:tev){for(auto& c:t.mTevColRegs){c.mAnimatedColor.r=1;c.mAnimatedColor.g=2;c.mAnimatedColor.b=3;c.mAnimatedColor.a=4;c.mAnimFrameCount=0;c.mAnimSpeed=0;}for(auto& c:t.mKonstColors)c.set(1,2,3,4);}
 for(int i=0;i<2;++i){auto& m=materials[i];m.mFlags=MATFLAG_PVW;m.mTextureIndex=-1;m.mColourInfo.mSpeed=1;m.mTextureInfo.mTextureDataCount=2;m.mTextureInfo.mTextureData=textures;m.mTextureInfo.mScale.set(1,1,1);m.mTevInfo=&tev[i];}
 Resolver resolver;resolver.pointers[&tev[0]]={0,2,0};resolver.pointers[&tev[1]]={0,3,0};resolver.pointers[&textures[0]]={0,4,0};resolver.pointers[&textures[1]]={0,4,1};
 std::vector<RenderObservation> src{{1,101,RenderKind::Materials,2,reinterpret_cast<uintptr_t>(materials),sizeof(Material)},{2,102,RenderKind::Tev,1,reinterpret_cast<uintptr_t>(&tev[0]),sizeof(PVWTevInfo)},{3,103,RenderKind::Tev,1,reinterpret_cast<uintptr_t>(&tev[1]),sizeof(PVWTevInfo)},{4,104,RenderKind::Textures,2,reinterpret_cast<uintptr_t>(textures),sizeof(PVWTextureData)}};
 auto factory=[&](u64,u32)->LogicalResolver*{return &resolver;};RenderGraph out;std::string e;
 check(captureRenderGraph(7,src,{1,2,3,4},factory,out,e),"actual native header alias capture");
 check(out.nodes.size()==4&&out.nodes[3].payloads.size()==2&&out.links[0].textures==out.links[1].textures,"shared array captured once per physical element");
 check(out.nodes[0].payloads.size()==2&&out.nodes[1].payloads.size()==1&&out.nodes[2].payloads.size()==1,"cloned TEV preserved as two allocations");
 ActorFields fields;check(decode_actor_fields(out.nodes[3].payloads[1],fields,e)&&fields.at("mAnimationFactor").bits==255&&!fields.count("mAnimatedTexMtx.matrix.0.0"),"uninitialized unused texture matrix never read");
 resolver.wrongAlias=true;check(!captureRenderGraph(7,src,{1,2,3,4},factory,out,e)&&out.nodes.size()==4,"logical alias mismatch fails without exposing partial census");resolver.wrongAlias=false;
 resolver.swapTexture=true;check(!captureRenderGraph(7,src,{1,2,3,4},factory,out,e)&&out.links[0].textures==4,"in-range swapped texture slot identity refuses transactionally");resolver.swapTexture=false;
 resolver.badTevSlot=true;check(!captureRenderGraph(7,src,{1,2,3,4},factory,out,e),"TEV singleton nonzero canonical slot refused");resolver.badTevSlot=false;
 check(captureRenderGraph(7,src,{1,2,3,4},factory,out,e),"exact canonical slots still accepted after rejected aliases");
 resolver.deny=true;check(!captureRenderGraph(7,src,{1,2,3,4},factory,out,e),"full typed schema refusal");resolver.deny=false;
 auto bad=src;bad[0].elementSize=sizeof(Material)-1;check(!captureRenderGraph(7,bad,{1,2,3,4},factory,out,e),"wrong compiled native stride refused");
 bad=src;bad[3].address=reinterpret_cast<uintptr_t>(materials)+1;int before=resolver.identifies;check(!captureRenderGraph(7,bad,{1,2,3,4},factory,out,e)&&resolver.identifies==before,"overlap fails before native visitor dereference");
 materials[1].mTextureInfo.mTextureData=textures+1;check(!captureRenderGraph(7,src,{1,2,3,4},factory,out,e),"interior array alias is not canonical base");materials[1].mTextureInfo.mTextureData=textures;
 materials[1].mTextureInfo.mTextureDataCount=1;check(!captureRenderGraph(7,src,{1,2,3,4},factory,out,e),"native alias count drift refused");materials[1].mTextureInfo.mTextureDataCount=2;
 auto missing=[](u64,u32)->LogicalResolver*{return nullptr;};check(!captureRenderGraph(7,src,{1,2,3,4},missing,out,e),"missing scene typed resolver refused");
 check(!captureRenderGraph(7,src,{1,2,3,4},{},out,e),"absent resolver factory refused");

 check(captureRenderGraph(7,src,{1,2,3,4},factory,out,e),"fresh complete captured payload for mutable staging controls");
 {const auto good=out;std::vector<PlannedRenderState> plan;
 auto geometry=[](const RenderNode&,u32,const ActorFields&,std::string&){return true;};
 auto owned=[&](u64 id,u32 slot,std::map<std::string,const void*>& expected,std::string&){if(id==1){expected["tev"]=&tev[slot];for(u32 i=0;i<2;++i)expected["texture."+std::to_string(i)]=&textures[i];}return true;};
 check(planRenderMutable(good,geometry,factory,owned,plan,e)&&plan.size()==6,"complete actual captured schemas prepare all six element caches");
 int resolved=resolver.resolves;void* pointer=nullptr;check(plan[0].pointers.resolve("tev",RefKind::PVWTevInfo,{0,2,0},pointer,e)&&pointer==&tev[0]&&resolver.resolves==resolved,"apply cache returns exact typed address without calling external resolver again");
 auto reject=[&](const RenderGraph& bad,const char* why){check(!planRenderMutable(bad,geometry,factory,owned,plan,e)&&plan.size()==6&&plan[0].id==1,why);};
 auto bad=good;bad.links[1].slot=0;reject(bad,"duplicate canonical link refuses transactionally");bad=good;bad.links.pop_back();reject(bad,"incomplete canonical link census refuses");bad=good;bad.nodes[3].kind=RenderKind(99);reject(bad,"unknown kind refuses before payload visitors");bad=good;bad.nodes[3].payloads.pop_back();reject(bad,"missing final element payload refuses");
 auto change=[&](RenderGraph& g,size_t node,size_t slot,const std::string& key,u64 bits){ActorFields f;check(decode_actor_fields(g.nodes[node].payloads[slot],f,e),"decode mutation fixture");f.at(key).bits=bits;check(encode_actor_fields(f,g.nodes[node].payloads[slot],e),"encode mutation fixture");};
 bad=good;change(bad,0,0,"textureCount",1);reject(bad,"payload texture count cannot omit canonical owned element");bad=good;ActorFields late;check(decode_actor_fields(bad.nodes[3].payloads[1],late,e),"decode late malformed fixture");late.at("mScaleX").bits=0x7fc00000;ActorBytes refused;check(!encode_actor_fields(late,refused,e),"nonfinite float refuses at canonical encoding boundary");late.erase("mScaleX");check(encode_actor_fields(late,bad.nodes[3].payloads[1],e),"encode late incomplete schema fixture");reject(bad,"late missing scalar refuses without publishing partial caches");
 bad=good;ActorFields changed;check(decode_actor_fields(bad.nodes[0].payloads[0],changed,e),"decode alias fixture");changed.at("texture.0").target.slot=1;check(encode_actor_fields(changed,bad.nodes[0].payloads[0],e),"encode alias fixture");reject(bad,"valid in-range swapped logical texture slot refuses before resolve");
 resolver.foreignResolved=true;reject(good,"foreign physical result refuses exact owned alias check");resolver.foreignResolved=false;
 resolver.deny=true;reject(good,"typed schema refusal cannot publish caches");resolver.deny=false;
 auto refuseGeometry=[](const RenderNode& n,u32 slot,const ActorFields&,std::string&){return !(n.kind==RenderKind::Textures&&slot==1);};check(!planRenderMutable(good,refuseGeometry,factory,owned,plan,e)&&plan.size()==6,"late installed geometry mismatch leaves prior complete plan intact");
 check(planRenderMutable(good,geometry,factory,owned,plan,e),"correct graph accepted after all negative staging controls");
 }
 materials[0].mFlags=MATFLAG_Opaque;materials[1].mFlags=MATFLAG_Opaque;
 std::vector<RenderObservation> simple{src[0]};check(captureRenderGraph(8,simple,{1},factory,out,e)&&out.links.size()==2&&!out.links[0].pvw&&out.nodes.size()==1,"non-PVW native material does not inspect stale PVW pointers");
 std::printf("%d checks, %d failures\n",checks,failed);return failed?1:0;
}
