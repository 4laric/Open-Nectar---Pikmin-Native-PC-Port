// Header-layout/visitor control: no game loop, GL, file IO or native factory.
#include "pc_midday_render_graph.h"
#include "Material.h"
#include <cstdio>
void Material::read(RandomAccessStream&){}
void Material::attach(){}
using namespace pc_midday;
namespace {int checks=0,failed=0;void check(bool b,const char*s){++checks;if(!b){++failed;std::printf("FAIL %s\n",s);}}
struct Resolver:LogicalResolver {
 std::map<const void*,LogicalRef> pointers;bool deny=false,wrongAlias=false,swapTexture=false,badTevSlot=false;int identifies=0;
 bool identify(const char* k,RefKind,const void* p,LogicalRef& out,std::string& e)override{++identifies;if(!p){out={};return true;}auto it=pointers.find(p);if(it==pointers.end()){e="unregistered test pointer";return false;}out=it->second;if(wrongAlias&&std::string(k)=="tev")out.resource=99;if(badTevSlot&&std::string(k)=="tev")out.slot=1;if(swapTexture&&std::string(k).find("texture.")==0)out.slot=1-out.slot;return true;}
 bool validate(const char*,RefKind,const LogicalRef&,std::string&)const override{return !deny;}
 bool validateTyped(const FieldSchema& s,const LogicalRef&r,std::string&)const override{if(deny||s.targetType.empty())return false;if(!r.owner&&!r.resource&&!r.slot)return s.nullable;for(const auto& p:pointers)if(p.second.resource==r.resource&&p.second.slot==r.slot)return true;return false;}
 bool resolve(const char*,RefKind,const LogicalRef&,void*&,std::string&)override{return false;}
 bool identifyHandle(const char*,RefKind,u32,LogicalRef&,std::string&)override{return false;}
 bool resolveHandle(const char*,RefKind,const LogicalRef&,u32&,std::string&)override{return false;}
};}
int main(){Material materials[2];PVWTevInfo tev[3];PVWTextureData textures[2];
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
 materials[1].mTevInfo=&tev[2];resolver.pointers[&tev[0]]={0,2,0};resolver.pointers[&tev[1]]={0,2,1};resolver.pointers[&tev[2]]={0,2,2};
 std::vector<RenderObservation> full{src[0],{2,202,RenderKind::Tev,3,reinterpret_cast<uintptr_t>(tev),sizeof(PVWTevInfo),true},src[3]};
 check(captureRenderGraph(9,full,{1,2,4},factory,out,e)&&out.nodes[1].payloads.size()==3&&out.nodes[1].contentRoot,"real header complete array capture retains unused initialized TEV element");
 check(out.links[0].tev==2&&out.links[0].tevSlot==0&&out.links[1].tev==2&&out.links[1].tevSlot==2,"real native aliases select exact elements within one original allocation");
 check(decode_actor_fields(out.nodes[1].payloads[1],fields,e)&&fields.at("mTevColRegs.2.mAnimatedColor.a").bits==4,"unused initialized element has captured named payload");
 resolver.badTevSlot=true;check(!captureRenderGraph(9,full,{1,2,4},factory,out,e)&&out.links[1].tevSlot==2,"wrong in-range logical TEV element refuses before graph publication");resolver.badTevSlot=false;
 check(captureRenderGraph(9,full,{1,2,4},factory,out,e),"exact complete array still captures after alias refusal");
 materials[0].mFlags=MATFLAG_Opaque;materials[1].mFlags=MATFLAG_Opaque;
 std::vector<RenderObservation> simple{src[0]};check(captureRenderGraph(8,simple,{1},factory,out,e)&&out.links.size()==2&&!out.links[0].pvw&&out.nodes.size()==1,"non-PVW native material does not inspect stale PVW pointers");
 std::printf("%d checks, %d failures\n",checks,failed);return failed?1:0;
}
