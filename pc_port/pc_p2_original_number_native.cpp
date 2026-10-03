#include "pc_p2_original_number_native.h"
#include "pc_p2_original_number_animation.h"
#include "pc_p2_original_number_geometry.h"
#include "pc_p2_original_onyon_native.h"
#include "pc_p2_original_onyon_lineage.h"
#include "pc_randomizer.h"
#include "Pellet.h"
#include "PelletView.h"
#include "GoalItem.h"
#include "Shape.h"
#include "Graphics.h"
#include "Camera.h"
#include "gameflow.h"
#include "system.h"
#include "netplay/pc_netplay_sha256.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <vector>
// Only the admitted numeric factory can initialize this native pool member.
struct P2OriginalNumberAccess {
 static void initialize(Pellet* p, PelletView* view, PelletConfig* config) { p->initPellet(view, config); }
};
namespace {
using namespace p2originalnumber;using namespace p2originalresource;
bool fail(std::string& e,const char* text){e=text;return false;}
[[noreturn]] void fault(const std::string& e){std::fprintf(stderr,"P2_ORIGINAL_NUMBER_FAIL %s\n",e.c_str());std::abort();}
std::unique_ptr<Ledger> ledger;std::string selectedSession,selectedCampaign;
bool ready=false;Shape* shapes[3]={};Animation animation;PelletConfig* configs[3]={};PelletProp* props=nullptr;ObjCollInfo* nodes[2]={};
struct Binding {std::uint64_t handle=0;ChildIdentity identity;PcOriginalNumberAuthority* authority=nullptr;const Profile* profile=nullptr;CreatureProp* previousProps=nullptr;CollPart* parts[2]={};bool updates[2]={},sticks[2]={};};
std::map<const Pellet*,Binding> bindings;
bool current(std::string& e){
 if(!pc_randomizer_original_session()||pc_randomizer_session_fingerprint().size()!=64||pc_randomizer_original_campaign().size()!=64)return fail(e,"numeric family requires admitted original session");
 if(!ledger){selectedSession=pc_randomizer_session_fingerprint();selectedCampaign=pc_randomizer_original_campaign();ledger=std::make_unique<Ledger>(selectedCampaign);}
 return (selectedSession==pc_randomizer_session_fingerprint()&&selectedCampaign==pc_randomizer_original_campaign())||fail(e,"numeric family requires explicit fresh-session boundary");
}
bool sameFloat(float a,float b){std::uint32_t x,y;std::memcpy(&x,&a,4);std::memcpy(&y,&b,4);return x==y;}
bool sameVector(const P2EggVec3& a,const P2EggVec3& b){return sameFloat(a.x,b.x)&&sameFloat(a.y,b.y)&&sameFloat(a.z,b.z);}
bool pendingMatches(const ChildOutcome& c,const ContentsRecord& r){
 if(r.complete||!(r.source==c.identity.source)||r.type!=P2EggDropType::OnePellets||r.children.size()!=1)return false;
 const auto& actual=r.children.front();
 return actual.identity==c.identity&&actual.kind==c.kind&&sameVector(actual.position,c.position)&&sameVector(actual.velocity,c.velocity)&&
  actual.pelletColor==c.pelletColor&&actual.mititeCount==c.mititeCount&&sameFloat(actual.facing,c.facing)&&actual.attempted&&!actual.born&&!actual.consumed;
}
bool readResource(const char* name,const char* expected,std::size_t maximum,std::string& bytes,std::string& e){
 const std::string path=std::string("p2-original/numbers/")+name;
 auto* in=gsys->openFile(path.c_str(),true,true);if(!in)return fail(e,"numeric converted resource missing");
 const int length=in->getLength();if(length<=0||std::size_t(length)>maximum){in->close();return fail(e,"numeric converted resource byte bound invalid");}
 std::string candidate(std::size_t(length),'\0');in->read(&candidate[0],length);in->close();
 unsigned char digest[32];pc_netplay_sha::sha256(candidate.data(),candidate.size(),digest);std::string hex;const char* digits="0123456789abcdef";
 for(unsigned char x:digest){hex+=digits[x>>4];hex+=digits[x&15];}
 if(hex!=expected)return fail(e,"numeric converted resource hash changed");bytes=std::move(candidate);return true;
}
void literals(){
 if(props)return;const int before=gsys->setHeap(SYSHEAP_Sys);
 props=new PelletProp;props->mCreatureProps.mFriction(profile(Size::One)->friction);
 for(unsigned color=0;color<3;++color){auto* c=new PelletConfig;configs[color]=c;
  c->mModelId.setID(0x50330100u|color);c->mPelletId=c->mModelId;c->mPelletType.mValue=int(color);c->mPelletColor.mValue=int(color);
  c->mCarryMinPikis.mValue=1;c->mCarryMaxPikis.mValue=2;c->mMatchingOnyonSeeds.mValue=2;c->mNonMatchingOnyonSeeds.mValue=1;
  c->mCarryInfoHeight.mValue=7.6f;c->mPelletScale.mValue=1;c->mUseDynamicMotion.mValue=0;c->mAnimSoundID.mValue=-1;
 }
 std::array<p2originalnumber::Sphere,2> source;collider(Size::One,source);
 for(unsigned i=0;i<2;++i){nodes[i]=new ObjCollInfo;nodes[i]->mId.setID(i?'cent':'nrot');nodes[i]->mCode.setID('____');nodes[i]->mJointIndex=-1;nodes[i]->mRadius=source[i].radius;nodes[i]->mCentrePosition.set(source[i].center[0],source[i].center[1],source[i].center[2]);}
 nodes[0]->add(nodes[1]);gsys->setHeap(before);
}
struct NumberView final:PelletView {
 Shape* shape=nullptr;CarryPlayer player;
 void viewKill()override;
 float viewGetBottomRadius()override{return 10;}
 float viewGetHeight()override{return 7.6f;}
 void viewStartTrembleMotion(float speed)override{if(speed>0){if(!player.start(gsys->getFrameTime()))fault("numeric animation start delta invalid");}else player.stop();}
 void viewSetMotionSpeed(float speed)override{if(speed==0)player.stop();else viewStartTrembleMotion(speed);}
 void viewFinishMotion()override{player.finish();}
 void viewDoAnimation()override{if(!mPellet||!player.advance(gsys->getFrameTime(),mPellet->getPickOffset()!=0))fault("numeric animation body/delta invalid");}
 Matrix4f local()const{std::array<float,12> sample;if(!animation.sample(float(player.sampleFrame()),sample))fault("numeric animation source pose invalid");Matrix4f m;m.makeIdentity();for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)m.mMtx[r][c]=sample[r*4+c];return m;}
 void viewDraw(Graphics& gfx,immut Matrix4f& world)override{Matrix4f transform,pose=local();world.multiplyTo(pose,transform);shape->updateAnim(gfx,transform,nullptr,mPellet);shape->drawshape(gfx,*gfx.mCamera,nullptr);}
};
std::vector<std::unique_ptr<NumberView>> views;
void NumberView::viewKill(){
 auto* p=mPellet;auto b=bindings.find(p);if(b==bindings.end())fault("numeric view retirement lost binding");std::string e;
 if(!ledger->retire(p,b->second.handle,e))fault(e);
 for(unsigned i=0;i<2;++i){b->second.parts[i]->mIsUpdateActive=b->second.updates[i];b->second.parts[i]->mIsStickEnabled=b->second.sticks[i];}
 p->mProps=b->second.previousProps;bindings.erase(b);mPellet=nullptr;shape=nullptr;player=CarryPlayer{};
}
}
bool pc_p2_original_number_authority_idle(const PcOriginalNumberAuthority* owner)noexcept{
 for(const auto& entry:bindings)if(entry.second.authority==owner)return false;
 return true;
}
PcOriginalNumberAuthority::~PcOriginalNumberAuthority(){
 if(!pc_p2_original_number_authority_idle(this))fault("numeric authority destroyed with live borrowed bodies");
}
bool pc_p2_original_number_resources(const p2originalresource::ContentsRequirements& requirements,std::string& e){
 if(!requirements.pelletOne&&!requirements.pelletFive){e.clear();return true;}
 if(requirements.pelletFive)return fail(e,"Five literal source LOD/rigid physics adapter is not implemented");
 // Retail One applies its floor-normal force after tracing; P1's earlier
 // horizontal damping/gravity ordering cannot qualify the physical path.
 // Keep this closure held before ANY producer RNG until that adapter lands.
 if(requirements.pelletOne)return fail(e,"One source floor-force/gravity ordering adapter is not implemented");
 if(!current(e)||!gsys||!pelletMgr||pelletMgr->getMax()<=0||pelletMgr->getMax()>4096)return fail(e,"numeric physical manager unavailable");
 if(!ledger->remainingCapacity())return fail(e,"numeric retained receipt capacity exhausted before producer RNG");
 if(ready){e.clear();return true;}
 const char* names[]={"number1_blue.mod","number1_red.mod","number1_yellow.mod"};
 const char* hashes[]={"0639a26006c745a22d01b47327f7b6ee9d31e157b62b829bb96a2b7b2edc1f68","e6639971063971c6d90881150720815f66ee57cb45d14e3f7dd874374cc58991","281c8c80488af88e1d850d794b79b4226fc4f9efc861037d87a501451c21ed60"};
 std::string bytes;
 // Verify the complete reachable color/clip closure before loading any model.
 for(unsigned i=0;i<3;++i)if(!readResource(names[i],hashes[i],262144,bytes,e))return false;
 if(!readResource("number1_carry.txt","0add432e7c94aadf7c140ecde2511554927d7292aff09e6f987b53390a31e3b6",32768,bytes,e)||!animation.read(bytes,e))return false;
 literals();
 for(unsigned i=0;i<3;++i){const std::string path=std::string("p2-original/numbers/")+names[i];shapes[i]=gameflow.loadShape(path.c_str(),true);if(!shapes[i]||shapes[i]->mJointCount!=1)return fail(e,"numeric original model topology unavailable");for(int j=0;j<shapes[i]->mTexAttrCount;++j)if(shapes[i]->mTexAttrList[j].mTexture)shapes[i]->mTexAttrList[j].mTexture->attach();}
 views.clear();views.reserve(unsigned(pelletMgr->getMax()));for(int i=0;i<pelletMgr->getMax();++i)views.emplace_back(std::make_unique<NumberView>());
 ready=true;e.clear();return true;
}
PcOriginalNumberBirth pc_p2_original_number_birth(const p2originalresource::ChildOutcome& child,PcOriginalNumberAuthority& authority,Pellet*& out,std::string& e){
 if(!current(e)||!ready||child.kind!=ChildKind::PelletOne||child.pelletColor<0||child.pelletColor>2||!sameFloat(child.facing,0))return PcOriginalNumberBirth::Fault;
 unsigned root=0;ContentsRecord pending;BirthPlan plan;
 if(!authority.pending(child,root,pending,e)||!pendingMatches(child,pending)||!ledger->preflightBirth(child,root,plan,e)){if(e.empty())e="numeric birth lacks exact active producer callback";return PcOriginalNumberBirth::Fault;}
 NumberView* view=nullptr;for(auto& candidate:views)if(!candidate->mPellet){view=candidate.get();break;}
 if(!view){e="numeric view capacity differs from admitted native pool";return PcOriginalNumberBirth::Fault;}
 auto* p=static_cast<Pellet*>(pelletMgr->birth());if(!p){e.clear();return PcOriginalNumberBirth::PoolExhausted;}
 if(p->mGenerator||p->mPelletView||bindings.count(p)){pelletMgr->kill(p);e="numeric pool slot retains foreign ownership";return PcOriginalNumberBirth::Fault;}
 std::uint64_t handle=0;if(!ledger->bindBirth(plan,p,handle,e)){pelletMgr->kill(p);return PcOriginalNumberBirth::Fault;}
 view->mPellet=p;view->shape=shapes[child.pelletColor];view->player=CarryPlayer{};
 Binding binding;binding.handle=handle;binding.identity=child.identity;binding.authority=&authority;binding.profile=profile(Size::One);binding.previousProps=p->mProps;
 p->mProps=props;P2OriginalNumberAccess::initialize(p,view,configs[child.pelletColor]);p->mPelletCollInfo->initInfoTree(nodes[0]);
 for(unsigned i=0;i<2;++i){auto* part=p->mPelletCollInfo->getSphere(i?'cent':'nrot');if(!part)fault("numeric pool collider allocation failed");binding.parts[i]=part;binding.updates[i]=part->mIsUpdateActive;binding.sticks[i]=part->mIsStickEnabled;part->mIsUpdateActive=false;part->mIsStickEnabled=false;}
 p->mCollInfo=p->mPelletCollInfo;bindings.emplace(p,binding);
 p->mUseSpawnPosition=false;p->init(Vector3f(child.position.x,child.position.y,child.position.z));p->disablePickOffset();p->releaseAllParticles();p->mFaceDirection=0;p->mSRT.r.set(0,0,0);p->mRotationQuat.fromEuler(Vector3f(0,0,0));
 p->mVelocity.set(child.velocity.x,child.velocity.y,child.velocity.z);p->mTargetVelocity=p->mVelocity;p->mCollisionRadius=3.8f;p->startAI(0);p->useSimpleDynamics();pc_p2_original_number_collision(p);
 out=p;e.clear();return PcOriginalNumberBirth::Born;
}
bool pc_p2_original_number_tag(const Pellet* p)noexcept{
 if(!p)return false;
 if(bindings.find(p)!=bindings.end())return true;
 // A late callback on a retired typed slot cannot become an ordinary yield.
 for(const auto* config:configs)if(config&&p->mConfig==config)return true;
 return false;
}
const p2originalnumber::Profile* pc_p2_original_number_profile(const Pellet* p)noexcept{auto b=bindings.find(p);return b==bindings.end()?nullptr:b->second.profile;}
p2originalnumber::QueryResult pc_p2_original_number_query(const Pellet* p,p2originalnumber::Receipt& out,std::string& e){
 auto b=bindings.find(p);if(b==bindings.end()){
  if(pc_p2_original_number_tag(p)){fail(e,"numeric source config lost current RAM receipt");return QueryResult::Unavailable;}
  e.clear();return QueryResult::Missing;
 }
 if(!current(e))return QueryResult::Unavailable;
 ContentsRecord record;unsigned root=0;
 if(!b->second.authority->completed(b->second.identity,root,record,e))return QueryResult::Unavailable;
 return ledger->query(p,b->second.handle,record,root,out,e);
}
bool pc_p2_original_number_consume(Pellet* p,GoalItem* onyon,unsigned& out,std::string& e){
 Receipt receipt;if(pc_p2_original_number_query(p,receipt,e)!=QueryResult::Present)return false;
 p2originalonyon::Root root;if(!pc_p2_original_onyon_root(onyon,root,e)||!pc_p2_original_onyon_access(onyon))return fail(e,"numeric receiver is not admitted and booted source Onyon");
 unsigned yield=0;if(!p2originalnumber::yield(Size::One,Color(receipt.birthPayload.pelletColor),Color(root.species),yield))return fail(e,"numeric receiver color invalid");
 if(receipt.consumed){out=0;e.clear();return true;}
 auto& b=bindings.at(p);ContentsRecord journal;unsigned type=0;ConsumePlan plan;
 if(!b.authority->completed(receipt.birthPayload.identity,type,journal,e)||!ledger->prepareConsume(p,b.handle,journal,type,plan,e))return false;
 if(!b.authority->consumeAccepted(receipt.birthPayload.identity,e))return false;
 if(!b.authority->completed(receipt.birthPayload.identity,type,journal,e)||!ledger->commitConsumed(plan,journal,type,e))fault("accepted numeric journal changed during consume: "+e);
 out=yield;e.clear();return true;
}
void pc_p2_original_number_collision(Pellet* p){
 auto b=bindings.find(p);if(b==bindings.end())return;auto* view=static_cast<NumberView*>(p->mPelletView);Matrix4f world,transform,pose=view->local();world.makeSRT(p->mSRT.s,p->mSRT.r,p->mSRT.t);world.multiplyTo(pose,transform);
 for(unsigned i=0;i<2;++i){auto* part=b->second.parts[i];Vector3f center(nodes[i]->mCentrePosition);center.multMatrix(transform);part->mCentre=center;part->mRadius=nodes[i]->mRadius;part->mJointMatrix=Matrix4f::ident;}
}
bool pc_p2_original_number_unload(std::string& e){
 if(ledger&&!ledger->unload(e))return false;
 ready=false;for(auto& shape:shapes)shape=nullptr;views.clear();e.clear();return true;
}
bool pc_p2_original_number_preflight_teardown(std::string& e){
 if(!bindings.empty())return fail(e,"typed numeric bodies require retained producer/physical graph");
 if(ledger&&!ledger->unload(e))return false;
 e.clear();return true;
}
bool pc_p2_original_number_new_session(std::string& e){
 if(!pc_p2_original_number_unload(e)||!pc_randomizer_original_session())return false;
 ledger.reset();selectedSession.clear();selectedCampaign.clear();return current(e);
}
