#include "pc_midday_animation_context.h"
#include "pc_midday_reference_catalog.h"
#include "Animator.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <cstring>
using namespace pc_midday;
int checks=0,callbacks=0;
// No engine loop/assets: the real native context layout is exercised, while its
// animate callback must remain unused throughout capture and staged restore.
void AnimContext::animate(float){++callbacks;}
void check(bool b){++checks;if(!b){std::cerr<<"animation context check failed "<<checks<<"\n";std::exit(1);}}
u32 bits(float f){u32 n;std::memcpy(&n,&f,4);return n;}
int main(){
 std::string e;SceneReferenceCatalog catalog({}, {10,20,21});int contentToken=1;
 // Pure factory metadata fixture; this content pointer is never dereferenced.
 const LogicalRef data{0,20,0},unknown{0,99,0},wrongData{0,21,0};
 check(catalog.declare({data,RefKind::Animation,FieldCategory::Reference,"AnimData",CatalogOrigin::Content,&contentToken,0,true},e));
 check(catalog.declare({wrongData,RefKind::Animation,FieldCategory::Reference,"AnimContext",CatalogOrigin::Content,nullptr,0,false},e));
 auto role=[](SceneSubject,const FieldSchema&,const LogicalRef&,std::string& error){error.clear();return true;};
 auto schema=animationContextSchema();SceneReferenceResolver resolver(SceneSubject{0,10},catalog,schema,role);
 AnimationContentIndex contentIndex;check(contentIndex.declare(data,60,e));
 check(!contentIndex.declare(data,30,e));
 check(!contentIndex.declare({1,22,0},10,e));
 check(!contentIndex.declare({0,22,1},10,e));
 check(!contentIndex.declare({0,22,0},0,e));
 check(!contentIndex.declare({0,22,0},1000001,e));
 check(contentIndex.validate(data,60,e));check(!contentIndex.validate(data,-1,e));
 check(!contentIndex.validate(wrongData,0,e));
 auto content=[&](const LogicalRef& r,float frame,std::string& error){return contentIndex.validate(r,frame,error);};
 AnimationContextRecord r{data,31.125f,-0.0f},decoded;ActorBytes bytes;
 check(encodeAnimationContext(r,resolver,content,bytes,e));
 check(decodeAnimationContext(bytes,resolver,content,decoded,e)&&bits(decoded.frame)==bits(r.frame)&&bits(decoded.speed)==bits(r.speed));
 AnimContext source;source.mData=reinterpret_cast<AnimData*>(&contentToken);source.mCurrentFrame=r.frame;source.mAnimSpeed=r.speed;
 ActorBytes actual;check(captureAnimationContext(source,resolver,content,actual,e)&&actual==bytes&&!callbacks);
 RestoreGate gate{true,true,true,true,true,true,true};AnimContext fresh;
 check(stageAnimationContext(fresh,actual,resolver,content,gate,e)&&fresh.mData==source.mData&&bits(fresh.mCurrentFrame)==bits(r.frame)&&bits(fresh.mAnimSpeed)==bits(r.speed)&&!callbacks);
 gate.rngDrawsSuppressed=false;fresh.mCurrentFrame=4;fresh.mAnimSpeed=9;
 check(!stageAnimationContext(fresh,actual,resolver,content,gate,e)&&fresh.mCurrentFrame==4&&fresh.mAnimSpeed==9);
 gate.rngDrawsSuppressed=true;auto malformed=actual;malformed.pop_back();
 check(!stageAnimationContext(fresh,malformed,resolver,content,gate,e)&&fresh.mCurrentFrame==4&&fresh.mAnimSpeed==9);
 check(!encodeAnimationContext({data,std::numeric_limits<float>::infinity(),1},resolver,content,bytes,e));
 check(!encodeAnimationContext({data,1,std::numeric_limits<float>::quiet_NaN()},resolver,content,bytes,e));
 check(!encodeAnimationContext({data,61,1},resolver,content,bytes,e));
 check(!encodeAnimationContext({unknown,1,1},resolver,content,bytes,e));
 check(!encodeAnimationContext({wrongData,1,1},resolver,content,bytes,e));
 check(!encodeAnimationContext(r,resolver,{},bytes,e));
 check(encodeAnimationContext({{},0,30},resolver,content,bytes,e));
 check(stageAnimationContext(fresh,bytes,resolver,content,gate,e)&&!fresh.mData&&fresh.mCurrentFrame==0&&fresh.mAnimSpeed==30);
 check(!encodeAnimationContext({{},1,30},resolver,content,bytes,e));
 // Known content templates validate before allocation, but restore cannot write
 // any native field until the referenced content allocation is actually bound.
 SceneReferenceCatalog templates({}, {10,20});check(templates.declare({data,RefKind::Animation,FieldCategory::Reference,"AnimData",CatalogOrigin::Content,nullptr,0,false},e));
 SceneReferenceResolver unresolved(SceneSubject{0,10},templates,schema,role);
 check(encodeAnimationContext(r,unresolved,content,bytes,e));
 fresh.mCurrentFrame=8;check(!stageAnimationContext(fresh,bytes,unresolved,content,gate,e)&&fresh.mCurrentFrame==8&&!fresh.mData);
 AnimationContextRecord sentinel{{},7,11};check(!decodeAnimationContext(malformed,resolver,content,sentinel,e)&&sentinel.frame==7&&sentinel.speed==11);
 // A resource with a shared alias is still encoded once; every consumer resolves
 // the same fresh context address, preserving its mutable state and aliasing.
 const LogicalRef context{0,10,0};check(catalog.declare({context,RefKind::Animation,FieldCategory::Reference,"AnimContext",CatalogOrigin::Content,&fresh,0,true},e));
 auto consumerSchema=FieldSchema::ref("animation",RefKind::Animation,false,"AnimContext",ReferenceOwnership::Content);
 SceneReferenceResolver consumer(SceneSubject{0,20},catalog,{consumerSchema},role);void* a=nullptr;void* b=nullptr;
 check(consumer.resolve("animation",RefKind::Animation,context,a,e)&&consumer.resolve("animation",RefKind::Animation,context,b,e)&&a==&fresh&&a==b&&!callbacks);
 std::cout<<checks<<" native animation context controls PASS\n";
}
