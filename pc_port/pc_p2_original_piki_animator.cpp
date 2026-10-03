#include "pc_p2_original_piki_animator.h"
#include "pc_p2_original_piki_origin.h"
#include "pc_p2_original_captain_damage.h"
#include "pc_randomizer.h"
#include "launcher/sha256.h"
#include <array>
#include <map>
#include <sstream>
#include <cmath>
#include <limits>
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace p2original { namespace piki {
namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
struct MotionName {Motion motion;const char* name;unsigned id;};
const MotionName names[]={{Motion::Wait,"wait",31},{Motion::Walk,"walk",30},
 {Motion::Run2,"run2",29},{Motion::Hang,"hang",36},{Motion::RollJump,"rolljmp",35},
 {Motion::Notice,"kizuku",32},{Motion::Step,"asibumi",1},{Motion::Escape,"nigeru",28},
 {Motion::Yawn,"akubi",0},{Motion::Chat,"chatting",3},{Motion::Search,"sagasu2",54},
 {Motion::Irritated,"iraira",21},{Motion::Sit,"suwaru",56},{Motion::Sleep,"neru",57}};
const char* speciesName(unsigned s){return s==0?"blue":s==1?"red":s==2?"yellow":nullptr;}
std::string sha(const std::string& bytes){pikmin::launcher::Sha256 hash;hash.update(bytes.data(),bytes.size());return pikmin::launcher::toHex(hash.finish());}
bool digest(const std::string& x){return x.size()==64&&x.find_first_not_of("0123456789abcdef")==std::string::npos;}
bool tail(std::istringstream& s){std::string x;return !(s>>x);}
unsigned be16(const std::string& x,std::size_t n){return (unsigned(static_cast<unsigned char>(x[n]))<<8)|static_cast<unsigned char>(x[n+1]);}
unsigned be32(const std::string& x,std::size_t n){return (unsigned(static_cast<unsigned char>(x[n]))<<24)|(unsigned(static_cast<unsigned char>(x[n+1]))<<16)|(unsigned(static_cast<unsigned char>(x[n+2]))<<8)|static_cast<unsigned char>(x[n+3]);}
bool bca(const std::string& raw,unsigned duration){
 // SZS member extraction omits final alignment padding. Validate all actual
 // ANF1 tables/data against retained bytes, while declared sizes keep their
 // authored 32-byte alignment. Never read the absent padding as animation data.
 if(raw.size()<68||raw.compare(0,8,"J3D1bca1")||be32(raw,8)!=(raw.size()+31u)/32u*32u||be32(raw,12)!=1||raw.compare(32,4,"ANF1")||be32(raw,36)+32u!=be32(raw,8)||be16(raw,42)!=duration)return false;
 const unsigned joints=be16(raw,44),counts[]={be16(raw,46),be16(raw,48),be16(raw,50)};
 const unsigned offsets[]={be32(raw,56),be32(raw,60),be32(raw,64)},sizes[]={4,2,4};
 const unsigned table=be32(raw,52);if(!joints||table>raw.size()-32||joints>(raw.size()-32-table)/36)return false;
 for(unsigned i=0;i<3;++i)if(offsets[i]>raw.size()-32||counts[i]>(raw.size()-32-offsets[i])/sizes[i])return false;
 for(unsigned j=0;j<joints;++j)for(unsigned axis=0;axis<3;++axis)for(unsigned channel=0;channel<3;++channel){const auto off=32+table+j*36+axis*12+channel*4;const unsigned count=be16(raw,off),index=be16(raw,off+2);if(!count||index>counts[channel]||count>counts[channel]-index)return false;}
 return true;
}
struct Key {unsigned frame=0,type=0;};
struct Sample {unsigned frame=0;Shape* shape=nullptr;std::array<float,12> happa{};};
struct Clip {unsigned duration=0,id=0;std::string name,rawSha;std::vector<Key> keys;std::vector<Sample> samples;};
struct Bank {std::string prefix;std::array<Shape*,3> happa{};std::map<Motion,Clip> clips;};
// Retail registry is a linked list in authored order, not a sorted event list.
// getLowestAnimKey picks the first minimum qualifying frame; animate then walks
// the original successors. Equal-frame and unsorted authored keys retain order.
std::size_t lowest(const Clip& c,float frame){std::size_t out=c.keys.size();unsigned best=~0u;for(std::size_t i=0;i<c.keys.size();++i)if(c.keys[i].frame>=unsigned(frame)&&c.keys[i].frame<best){best=c.keys[i].frame;out=i;}return out;}
std::string tokens(const std::string& bytes){std::istringstream in(bytes);std::string out,line;while(std::getline(in,line)){line.erase(line.find('#')==std::string::npos?line.size():line.find('#'));out+=line;out+='\n';}return out;}
bool registry(const std::string& bytes,std::map<unsigned,std::pair<std::string,std::vector<Key>>>& out,std::string& e){
 std::istringstream in(tokens(bytes));unsigned count=0;if(!(in>>count)||count!=67)return fail(e,"invalid selected retail animation count");
 for(unsigned id=0;id<count;++id){std::string open,path,name,close;if(!(in>>open>>path>>name)||open!="{"||name.size()<5||name.substr(name.size()-4)!=".bca")return fail(e,"invalid selected animation registry record");
  std::vector<Key> keys;int frame=-2;while(in>>frame){if(frame==-1)break;unsigned type=0;if(frame<0||frame>65535||!(in>>type)||type>1000||keys.size()>=1024)return fail(e,"invalid selected animation key");keys.push_back({unsigned(frame),type});}
  if(frame!=-1||!(in>>close)||close!="}"||!out.emplace(id,std::make_pair(name.substr(0,name.size()-4),std::move(keys))).second)return fail(e,"invalid selected animation registry terminator");
 }
 std::string extra;if(in>>extra)return fail(e,"trailing selected animation registry bytes");return true;
}
}
struct NativeAnimator::Impl {
 AnimationBank& resources;
 const captain::LoadedScene* scene=nullptr;const captain::World* world=nullptr;std::array<Navi*,2> captains{};std::uint64_t incarnation=0,revision=0;
 std::string campaign,session,catalog;
 std::map<unsigned,Bank> banks;
 struct Actor {Handle handle;unsigned species=0;Motion motion=Motion::Wait;float timer=0,rate=30;std::size_t key=0;bool completed=false,finishing=false;std::uint64_t generation=0;};
 std::map<Piki*,Actor> actors;
 bool busy=false;
 explicit Impl(AnimationBank& r):resources(r){}
 bool current(std::string& e)const{
  auto* loaded=pc_p2_original_captain_loaded_scene();auto* w=pc_p2_original_captain_world();
  if(!scene||loaded!=scene||!world||w!=world||&resources.scene()!=scene)return fail(e,"source animator canonical scene changed");
  if(loaded->incarnation()!=incarnation||loaded->selectedCampaign()!=campaign||loaded->selectedFingerprint()!=session||loaded->sourceCatalog()!=catalog
    ||w->incarnation()!=incarnation||w->selectedCampaign()!=campaign||w->selectedFingerprint()!=session||w->sourceCatalog()!=catalog
    ||loaded->captainAt(0)!=captains[0]||loaded->captainAt(1)!=captains[1]||w->captainAt(0)!=captains[0]||w->captainAt(1)!=captains[1]
    ||pc_randomizer_original_selection_revision()!=revision||!revision)return fail(e,"source animator selected ownership changed");
  if(!resources.current(e))return false;
  if(pc_p2_original_captain_loaded_scene()!=loaded||pc_p2_original_captain_world()!=w||&resources.scene()!=loaded)return fail(e,"source animator resource callback changed scene");
  return loaded->incarnation()==incarnation&&loaded->selectedCampaign()==campaign&&loaded->selectedFingerprint()==session&&loaded->sourceCatalog()==catalog
   &&w->incarnation()==incarnation&&w->selectedCampaign()==campaign&&w->selectedFingerprint()==session&&w->sourceCatalog()==catalog
   &&loaded->captainAt(0)==captains[0]&&loaded->captainAt(1)==captains[1]&&w->captainAt(0)==captains[0]&&w->captainAt(1)==captains[1]
   &&pc_randomizer_original_selection_revision()==revision?true:fail(e,"source animator resource callback changed selection");
 }
 bool body(Handle h,OriginalPikiBodyHandle& value,std::string& e)const{
  if(!h.body||!h.lifetime||!current(e)||!pc_p2_original_piki_body_handle(h.body,value)||value.nativeLifetime!=h.lifetime)return fail(e,"source animator native lifetime changed");
  return true;
 }
 Actor* actor(Handle h,std::string& e){OriginalPikiBodyHandle value;if(!body(h,value,e))return nullptr;auto it=actors.find(h.body);if(it==actors.end()||it->second.handle.lifetime!=h.lifetime||it->second.species!=value.body.state.species){fail(e,"source animator actor is not attached");return nullptr;}return &it->second;}
 const Actor* actor(Handle h,std::string& e)const{return const_cast<Impl*>(this)->actor(h,e);}
 bool writable(std::string& e)const{return world&&(world->phase()==captain::Phase::Loading||world->phase()==captain::Phase::GameWorldActive)?true:fail(e,"source animator mutation requires owned Loading or Active World");}
 const Clip& clip(const Actor& a)const{return banks.at(a.species).clips.at(a.motion);}
};
NativeAnimator::NativeAnimator(AnimationBank& r):impl(new Impl(r)){}
// The owner must complete checked retirement before destroying this leaf.
NativeAnimator::~NativeAnimator()=default;
bool NativeAnimator::prepare(unsigned species,std::string& e){
 auto& p=*impl;const char* color=speciesName(species);if(!color||p.busy||!p.actors.empty()||p.banks.count(species))return fail(e,"source animation bank preparation refused");
 struct Busy {bool& value;Busy(bool& v):value(v){value=true;}~Busy(){value=false;}} busy(p.busy);
 auto* loaded=pc_p2_original_captain_loaded_scene();auto* world=pc_p2_original_captain_world();const auto revision=pc_randomizer_original_selection_revision();
 if(!loaded||&p.resources.scene()!=loaded||!world||!loaded->incarnation()||loaded->incarnation()!=world->incarnation()||loaded->selectedCampaign()!=world->selectedCampaign()||loaded->selectedFingerprint()!=world->selectedFingerprint()||loaded->sourceCatalog()!=world->sourceCatalog()
    ||loaded->captainAt(0)!=world->captainAt(0)||loaded->captainAt(1)!=world->captainAt(1)||!revision
    ||(world->phase()!=captain::Phase::Loading&&world->phase()!=captain::Phase::GameWorldActive))return fail(e,"source animation bank has no owned bootstrap scene");
 if(!p.resources.current(e)||pc_p2_original_captain_loaded_scene()!=loaded||pc_p2_original_captain_world()!=world||&p.resources.scene()!=loaded)return fail(e,"source animation preparation changed canonical owner");
 if(!p.banks.empty()&&!p.current(e))return false;
 const auto incarnation=loaded->incarnation();const auto campaign=loaded->selectedCampaign(),session=loaded->selectedFingerprint(),catalog=loaded->sourceCatalog();
 const std::array<Navi*,2> captains{{loaded->captainAt(0),loaded->captainAt(1)}};
 Bank bank;bank.prefix=std::string("p2-original/piki-bodies/")+color+"/";std::string bytes,reg;
 for(unsigned i=0;i<3;++i){bank.happa[i]=p.resources.shape(bank.prefix+color+"_happa_"+std::to_string(i)+".mod");if(!bank.happa[i])return fail(e,"missing genuine selected growth Shape");}
 if(!pc_randomizer_original_input(bank.prefix+"bank.txt",bytes,e)||!pc_randomizer_original_input(bank.prefix+"animmgr.txt",reg,e))return false;
 std::map<unsigned,std::pair<std::string,std::vector<Key>>> registrations;if(!registry(reg,registrations,e))return false;
 std::istringstream lines(bytes);std::string line;if(!std::getline(lines,line))return fail(e,"empty selected source animation bank");
 std::istringstream header(line);std::string magic,modelHash,paramHash;if(!(header>>magic>>modelHash>>paramHash)||magic!="P2_SOURCE_PIKI_BANK_1"||!digest(modelHash)||!digest(paramHash)||!tail(header))return fail(e,"invalid selected source animation bank header");
 std::string params;if(!pc_randomizer_original_input(bank.prefix+"pikiParms.txt",params,e)||sha(params)!=paramHash)return fail(e,"selected source animation parameter identity changed");
 std::string model;if(!pc_randomizer_original_input(bank.prefix+color+".bmd",model,e)||sha(model)!=modelHash)return fail(e,"selected source animation model identity changed");
 Clip* active=nullptr;std::size_t happa=0;
 while(std::getline(lines,line)){if(line.empty()||line=="\r")continue;std::istringstream row(line);std::string kind,name;row>>kind>>name;
  if(kind=="clip"){
   if(active&&happa!=active->samples.size())return fail(e,"incomplete selected happa poses");
   const MotionName* found=nullptr;for(const auto& n:names)if(name==n.name)found=&n;
   if(!found)return fail(e,"unexpected selected source clip");
   Clip c;c.name=name;c.id=found->id;unsigned count=0;
   if(!(row>>c.duration>>c.rawSha>>count)||!c.duration||c.duration>65535||!digest(c.rawSha)||!count||count>4096||bank.clips.count(found->motion))return fail(e,"invalid selected source clip descriptor");
   for(unsigned i=0;i<count;++i){Sample sample;if(!(row>>sample.frame)||sample.frame>=c.duration||(i&&sample.frame<=c.samples.back().frame))return fail(e,"invalid selected source sampled frames");
    char suffix[24];std::snprintf(suffix,sizeof(suffix),"_%02u.mod",i);const std::string role=bank.prefix+color+"_"+name+suffix;
    sample.shape=p.resources.shape(role);if(!sample.shape)return fail(e,"missing genuine selected source pose Shape");c.samples.push_back(sample);}
   if(!tail(row)||c.samples.front().frame!=0||c.samples.back().frame!=c.duration-1)return fail(e,"selected source clip endpoint missing");
   const auto r=registrations.find(c.id);if(r==registrations.end()||r->second.first!=name)return fail(e,"selected source motion ID mismatch");c.keys=r->second.second;
   for(const auto& key:c.keys)if(key.frame>=c.duration)return fail(e,"selected source key exceeds actual animation duration");
   std::string raw;if(!pc_randomizer_original_input(bank.prefix+name+".bca",raw,e)||sha(raw)!=c.rawSha||!bca(raw,c.duration))return fail(e,"selected source BCA duration or identity mismatch");
   active=&bank.clips.emplace(found->motion,std::move(c)).first->second;happa=0;
  }else if(kind=="happa"){
   unsigned index=0;if(!active||name!=active->name||!(row>>index)||index!=happa||index>=active->samples.size())return fail(e,"invalid selected happa sample association");
   for(auto& v:active->samples[index].happa)if(!(row>>v)||!std::isfinite(v))return fail(e,"invalid selected happa transform");
   if(!tail(row))return fail(e,"trailing selected happa transform bytes");
   ++happa;
  }else return fail(e,"unexpected selected source bank row");
 }
 if(!active||happa!=active->samples.size()||bank.clips.size()!=sizeof(names)/sizeof(names[0]))return fail(e,"source animation bank lacks actual required motions");
 if(pc_p2_original_captain_loaded_scene()!=loaded||pc_p2_original_captain_world()!=world||&p.resources.scene()!=loaded||loaded->incarnation()!=incarnation||loaded->selectedCampaign()!=campaign||loaded->selectedFingerprint()!=session||loaded->sourceCatalog()!=catalog
   ||world->incarnation()!=incarnation||world->selectedCampaign()!=campaign||world->selectedFingerprint()!=session||world->sourceCatalog()!=catalog
   ||loaded->captainAt(0)!=captains[0]||loaded->captainAt(1)!=captains[1]||world->captainAt(0)!=captains[0]||world->captainAt(1)!=captains[1]
   ||pc_randomizer_original_selection_revision()!=revision||!p.resources.current(e))return fail(e,"source animation preparation owner changed");
 p.scene=loaded;p.world=world;p.captains=captains;p.incarnation=incarnation;p.revision=revision;p.campaign=campaign;p.session=session;p.catalog=catalog;p.banks.emplace(species,std::move(bank));
 if(!p.current(e)){p.banks.erase(species);if(p.banks.empty()){p.scene=nullptr;p.world=nullptr;}return false;}return true;
}
bool NativeAnimator::attach(Handle h,std::string& e){auto& p=*impl;OriginalPikiBodyHandle body;if(p.busy||!p.body(h,body,e)||!p.writable(e)||!p.banks.count(body.body.state.species)||p.actors.count(h.body))return fail(e,"source animator attachment refused");Impl::Actor a;a.handle=h;a.species=body.body.state.species;a.key=lowest(p.banks.at(a.species).clips.at(a.motion),0);p.actors.emplace(h.body,a);return true;}
bool NativeAnimator::supports(Handle h,Motion m,std::string& e)const{OriginalPikiBodyHandle body;auto& p=*impl;if(!p.body(h,body,e))return false;auto b=p.banks.find(body.body.state.species);return b!=p.banks.end()&&b->second.clips.count(m)?true:fail(e,"missing authenticated source motion");}
bool NativeAnimator::start(Handle h,Motion m,std::string& e){auto& p=*impl;if(!supports(h,m,e))return false;auto* a=p.actor(h,e);if(!a||!p.writable(e))return false;if(a->generation==std::numeric_limits<std::uint64_t>::max())return fail(e,"source animator motion generation exhausted");a->motion=m;a->timer=0;a->key=lowest(p.clip(*a),0);a->completed=false;a->finishing=false;++a->generation;return true;}
bool NativeAnimator::status(Handle h,Motion& m,float& speed,bool& completed,std::string& e)const{auto* a=impl->actor(h,e);if(!a)return false;m=a->motion;speed=a->rate;completed=a->completed;return true;}
bool NativeAnimator::speed(Handle h,float rate,std::string& e){auto* a=impl->actor(h,e);if(!a||!impl->writable(e)||!std::isfinite(rate)||rate<0)return fail(e,"invalid source animation playback rate");a->rate=rate;return true;}
bool NativeAnimator::finish(Handle h,std::string& e){auto* a=impl->actor(h,e);if(!a||!impl->writable(e))return false;a->finishing=true;return true;}
bool NativeAnimator::loopStart(Handle h,std::string& e){auto& p=*impl;auto* a=p.actor(h,e);if(!a||!p.writable(e))return false;const auto& c=p.clip(*a);auto it=std::find_if(c.keys.begin(),c.keys.end(),[](const Key& k){return k.type==0;});if(it!=c.keys.end()){a->timer=float(it->frame);a->key=lowest(c,a->timer);a->completed=false;a->finishing=false;}return true;}
bool NativeAnimator::advance(Handle h,float seconds,std::string& e){
 auto& p=*impl;if(p.busy||!std::isfinite(seconds)||seconds<0)return fail(e,"source animator advance refused");
 struct Busy {bool& b;Busy(bool& x):b(x){b=true;}~Busy(){b=false;}} busy(p.busy);
 auto* a=p.actor(h,e);if(!a)return false;
 if(p.world->phase()!=captain::Phase::GameWorldActive)return fail(e,"source animation advance requires actual active World");
 const auto generation=a->generation;const auto& c=p.clip(*a);const float advance=a->rate*seconds;if(!std::isfinite(advance)||advance>65535)return fail(e,"source animation frame advance overflow");a->timer+=advance;
 bool loop=false;
 while(!loop&&a->key<c.keys.size()&&c.keys[a->key].frame<unsigned(a->timer)){
  const auto key=c.keys[a->key];
  if(!animationKey(h,key.type,e)||!p.actor(h,e))return false;
  if(a->generation!=generation)return true; // authored receiver started a new motion
  if(key.type==1&&!a->finishing){std::size_t start=a->key;while(start&&c.keys[--start].type!=0){}if(c.keys[start].type!=0)return fail(e,"source animation LOOP_END lacks LOOP_START");a->timer=float(c.keys[start].frame);loop=true;}
  ++a->key;
 }
 if(loop)a->key=lowest(c,a->timer);
 if(a->timer>=c.duration){a->timer=float(c.duration-1);if(!a->completed){a->completed=true;if(!animationKey(h,1000,e)||!p.actor(h,e))return false;}}
 return p.current(e);
}
bool NativeAnimator::pose(Handle h,AnimatedPose& out,std::string& e)const{auto& p=*impl;auto* a=p.actor(h,e);if(!a)return false;const auto& c=p.clip(*a);const Sample* sample=&c.samples.front();for(const auto& s:c.samples){if(float(s.frame)>a->timer)break;sample=&s;}AnimatedPose value;value.shape=sample->shape;value.sourceFrame=a->timer;value.sampledFrame=sample->frame;std::copy(sample->happa.begin(),sample->happa.end(),value.happa);std::copy(p.banks.at(a->species).happa.begin(),p.banks.at(a->species).happa.end(),value.happaShapes);out=value;return true;}
bool NativeAnimator::canRetire(std::string& e)const{auto& p=*impl;if(p.busy)return fail(e,"source animator callback is in flight");if(p.banks.empty()&&p.actors.empty())return true;if(!p.current(e))return false;for(const auto& item:p.actors)if(!pc_p2_original_piki_body_current(item.first,item.second.handle.lifetime))return fail(e,"source animator cleanup lifetime changed");if(piki::owned())return fail(e,"source runtime still owns animator consumers");return true;}
bool NativeAnimator::retire(std::string& e){if(!canRetire(e))return false;impl->actors.clear();impl->banks.clear();impl->scene=nullptr;impl->world=nullptr;impl->captains={};impl->incarnation=impl->revision=0;impl->campaign.clear();impl->session.clear();impl->catalog.clear();return true;}
std::size_t NativeAnimator::retainedBodies()const noexcept{return impl->actors.size();}
bool NativeAnimator::owned()const noexcept{return impl->busy||!impl->banks.empty()||!impl->actors.empty();}
} }
