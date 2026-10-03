#include "pc_p2_original_piki_plate.h"
#include "pc_p2_original_progress.h"
#include <cmath>
#include <iostream>
#include <map>
#include <limits>
using namespace p2original;
using namespace p2original::piki;
namespace {
unsigned checks=0;
void check(bool condition,const char* message){++checks;if(!condition){std::cerr<<message<<'\n';std::exit(1);}}
Navi* n0=reinterpret_cast<Navi*>(0x1000);Navi* n1=reinterpret_cast<Navi*>(0x2000);
class Scene final:public captain::LoadedScene {
public:
 std::string campaign=std::string(64,'a'),fingerprint="selected-seed",catalog="scene-catalog";std::uint64_t epoch=7;
 const std::string& selectedCampaign()const override{return campaign;}
 const std::string& selectedFingerprint()const override{return fingerprint;}
 const std::string& sourceCatalog()const override{return catalog;}
 MoviePlayer* moviePlayer()const override{return nullptr;}
 std::uint64_t incarnation()const override{return epoch;}
 Navi* captainAt(unsigned i)const override{return i==0?n0:i==1?n1:nullptr;}
} scene;
std::map<Piki*,NativeBodyFacts> bodies;
std::map<std::pair<Navi*,Piki*>,int> listeners;
bool callbackFailure=false,inactive=false;
std::vector<SlotChange> lastChanges;
class Source final:public PlateSource {
public:
 PlateParameters parameters{17.5f,130.0f,6.0f};bool missingParameters=false;
 mutable unsigned poseReads=0;
 PlatePose pose;bool missingPose=false;unsigned formedEvents=0;Plate* reenter=nullptr;
 Plate* parameterReenter=nullptr;mutable bool validationWindow=false;
 unsigned phaseChangeAt=0;mutable unsigned parameterReads=0;
 Source(){pose.position.set(100,20,200);pose.velocity.set(0,0,0);pose.scale=1;}
 const captain::LoadedScene& scene()const override{return ::scene;}
 bool readParameters(const Navi*,PlateParameters& out,std::string& e)const override{
  ++parameterReads;if(phaseChangeAt==parameterReads)inactive=true;
  if(missingParameters){e="parameters missing";return false;}
  if(parameterReenter&&validationWindow){std::string nested;parameterReenter->shrink(n0,nested);}
  out=parameters;return true;
 }
 bool readPose(const Navi*,PlatePose& out,std::string& e)const override{
  ++poseReads;if(missingPose){e="pose missing";return false;}
  if(reenter){std::string nested;reenter->shrink(n0,nested);}
  validationWindow=true;out=pose;return true;
 }
 bool setFormed(Handle,Navi*,std::string&)override{validationWindow=true;++formedEvents;return true;}
};
Handle birth(unsigned i,unsigned kind=1,unsigned happa=0){
 Handle h{reinterpret_cast<Piki*>(0x3000+i*0x100),100+i};NativeBodyFacts b;
 b.handle=h;b.species=kind;b.happa=happa;b.position.set(static_cast<float>(i),0,0);
 bodies[h.body]=b;return h;
}
void reserve(Plate& plate,Handle h,Navi* n,int expected,std::string& e){int slot=-9;
 check(plate.allocateSlot(h,n,slot,e)&&slot==expected,"allocation index");listeners[{n,h.body}]=slot;
}
}
namespace p2original {namespace piki {
bool nativeSceneBinding(SceneBinding& out,bool cleanup,std::string& e){
 if(inactive&&!cleanup){e="inactive";return false;}SceneBinding b;b.scene=&::scene;b.incarnation=::scene.epoch;
 b.captains[0]=n0;b.captains[1]=n1;b.campaign=::scene.campaign;b.fingerprint=::scene.fingerprint;b.catalog=::scene.catalog;
 out=b;return true;
}
bool nativeSceneCurrent(const SceneBinding& b,bool cleanup,std::string& e){
 SceneBinding now;return nativeSceneBinding(now,cleanup,e)&&b.scene==now.scene&&b.incarnation==now.incarnation
 &&b.captains[0]==n0&&b.captains[1]==n1;
}
bool nativeCaptainFacts(const SceneBinding& b,const Navi* n,bool cleanup,std::string& e){return nativeSceneCurrent(b,cleanup,e)&&(n==n0||n==n1);}
bool nativeBodyFacts(Handle h,NativeBodyFacts& out,std::string&){auto i=bodies.find(h.body);
 if(i==bodies.end()||i->second.handle.lifetime!=h.lifetime)return false;out=i->second;return true;
}
bool slotsChanged(const std::vector<SlotChange>& changes,std::string& e){
 if(callbackFailure){e="injected notification refusal";return false;}
 // Atomic listener double models only the published notification boundary.
 for(const auto& c:changes)if(listeners[{c.captain,c.handle.body}]!=c.oldSlot)return false;
 for(const auto& c:changes){if(c.newSlot<0)listeners.erase({c.captain,c.handle.body});else listeners[{c.captain,c.handle.body}]=c.newSlot;}
 lastChanges=changes;return true;
}
}}
bool pc_p2_original_piki_body_current(const Piki* p,std::uint64_t lifetime)noexcept{
 auto i=bodies.find(const_cast<Piki*>(p));return i!=bodies.end()&&i->second.handle.lifetime==lifetime;
}
int main(){
 std::string e;check(originalProgress().initialize(scene.campaign,e),"progress init");
 PlateParameters p;p.startingOffset=99;
 check(!parsePlateParameters("",p,e)&&p.startingOffset==99,"missing bytes leave output");
 check(!parsePlateParameters("{p000} 4 17 {p001} 4 130 {p002} 4 6 {p000} 4 10",p,e),"duplicate parameter refusal");
 Source source;Plate plate(source);auto a=birth(1,1,0),b=birth(2,4,2),c=birth(3,1,1);
 reserve(plate,a,n0,0,e);reserve(plate,b,n0,1,e);reserve(plate,c,n0,2,e);
 int sentinel=55;check(!plate.allocateSlot(a,n0,sentinel,e)&&sentinel==55,"same-plate duplicate refuses unchanged");
 reserve(plate,a,n1,0,e);check(plate.retainedSlots()==4&&plate.retainedListeners()==4,"transfer retains both plate owners");
 std::array<unsigned,3> growth{};
 check(plate.growthCounts(n0,growth,e)&&growth==std::array<unsigned,3>{1,1,1},"initial actual growth counts");
 bodies[a.body].happa=1;
 check(plate.changeFlower(a,n0,e)&&plate.changeFlower(a,n1,e),"genuine growth event each retained source slot");
 check(plate.growthCounts(n0,growth,e)&&growth==std::array<unsigned,3>{0,2,1},"source growth count migration");
 check(!plate.changeFlower(a,n0,e),"duplicate growth event refuses");
 check(plate.canReleaseSlot(a,n0,0,e)&&plate.retainedSlots()==4,"readonly release preflight preserves owners");
 check(plate.refresh(n0,3,0,e)&&plate.setPos(n0,e),"real geometry refresh then evaluated SetPos");Vector3f first;
 check(plate.slotPosition(a,n0,0,first,e),"first geometry available");
 // Three stationary actors: retail first row width1, x=12 and base z=17.5.
 check(std::abs(first.x-112)<0.001f&&std::abs(first.y-20)<0.001f&&std::abs(first.z-217.5f)<0.001f,"retail three-body first position");
 callbackFailure=true;check(!plate.releaseSlot(a,n0,0,e)&&plate.retainedSlots()==4,"failed inform preserves references");
 Vector3f unchanged;check(plate.slotPosition(a,n0,0,unchanged,e)&&unchanged.x==first.x,"failed release preserves positions");
 callbackFailure=false;check(plate.sortSlot(b,n0,1,2,e),"sort selected White flower first");
 check(listeners[{n0,b.body}]==0&&listeners[{n0,a.body}]==1&&listeners[{n0,c.body}]==2,"retail kind/growth priority informs");
 Vector3f sorted;check(plate.slotPosition(b,n0,0,sorted,e)&&sorted.x==first.x&&sorted.z==first.z,"sort preserves ordinal geometry");
 check(plate.releaseSlot(a,n0,1,e)&&plate.retainedSlots()==3,"release and compact");
 check(listeners[{n0,c.body}]==1,"compacted listener informed");
 check(!plate.releaseSlot(a,n0,1,e)&&plate.retainedSlots()==3,"double release refuses");
 check(!plate.reset(e),"nonempty reset refuses");
 source.missingPose=true;check(!plate.setPos(n0,e),"missing actual pose refuses");source.missingPose=false;
 source.reenter=&plate;check(!plate.setPos(n0,e)&&plate.retainedSlots()==3,"reentrant pose mutation refuses without owner loss");source.reenter=nullptr;
 PlateState before,after;check(plate.state(n0,before,e),"prevalidation reentry snapshot");
 source.validationWindow=false;source.parameterReenter=&plate;source.pose.position.x+=500;
 check(!plate.setPos(n0,e),"parameter validation reentry after successful pose refuses");
 source.parameterReenter=nullptr;source.pose.position.x-=500;
 check(plate.state(n0,after,e)&&after.baseOffset.x==before.baseOffset.x&&after.baseOffset.z==before.baseOffset.z
       &&after.baseRadius==before.baseRadius&&after.moveRadius==before.moveRadius&&after.angle==before.angle,"failed post-pose validation preserves live geometry");
 source.validationWindow=false;source.parameterReenter=&plate;
 check(!plate.formed(b,n0,e),"formed final-validation reentry cannot report success");source.parameterReenter=nullptr;
 source.pose.scale=std::numeric_limits<float>::quiet_NaN();check(!plate.setPos(n0,e),"nonfinite pose refuses");source.pose.scale=1;
 ++source.parameters.startingOffset;check(!plate.setPos(n0,e),"selected parameters changed refuses");--source.parameters.startingOffset;
 source.missingParameters=true;check(!plate.setPos(n0,e),"missing selected parameters refuses");source.missingParameters=false;
 ++scene.epoch;check(!plate.releaseSlot(b,n0,0,e)&&plate.retainedSlots()==3,"stale scene refuses cleanup");--scene.epoch;
 inactive=true;check(!plate.setPos(n0,e),"inactive mutation refuses");
 check(plate.releaseSlot(b,n0,0,e)&&plate.releaseSlot(c,n0,0,e)&&plate.releaseSlot(a,n1,0,e),"inactive retained cleanup");
 check(plate.retainedSlots()==0&&plate.retainedListeners()==0&&plate.reset(e),"zero genuine references before reset");inactive=false;
 Source splitSource;Plate split(splitSource);auto splitBody=birth(250);reserve(split,splitBody,n1,0,e);
 PlateState initial;check(split.state(n1,initial,e)&&!initial.positionKnown&&!initial.maxRadiusKnown
 &&!initial.maxPositionKnown&&!initial.scaleKnown,"ctor preserves unknown max/pose/scale facts");
 Vector3f unpublished(77,78,79);check(!split.slotPosition(splitBody,n1,0,unpublished,e)&&unpublished.x==77,"initial unknown slot pose never published");
 splitSource.pose.angle=.4f;check(split.setPos(n1,e)&&split.state(n1,before,e)&&before.positionKnown&&before.scaleKnown
 &&!before.maxRadiusKnown&&!before.maxPositionKnown,"literal SetPos before Refresh retains unknown max geometry");
 const unsigned readBefore=splitSource.poseReads;splitSource.missingPose=true;
 check(split.refresh(n1,1,.5f,e)&&splitSource.poseReads==readBefore,"Refresh count strength never evaluates pose");splitSource.missingPose=false;
 check(split.state(n1,after,e)&&after.maxRadiusKnown&&!after.maxPositionKnown&&after.angle==before.angle
 &&after.baseOffset.x==before.baseOffset.x&&after.scale==before.scale,"Refresh leaves previous pose angle and scale untouched");
 splitSource.pose.scale=3;splitSource.pose.angle=1;
 check(split.setPos(n1,e)&&split.state(n1,after,e)&&after.maxPositionKnown&&after.scale==3
 &&std::abs(after.baseOffset.x-(100+(after.baseRadius+52.5f)*std::sin(1.f)))<.001f,"SetPos consumes scale evaluated AFTER Captain timer step");
 const float retainedAngle=after.angle;splitSource.pose.gray=true;splitSource.pose.angle=2;splitSource.pose.scale=1;
 check(split.setPosGray(n1,e)&&split.state(n1,after,e)&&after.angle==retainedAngle
 &&std::abs(after.baseOffset.x-(100+(after.baseRadius+17.5f)*std::sin(2.f)))<.001f,"SetPosGray uses new direction but retains source angle");
 check(!split.setPos(n1,e),"explicit SetPos variant refuses wrong evaluated source branch");splitSource.pose.gray=false;
 splitSource.pose.velocity.set(6,0,0);check(split.setPos(n1,e),"actual new velocity retained");
 splitSource.pose.velocity.set(0,0,0);check(split.setPos(n1,e)&&split.state(n1,after,e)
 &&std::abs(after.baseOffset.x-(100+after.baseRadius*std::sin(2.f)))<.001f,"SetPos offset uses PRIOR velocity not new velocity");
 check(!split.refresh(n1,0,0,e)&&split.retainedSlots()==1,"Refresh mismatched census cannot drop retained listener");
 check(!split.refresh(n1,1,std::numeric_limits<float>::quiet_NaN(),e),"nonfinite strength refuses");
 splitSource.validationWindow=true;splitSource.parameterReenter=&split;
 check(!split.refresh(n1,1,1,e),"Refresh parameter callback reentry refuses");splitSource.parameterReenter=nullptr;
 check(split.releaseSlot(splitBody,n1,0,e)&&split.reset(e),"split owner explicit retained cleanup");
 Plate defaultSort(source);auto leaf=birth(200,1,0),bud=birth(201,1,1),flower=birth(202,1,2);
 reserve(defaultSort,leaf,n0,0,e);reserve(defaultSort,bud,n0,1,e);reserve(defaultSort,flower,n0,2,e);
 check(defaultSort.sortSlot(bud,n0,1,-1,e),"retail default signed growth sort");
 check(listeners[{n0,bud.body}]==0&&listeners[{n0,leaf.body}]==1&&listeners[{n0,flower.body}]==2,"minus-one prefers selected growth then preserves other growth order");
 check(!defaultSort.sortSlot(bud,n0,0,-2,e),"invalid signed growth refuses");
 source.parameterReads=0;source.phaseChangeAt=2;
 check(!defaultSort.sortSlot(leaf,n0,1,0,e)&&listeners[{n0,bud.body}]==0,"sort callback phase change refuses without listener mutation");
 source.phaseChangeAt=0;inactive=false;
 Plate lifecycle(source);auto earlier=birth(203),later=birth(204);
 reserve(lifecycle,earlier,n1,0,e);check(lifecycle.refresh(n1,1,0,e)&&lifecycle.setPos(n1,e),"initial one-body refresh");reserve(lifecycle,later,n1,1,e);
 check(lifecycle.state(n1,before,e)&&before.count==2&&before.activeCount==1,"allocate grows slots before next source refresh");
 check(lifecycle.releaseSlot(earlier,n1,0,e)&&lifecycle.state(n1,after,e)&&after.count==1&&after.activeCount==0,"release decrements actual refreshed active count");
 Vector3f retainedPosition;check(lifecycle.slotPosition(later,n1,0,retainedPosition,e),"retail validSlot uses physical count between refreshes");
 check(lifecycle.refresh(n1,1,0,e)&&lifecycle.setPos(n1,e)&&lifecycle.state(n1,after,e)&&after.activeCount==1,"next source refresh restores active formation count");
 Plate full(source);for(unsigned i=0;i<100;++i)reserve(full,birth(10+i,i%7,i%3),n0,static_cast<int>(i),e);
 sentinel=88;check(!full.allocateSlot(birth(111),n0,sentinel,e)&&sentinel==88&&full.retainedSlots()==100,"capacity100 unchanged");
 auto stale=bodies.find(reinterpret_cast<Piki*>(0x3a00));++stale->second.handle.lifetime;check(!full.refresh(n0,100,0,e)&&full.retainedSlots()==100,"retired body refuses without dropping slot");
 std::cout<<checks<<" source CPlate component controls passed (synthetic authority/listener only; no gameplay)\n";
}
