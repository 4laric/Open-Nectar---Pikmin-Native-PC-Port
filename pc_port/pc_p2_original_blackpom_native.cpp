#include "pc_p2_original_blackpom_native.h"
#include "pc_p2_pose_family.h"
#include "Pom.h"
#include "Generator.h"
#include "Graphics.h"
#include "Camera.h"
#include "sysNew.h"
#include <array>
#include <cmath>
#include <fstream>
#include <map>
#include <cstdio>
#include <cstdlib>
namespace p2original { namespace blackpom {
namespace {
const char* clips[]={"wait","dead","type1","type2","type3","type4"};
const int durations[]={1,40,30,30,40,20};
bool refuse(std::string& e,const char* s){e=s;return false;}
struct AppHeap {int previous;AppHeap():previous(gsys->setHeap(SYSHEAP_App)){}~AppHeap(){gsys->setHeap(previous);}};
}
struct Native::Impl {
 Mechanic& mechanic;p2posefamily::Bank bank{"ORIGINAL_BLACKPOM"};
 p2poseload::Shared shared;std::array<std::vector<Shape*>,6> shapes;
 std::map<Pom*,unsigned> actors;unsigned reserved=0;bool loaded=false;
 explicit Impl(Mechanic& core):mechanic(core){}
 bool load(std::string& e){
  if(loaded)return true;
  std::ifstream in("p2-original-blackpom-bank.txt");std::string magic,name,stem,word,extra;
  unsigned source;int count,duration;
  if(!(in>>magic>>source)||magic!="P2_ORIGINAL_BLACKPOM_BANK_1"||source!=6)
   return refuse(e,"actual BlackPom resource index absent");
  std::size_t total=0;
  for(unsigned i=0;i<6;++i){
   if(!(in>>word>>name>>count>>duration>>stem)||word!="clip"||name!=clips[i]
      ||duration!=durations[i]||count<1||count>64||stem!="flora_BlackPom_"+name)
    return refuse(e,"actual BlackPom clip identity mismatch");
   std::vector<int> frames;
   if(!(in>>word)||word!="frames")return refuse(e,"BlackPom source frame list absent");
   for(int j=0;j<count;++j){int frame;if(!(in>>frame))return refuse(e,"BlackPom source frame list truncated");frames.push_back(frame);}
   if(!p2posefamily::Bank::validFrames(frames,count,duration))return refuse(e,"BlackPom source frames invalid");
   if(!p2posefamily::loadFamilyClip(bank,name,stem,count,duration,frames,shared,total,shapes[i],e))return false;
   // Approximate materials are an explicit presentation limit. Missing poses
   // cannot silently select the inherited P1 flower or Chappy model.
   if(shapes[i].empty())return refuse(e,"BlackPom physical poses missing");
  }
  if(in>>extra)return refuse(e,"BlackPom resource index trailing data");
  loaded=true;return true;
 }
};
Native::Native(Mechanic& core):m(std::make_unique<Impl>(core)){}
Native::~Native(){if(!m->actors.empty()){std::fputs("BlackPom owner destroyed before actual root release\n",stderr);std::abort();}}
bool Native::prepare(unsigned count,std::string& e){
 if(!count||count>10||!gsys||!bossMgr)return refuse(e,"BlackPom managers/count unavailable");
 if(!m->actors.empty()||m->reserved)return refuse(e,"BlackPom factory already reserved");
 AppHeap heap;
 if(!m->load(e)||!m->mechanic.preflight(e))return false;
 if(bossMgr->pcOriginalPomCapacity()<int(count))return refuse(e,"BlackPom native Pom pool capacity insufficient");
 m->reserved=count;e.clear();return true;
}
bool Native::birth(Generator* gen,const Vector3f& p,float yaw,const BirthContext& context,
                   Pom*& out,bool& wasSuppressed,std::string& e){
 out=nullptr;wasSuppressed=false;
 if(!m->loaded||!m->reserved||!bossMgr||!gen||!std::isfinite(p.x)||!std::isfinite(p.y)
    ||!std::isfinite(p.z)||!std::isfinite(yaw))return refuse(e,"BlackPom birth lacks prepared physical slot");
 const auto* row=originalActors().find(gen->_70);
 if(!row||row->enemy.source!=6)return refuse(e,"BlackPom generator UID is not admitted original source6");
 if(suppressed(context)){wasSuppressed=true;--m->reserved;e.clear();return true;}
 AppHeap heap;BirthInfo info;info.set(p,Vector3f(0,yaw,0),Vector3f(1,1,1),gen);
 Boss* root=bossMgr->pcAllocateOriginalPom(info);
 if(!root)return refuse(e,"BlackPom actual Pom manager allocation failed");
 out=static_cast<Pom*>(root);m->actors.emplace(out,0);--m->reserved;e.clear();return true;
}
bool Native::bind(Pom* body,unsigned token,std::string& e){
 auto it=m->actors.find(body);if(it==m->actors.end()||it->second||!token)return refuse(e,"BlackPom binding lacks new owned root");
 unsigned source=0,actualToken=0;InstanceIdentity identity;
 if(!originalActors().query(body,source,actualToken,&identity)||source!=6||token!=actualToken
    ||identity.catalog.empty()||!identity.generator||!identity.epoch||!identity.activation)
  return refuse(e,"BlackPom root lacks actual original source identity");
 // Mark ownership before callback: a failed partial bind still needs release.
 it->second=token;
 if(!m->mechanic.bind(body,identity,token,e)||!m->mechanic.start(body,e))return false;
 e.clear();return true;
}
bool Native::release(Pom* body,std::string& e){
 auto it=m->actors.find(body);if(it==m->actors.end())return refuse(e,"BlackPom release does not own root");
 if(it->second){if(!m->mechanic.release(body,e))return false;it->second=0;}
 // Floor owner retires its original registry handle before freeing manager slot.
 unsigned source=0,token=0;if(originalActors().query(body,source,token))return refuse(e,"BlackPom registry authority must retire before pool reuse");
 // Revoke leaf dispatch before kill enters native retirement hooks.
 m->actors.erase(it);body->kill(false);e.clear();return true;
}
bool Native::nativeRetired(Pom* body,std::string& e){
 auto it=m->actors.find(body);if(it==m->actors.end())return refuse(e,"BlackPom native retirement does not own root");
 unsigned source=0,token=0;
 if(originalActors().query(body,source,token))return refuse(e,"BlackPom native retirement still has original registry authority");
 if(it->second&&!m->mechanic.release(body,e))return false;
 m->actors.erase(it);e.clear();return true;
}
bool Native::cancel(std::string& e){
 if(!m->actors.empty())return refuse(e,"BlackPom cancel requires partial roots released");
 m->reserved=0;e.clear();return true;
}
bool Native::owns(const Creature* body)const{return m->actors.count(const_cast<Pom*>(dynamic_cast<const Pom*>(body)))!=0;}
bool Native::draw(Pom* body,Graphics& gfx){
 auto it=m->actors.find(body);if(it==m->actors.end())return false;
 if(!it->second||!gfx.mCamera)return true;
 unsigned source=0,token=0;
 if(!originalActors().query(body,source,token)||source!=6||token!=it->second)return true;
 unsigned motion;float frame;if(!m->mechanic.pose(body,motion,frame)||motion>=6||!std::isfinite(frame)||frame<0)return true;
 auto* clip=m->bank.clip(clips[motion]);const auto& shapes=m->shapes[motion];
 std::size_t index=0;
 if(clip){for(std::size_t i=1;i<clip->frames.size();++i)if(std::fabs(float(clip->frames[i])-frame)<std::fabs(float(clip->frames[index])-frame))index=i;}
 else index=std::min(shapes.size()-1,std::size_t(frame*shapes.size()/durations[motion]));
 Matrix4f root,view;root.makeSRT(Vector3f(1,1,1),Vector3f(0,body->mFaceDirection,0),body->mSRT.t);gfx.mCamera->mLookAtMtx.multiplyTo(root,view);
 gfx.useMatrix(Matrix4f::ident,0);shapes[index]->updateAnim(gfx,view,nullptr,body);shapes[index]->drawshape(gfx,*gfx.mCamera,nullptr);return true;
}
} }
