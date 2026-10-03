#include "pc_p2_original_piki_plate.h"
#include "pc_p2_original_progress.h"
#include <algorithm>
#include <cmath>
#include <sstream>
namespace p2original { namespace piki {
namespace {
constexpr float pi=3.14159265358979323846f;
bool fail(std::string& e,const char* t){e=t;return false;}
bool finite(const Vector3f& v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool same(Handle a,Handle b){return a.body==b.body&&a.lifetime==b.lifetime;}
bool validParameters(const PlateParameters& p){return std::isfinite(p.startingOffset)&&std::isfinite(p.lengthLimit)
 &&std::isfinite(p.maxPositionSize)&&p.startingOffset>=0&&p.startingOffset<=100
 &&p.lengthLimit>=10&&p.lengthLimit<=1000&&p.maxPositionSize>=1&&p.maxPositionSize<=50;}
bool parameter(const std::string& bytes,const char* name,float& out){
 const std::string token=std::string("{")+name+"}";auto pos=bytes.find(token);
 if(pos==std::string::npos||bytes.find(token,pos+token.size())!=std::string::npos)return false;
 std::istringstream stream(bytes.substr(pos+token.size()));unsigned type=0;float value=0;
 if(!(stream>>type>>value)||type!=4||!std::isfinite(value))return false;
 out=value;return true;
}
}
bool parsePlateParameters(const std::string& bytes,PlateParameters& out,std::string& e){
 PlateParameters next;
 if(!parameter(bytes,"p000",next.startingOffset)||!parameter(bytes,"p001",next.lengthLimit)
 ||!parameter(bytes,"p002",next.maxPositionSize)||next.startingOffset<0||next.startingOffset>100
 ||next.lengthLimit<10||next.lengthLimit>1000||next.maxPositionSize<1||next.maxPositionSize>50)
  return fail(e,"invalid/missing selected source CPlate parameters");
 out=next;return true;
}
class Plate::Operation {
 Plate& owner;bool entered=false;
public:
 Operation(Plate& p,std::string& e):owner(p){
  if(p.mBusy){p.mReentered=true;e="reentrant CPlate mutation refused";return;}
  p.mBusy=true;p.mReentered=false;entered=true;
 }
 ~Operation(){if(entered)owner.mBusy=false;}
 bool ok()const{return entered;}
};
class Plate::ReadOperation {
 const Plate& owner;bool entered=false;
public:
 explicit ReadOperation(const Plate& p):owner(p){if(!p.mBusy){p.mBusy=true;p.mReentered=false;entered=true;}}
 ~ReadOperation(){if(entered)owner.mBusy=false;}
 bool complete()const{return !owner.mReentered;}
};
bool Plate::group(Navi* n,bool cleanup,Group*& out,std::string& e){
 SceneBinding binding;if(!nativeSceneBinding(binding,cleanup,e)||&mSource.scene()!=binding.scene
 ||!nativeCaptainFacts(binding,n,cleanup,e))return false;
 unsigned index=n==binding.captains[0]?0:1;auto& g=mGroups[index];
 if(g.captain){if(g.captain!=n||!validate(g,cleanup,e))return false;out=&g;return true;}
 // Prepare actual source instance parameters before publishing this owner.
 Group next;next.binding=binding;next.captain=n;
 if(!mSource.readParameters(n,next.parameters,e)||!validParameters(next.parameters)
 ||!nativeSceneCurrent(binding,cleanup,e)||mReentered)return fail(e,"actual source CPlate parameters unavailable");
 g=std::move(next);out=&g;return true;
}
bool Plate::validate(const Group& g,bool cleanup,std::string& e)const{
 PlateParameters selected;
 if(!nativeCaptainFacts(g.binding,g.captain,cleanup,e)||&mSource.scene()!=g.binding.scene
 ||!mSource.readParameters(g.captain,selected,e)||!validParameters(selected)
 ||selected.startingOffset!=g.parameters.startingOffset||selected.lengthLimit!=g.parameters.lengthLimit
 ||selected.maxPositionSize!=g.parameters.maxPositionSize||originalProgress().snapshot().campaign!=g.binding.campaign)
  return fail(e,"CPlate source binding/selected parameters changed");
 for(unsigned i=0;i<g.count;++i){NativeBodyFacts b;
  if(!nativeBodyFacts(g.slots[i].handle,b,e)||b.species!=g.slots[i].species)return false;}
 if(!nativeSceneCurrent(g.binding,cleanup,e))return false;
 for(unsigned i=0;i<g.count;++i)if(!pc_p2_original_piki_body_current(g.slots[i].handle.body,g.slots[i].handle.lifetime))
  return fail(e,"CPlate body retired during source callbacks");
 return true;
}
bool Plate::publish(Group& live,Group& next,bool cleanup,std::string& e){
 // Retail SlotChangeListener::inform updates the owning ActFormation slot.
 // Compute and validate the whole mapping before its atomic owner notification.
 std::vector<SlotChange> changes;changes.reserve(live.count);
 for(unsigned old=0;old<live.count;++old){int dest=-1;
  for(unsigned i=0;i<next.count;++i)if(same(live.slots[old].handle,next.slots[i].handle)){dest=static_cast<int>(i);break;}
  if(dest!=static_cast<int>(old))changes.push_back({live.slots[old].handle,live.captain,static_cast<int>(old),dest});
 }
 if(!validate(live,cleanup,e)||mReentered)return false;
 // Retail swap/release move references/listeners, not ordinal geometry.
 for(unsigned i=0;i<next.slots.size();++i){next.slots[i].relative=live.slots[i].relative;next.slots[i].position=live.slots[i].position;next.slots[i].geometryKnown=live.slots[i].geometryKnown;}
 // Only nonthrowing assignments follow a successful listener commit. Strings,
 // bindings and parameters are immutable through every mapping operation.
 if(!changes.empty()&&!slotsChanged(changes,e))return false;
 live.slots=next.slots;live.count=next.count;live.activeCount=next.activeCount;
 live.happaCounts=next.happaCounts;return true;
}
bool Plate::allocateSlot(Handle h,Navi* n,int& out,std::string& e){
 Operation operation(*this,e);if(!operation.ok())return false;
 Group* g=nullptr;NativeBodyFacts body;
 if(!group(n,false,g,e)||!nativeBodyFacts(h,body,e))return false;
 for(unsigned i=0;i<g->count;++i)if(g->slots[i].handle.body==h.body)
  return fail(e,"CPlate body already owns a slot in this captain plate");
 // Captain transfer retains the old plate until new ownership is recorded.
 // Only the SAME committed lifetime may temporarily own both plate slots.
 for(const auto& other:mGroups)for(unsigned i=0;i<other.count;++i)
  if(other.slots[i].handle.body==h.body&&!same(other.slots[i].handle,h))
   return fail(e,"CPlate holds a retired lifetime of this body");
 if(g->count>=g->slots.size())return fail(e,"source CPlate capacity exceeded");
 if(!validate(*g,false,e)||!pc_p2_original_piki_body_current(h.body,h.lifetime)||mReentered)return false;
 const unsigned index=g->count;Slot next=g->slots[index];next.handle=h;next.species=body.species;next.happa=body.happa;
 g->slots[index]=next;++g->count;++g->happaCounts[body.happa];
 out=static_cast<int>(index);return true;
}
bool Plate::releaseSlot(Handle h,Navi* n,int slot,std::string& e){
 Operation operation(*this,e);if(!operation.ok())return false;Group* g=nullptr;
 if(!group(n,true,g,e)||slot<0||static_cast<unsigned>(slot)>=g->count||!same(g->slots[slot].handle,h))
  return fail(e,"source CPlate release does not own exact slot/lifetime");
 Group next=*g;
 for(unsigned i=static_cast<unsigned>(slot);i+1<next.count;++i)next.slots[i]=next.slots[i+1];
 --next.count;next.slots[next.count].handle={};next.slots[next.count].species=0;next.slots[next.count].happa=0;
 // Retail decrements the refreshed active group even when getSlot grew the
 // physical count meanwhile. Saturate its unrefreshed-zero underflow safely.
 if(next.activeCount)--next.activeCount;
 next.happaCounts={};for(unsigned i=0;i<next.count;++i){NativeBodyFacts b;
  if(!nativeBodyFacts(next.slots[i].handle,b,e))return false;
  next.slots[i].happa=b.happa;++next.happaCounts[b.happa];}
 return publish(*g,next,true,e);
}
bool Plate::canReleaseSlot(Handle h,Navi* n,int slot,std::string& e)const{
 ReadOperation operation(*this);
 const Group* g=nullptr;for(const auto& candidate:mGroups)if(candidate.captain==n)g=&candidate;
 if(!g||!validate(*g,true,e)||slot<0||static_cast<unsigned>(slot)>=g->count
 ||!same(g->slots[slot].handle,h)||!operation.complete())return fail(e,"CPlate cleanup preflight lacks exact retained owner");
 return true;
}
bool Plate::slotPosition(Handle h,Navi* n,int slot,Vector3f& out,std::string& e)const{
 ReadOperation operation(*this);
 // Read-only nesting is allowed, but never invokes a producer to change pose.
 const Group* g=nullptr;for(const auto& candidate:mGroups)if(candidate.captain==n)g=&candidate;
 if(!g||!validate(*g,false,e)||slot<0||static_cast<unsigned>(slot)>=g->count
 ||!same(g->slots[slot].handle,h))return fail(e,"source CPlate slot position unavailable");
 if(!g->positionKnown||!g->slots[slot].geometryKnown)return fail(e,"source CPlate position not yet evaluated");
 Vector3f next=g->slots[slot].position+g->baseOffset;
 if(!finite(next)||!operation.complete())return fail(e,"source CPlate position nonfinite/reentrant");
 out=next;return true;
}
bool Plate::sortSlot(Handle h,Navi* n,int slot,int happa,std::string& e){
 Operation operation(*this,e);if(!operation.ok())return false;Group* g=nullptr;
 if(happa< -1||happa>2||!group(n,false,g,e)||slot<0||static_cast<unsigned>(slot)>=g->count||!same(g->slots[slot].handle,h))
  return fail(e,"source CPlate sort owner/growth unavailable");
 Group next=*g;unsigned selected=next.slots[slot].species;next.happaCounts={};
 for(unsigned i=0;i<next.count;++i){NativeBodyFacts b;if(!nativeBodyFacts(next.slots[i].handle,b,e))return false;
  next.slots[i].happa=b.happa;++next.happaCounts[b.happa];}
 const unsigned selectedHappa=next.slots[slot].happa;
 // Literal retail nested priority swaps, preserving stable equal priorities.
 for(unsigned i=0;i<next.count;++i)for(unsigned j=i+1;j<next.count;++j){
  auto& a=next.slots[i];auto& b=next.slots[j];
  unsigned ap=(a.species+7-selected)%7,bp=(b.species+7-selected)%7;
  unsigned ah=happa==-1?(a.happa!=selectedHappa):(a.happa+3-static_cast<unsigned>(happa))%3;
  unsigned bh=happa==-1?(b.happa!=selectedHappa):(b.happa+3-static_cast<unsigned>(happa))%3;
  if(bp<ap||(bp==ap&&bh<ah))std::swap(a,b);
 }
 return publish(*g,next,false,e);
}
bool Plate::formed(Handle h,Navi* n,std::string& e){
 Operation operation(*this,e);if(!operation.ok())return false;Group* g=nullptr;
 if(!group(n,false,g,e))return false;
 bool owned=false;for(unsigned i=0;i<g->count;++i)if(same(g->slots[i].handle,h))owned=true;
 if(!owned)return fail(e,"source formed event lacks current slot");
 if(!mSource.setFormed(h,n,e)||mReentered)return false;
 return validate(*g,false,e)&&!mReentered;
}
bool Plate::geometry(Group& g,float moveStrength,std::string& e){
 if(!std::isfinite(moveStrength))return fail(e,"source CPlate move strength unavailable");
 float strength=std::max(0.0f,std::min(1.0f,moveStrength));
 float size=g.parameters.maxPositionSize*(g.shrinkTimer?0.5f:1.0f);
 g.activeCount=g.count;g.maxRadiusKnown=true;g.maxRadius=(2.0f+0.1f)*size*std::sqrt(static_cast<float>(g.count)/pi);
 float small=(2.0f-0.1f)*size,large=g.maxRadius;
 float factor=std::max(small,large),lower=std::min(small,large);
 g.moveRadius=strength*-(factor-lower)+factor;
 g.baseRadius=g.moveRadius==0?10:(size*(4.0f*g.count*size))/(pi*g.moveRadius);
 if(g.baseRadius>g.parameters.lengthLimit){g.baseRadius=g.parameters.lengthLimit;
  g.moveRadius=(size*(4.0f*g.count*size))/(pi*g.baseRadius);}
 // setPosGray leaves mAngle intact. Rotation uses the previous selected angle
 // until setPos, just as retail refreshSlot then setPos ordering does.
 const float sine=std::sin(g.angle),cosine=std::cos(g.angle);
 float radius=-g.baseRadius;unsigned count=0;int direction=1;
 while(count<g.activeCount){
  float difference=std::abs(radius)<g.baseRadius?std::sqrt(std::max(0.0f,g.baseRadius*g.baseRadius-radius*radius)):0;
  int width=g.baseRadius>0?static_cast<int>(g.moveRadius*difference/g.baseRadius/(2*size)):0;
  width=std::max(0,width);if(strength<0.1f&&width==0&&g.activeCount-count>1)width=1;
  float x=direction*width*size*2,step=direction*size*2;
  for(int i=width*2+1;i>0;--i){if(count<g.activeCount){auto& s=g.slots[count++];
    s.relative.set(x,0,radius);s.position.set(cosine*x+sine*radius,0,-sine*x+cosine*radius);s.geometryKnown=true;}x-=step;}
  radius+=size*2;direction=-direction;
 }
 if(!std::isfinite(g.baseRadius)||!std::isfinite(g.moveRadius)||!std::isfinite(g.maxRadius))return fail(e,"nonfinite source CPlate geometry");
 return true;
}
bool Plate::refresh(Navi* n,int count,float strength,std::string& e){
 Operation operation(*this,e);if(!operation.ok())return false;Group* g=nullptr;
 if(!group(n,false,g,e))return false;
 // Genuine Navi calls refresh(mSlotCount,strength). A different census must
 // first retire its actual listeners; never discard live references by count.
 if(count<0||static_cast<unsigned>(count)!=g->count)return fail(e,"source CPlate refresh census differs from retained slots");
 Group next=*g;if(!geometry(next,strength,e)||!validate(*g,false,e)||mReentered)return false;
 g->slots=next.slots;g->activeCount=next.activeCount;g->baseRadius=next.baseRadius;
 g->moveRadius=next.moveRadius;g->maxRadius=next.maxRadius;g->maxRadiusKnown=true;
 return true;
}
bool Plate::position(Navi* n,bool gray,std::string& e){
 Operation operation(*this,e);if(!operation.ok())return false;Group* g=nullptr;
 if(!group(n,false,g,e))return false;
 PlatePose pose;
 if(!mSource.readPose(n,pose,e)||mReentered||!finite(pose.position)||!finite(pose.velocity)
 ||!std::isfinite(pose.angle)||!std::isfinite(pose.scale)||pose.scale<=0||pose.gray!=gray
 ||!validate(*g,false,e)||mReentered)return fail(e,"source CPlate evaluated SetPos pose unavailable");
 float offset=g->parameters.startingOffset*pose.scale;
 // Literal source reads PREVIOUS mVelocity before assigning the new velocity.
 if(std::sqrt(g->velocity.x*g->velocity.x+g->velocity.z*g->velocity.z)>5)offset=0;
 float radius=g->baseRadius+offset;
 const Vector3f base=pose.position+Vector3f(radius*std::sin(pose.angle),0,radius*std::cos(pose.angle));
 const Vector3f maximum=pose.position+Vector3f(g->maxRadius*std::sin(pose.angle),0,g->maxRadius*std::cos(pose.angle));
 if(!finite(base)||(g->maxRadiusKnown&&!finite(maximum)))return fail(e,"nonfinite source CPlate SetPos geometry");
 g->baseOffset=base;g->velocity=pose.velocity;g->positionKnown=true;g->scaleKnown=true;g->scale=pose.scale;
 if(!gray)g->angle=pose.angle;
 g->maxPositionKnown=g->maxRadiusKnown;
 if(g->maxPositionKnown)g->maxPositionOffset=maximum;
 return true;
}
bool Plate::setPos(Navi* n,std::string& e){return position(n,false,e);}
bool Plate::setPosGray(Navi* n,std::string& e){return position(n,true,e);}
bool Plate::rearrange(Navi* n,const Vector3f& target,std::string& e){
 Operation operation(*this,e);if(!operation.ok())return false;Group* g=nullptr;
 if(!finite(target)||!group(n,false,g,e))return false;
 Group next=*g;
 std::array<Vector3f,100> positions{};
 for(unsigned i=0;i<g->count;++i){NativeBodyFacts b;if(!nativeBodyFacts(g->slots[i].handle,b,e))return false;positions[i]=b.position;}
 for(int i=static_cast<int>(next.count)-1;i>=1;--i)for(int j=i;j>=1;--j){
  Vector3f a=positions[j]-target,b=positions[j-1]-target;
  if(a.x*a.x+a.y*a.y+a.z*a.z<b.x*b.x+b.y*b.y+b.z*b.z){std::swap(next.slots[j],next.slots[j-1]);std::swap(positions[j],positions[j-1]);}
 }
 return publish(*g,next,false,e);
}
bool Plate::shrink(Navi* n,std::string& e){Operation op(*this,e);if(!op.ok())return false;Group* g=nullptr;
 if(!group(n,false,g,e)||mReentered)return false;
 g->shrinkTimer=10;return true;}
bool Plate::update(Navi* n,std::string& e){Operation op(*this,e);if(!op.ok())return false;Group* g=nullptr;
 if(!group(n,false,g,e)||mReentered)return false;
 if(g->shrinkTimer)--g->shrinkTimer;return true;}
bool Plate::changeFlower(Handle h,Navi* n,std::string& e){Operation op(*this,e);if(!op.ok())return false;Group* g=nullptr;
 NativeBodyFacts b;if(!group(n,false,g,e)||!nativeBodyFacts(h,b,e))return false;
 int slot=-1;for(unsigned i=0;i<g->count;++i)if(same(g->slots[i].handle,h))slot=static_cast<int>(i);
 unsigned previous=(b.happa+2)%3;
 if(slot<0||g->slots[slot].happa!=previous||!g->happaCounts[previous]||!validate(*g,false,e)||mReentered)
  return fail(e,"source CPlate growth event does not match owned previous stage");
 --g->happaCounts[previous];++g->happaCounts[b.happa];g->slots[slot].happa=b.happa;return true;
}
bool Plate::growthCounts(Navi* n,std::array<unsigned,3>& out,std::string& e)const{
 ReadOperation op(*this);const Group* g=nullptr;
 for(const auto& candidate:mGroups)if(candidate.captain==n)g=&candidate;
 if(!g||!validate(*g,false,e)||!op.complete())return false;
 out=g->happaCounts;return true;
}
bool Plate::state(Navi* n,PlateState& out,std::string& e)const{
 ReadOperation op(*this);const Group* g=nullptr;
 for(const auto& candidate:mGroups)if(candidate.captain==n)g=&candidate;
 if(!g||!validate(*g,true,e)||!op.complete())return false;
 PlateState next;next.count=g->count;next.activeCount=g->activeCount;next.shrinkTimer=g->shrinkTimer;
 next.happaCounts=g->happaCounts;next.baseRadius=g->baseRadius;next.moveRadius=g->moveRadius;
 next.maxRadius=g->maxRadius;next.angle=g->angle;next.baseOffset=g->baseOffset;
 next.maxPositionOffset=g->maxPositionOffset;next.velocity=g->velocity;
 next.positionKnown=g->positionKnown;next.maxRadiusKnown=g->maxRadiusKnown;next.maxPositionKnown=g->maxPositionKnown;
 next.scaleKnown=g->scaleKnown;next.scale=g->scale;out=next;return true;
}
bool Plate::reset(std::string& e){Operation op(*this,e);if(!op.ok())return false;
 for(const auto& g:mGroups)if(g.count)return fail(e,"CPlate reset would discard live source slots");
 mGroups={};return true;}
unsigned Plate::retainedSlots()const noexcept{unsigned total=0;for(const auto& g:mGroups)total+=g.count;return total;}
} }
