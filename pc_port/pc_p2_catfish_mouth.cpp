#include "pc_p2_catfish_mouth.h"
#include "pc_p2_catfish_attachments.h"
#include "pc_p2_captor_host.h"
#include "pc_p2_body_coll.h"
#include "Collision.h"
#include "CreatureCollPart.h"
#include "sysNew.h"
#include <fstream>
#include <map>
#include <memory>
#include <cstdio>
extern Matrix4f invCamMat;
namespace {
struct Trees {
 CollInfo* body=nullptr;CollInfo* mouth=nullptr;CollInfo* chassis=nullptr;
 CollPart* nodes[3]={};CollPart* slots[2]={};
};
bool complete(const Trees& t){
 if(!t.body||!t.mouth)return false;
 for(auto* p:t.nodes)if(!p)return false;
 for(auto* p:t.slots)if(!p)return false;
 return true;
}
struct Actor {Trees trees;p2attach::Instance joints;p2attach::Token token=0;std::uint64_t tick=0;p2captor::Held<Piki> held;};
std::map<BTeki*,std::unique_ptr<Actor>> actors;
// Stable App heap parts outlive detached PikiSwallowedState/render references.
// Reuse one pair of trees per physical pool address; never delete live parts.
std::map<BTeki*,Trees> retiredTrees;
std::shared_ptr<const p2attach::Bank> bank;
std::string residentBytes;
u32 four(const char* s){return u32(s[0])<<24|u32(s[1])<<16|u32(s[2])<<8|u32(s[3]);}
ObjCollInfo* object(const char* id,const char* code,float radius){
 auto* n=new ObjCollInfo;n->mId.setID(four(id));n->mCode.setID(four(code));n->mRadius=radius;n->mJointIndex=-1;n->mCentrePosition.set(0,0,0);return n;
}
bool create(Trees& t){
 auto* root=object("none","____",22.5f);root->add(object("none","st__",10));root->add(object("none","st__",10));
 t.body=new CollInfo(8);t.body->initInfoTree(root);t.nodes[0]=t.body->getBoundingSphere();
 if(!t.nodes[0])return false;for(int i=1;i<3;++i)t.nodes[i]=t.nodes[0]->getChildAt(i-1);
 auto* mouth=object("slot","____",1);mouth->add(object("km01","____",20));mouth->add(object("km02","____",20));
 t.mouth=new CollInfo(8);t.mouth->initInfoTree(mouth);auto* mouthRoot=t.mouth->getBoundingSphere();if(!mouthRoot)return false;
 for(int i=0;i<2;++i)t.slots[i]=mouthRoot->getChildAt(i);
 for(auto* p:t.nodes){if(!p)return false;p->mIsUpdateActive=false;}
 mouthRoot->mIsUpdateActive=false;
 for(auto* p:t.slots){if(!p)return false;p->mIsUpdateActive=false;}
 return true;
}
void seat(CollPart* part,const p2attach::Affine& joint,p2attach::Vec offset,float radius){
 auto point=p2attach::point(joint,offset);part->mCentre.set(point.x,point.y,point.z);part->mRadius=radius;
 // getMatrix() premultiplies invCamMat. Store the inverse view rotation
 // times the full authored world basis, preserving scale and parent shear.
 Matrix4f basis,viewRotation,stored;basis.makeIdentity();viewRotation.makeIdentity();
 for(int r=0;r<3;++r)for(int c=0;c<3;++c){basis.mMtx[r][c]=joint.m[r][c];viewRotation.mMtx[r][c]=invCamMat.mMtx[c][r];}
 viewRotation.multiplyTo(basis,stored);part->mJointMatrix=stored;
}
}
bool pc_p2_catfish_mouth_resources(std::string& error){
 std::ifstream file("p2-original-catfish-attach.txt");auto staged=p2original::catfish::readAttachments(file,error);if(!staged)return false;
 file.clear();file.seekg(0);std::string bytes((std::istreambuf_iterator<char>(file)),{});
 if(bank&&bytes!=residentBytes){error="resident original Catfish joint bank changed";return false;}
 bank=std::move(staged);residentBytes=std::move(bytes);error.clear();return true;
}
bool pc_p2_catfish_mouth_birth(BTeki* a,std::string& error){
 if(!a||!gsys||!bank||!a->mCollInfo||actors.count(a)){error="original Catfish joint birth prerequisites unavailable";return false;}
 auto state=std::make_unique<Actor>();auto cached=retiredTrees.find(a);
 if(cached!=retiredTrees.end()){state->trees=cached->second;retiredTrees.erase(cached);}
 const int old=gsys->setHeap(SYSHEAP_App);
 // A partially allocated cache never qualifies for reuse.
 bool ready=complete(state->trees);
 if(!state->trees.body&&!state->trees.mouth)ready=create(state->trees)&&complete(state->trees);
 gsys->setHeap(old);
 if(!ready){retiredTrees[a]=state->trees;error="original Catfish physical parts allocation failed";return false;}
 state->token=state->joints.bind(bank);if(!state->token){error="original Catfish joint instance refused";return false;}
 pc_p2_body_coll_forget(a);state->trees.chassis=a->mCollInfo;a->mCollInfo=state->trees.body;a->mPlatMgr.release();
 actors.emplace(a,std::move(state));
 if(!pc_p2_catfish_mouth_follow(a,"wait1",0)){pc_p2_catfish_mouth_forget(a);error="original Catfish initial joint pose refused";return false;}
 std::printf("P2_ORIGINAL_CATFISH_PARTS source=26 body_nodes=3 mouth_slots=2 radius=20 authored_joints=1\n");error.clear();return true;
}
bool pc_p2_catfish_mouth_follow(BTeki* a,const std::string& clip,float frame){
 auto found=actors.find(a);if(found==actors.end())return false;auto& s=*found->second;
 const int index=bank->clip(clip);if(index<0)return false;
 Matrix4f world;world.makeSRT(Vector3f(1,1,1),Vector3f(0,a->getDirection(),0),a->getPosition());p2attach::Affine owner;
 for(int r=0;r<3;++r)for(int c=0;c<4;++c)owner.m[r][c]=world.mMtx[r][c];
 if(!s.joints.sample(s.token,index,frame,owner,++s.tick))return false;
 const int joint[3]={4,5,4};const p2attach::Vec offset[3]={{-2.5f,-2.5f,0},{7.5f,0,0},{2.5f,0,0}};
 for(int i=0;i<3;++i){p2attach::Affine posed;if(!s.joints.socket(s.token,joint[i],posed))return false;seat(s.trees.nodes[i],posed,offset[i],i?10:22.5f);}
 for(int i=0;i<2;++i){p2attach::Affine posed;if(!s.joints.socket(s.token,2+i,posed))return false;seat(s.trees.slots[i],posed,{0,0,0},20);}
 return true;
}
int pc_p2_catfish_mouth_eat(BTeki* a){
 auto found=actors.find(a);if(found==actors.end())return 0;auto& s=*found->second;
 bool occupied[2]={};p2captorhost::validate(a,s.held,2,occupied);auto scene=p2captorhost::snapshot(a);
 p2captor::Vec3 positions[2];for(int i=0;i<2;++i)positions[i]=p2captorhost::vec(s.trees.slots[i]->mCentre);
 return p2captor::eatAt(positions,2,20,scene.prey.data(),int(scene.prey.size()),occupied,p2captor::defaultEligible,[&](int n,int slot){
  Piki* p=scene.pikis[n];if(!p->stimulate(InteractSwallow(a,s.trees.slots[slot],0))||!p2captorhost::heldBy(a,p))return false;
  s.held.slot[slot]=p;return true;
 });
}
int pc_p2_catfish_mouth_swallow(BTeki* a){
 auto found=actors.find(a);if(found==actors.end())return 0;auto& s=*found->second;
 // Snapshot before kill/poison receivers mutate the owner sticker list.
 std::vector<Piki*> prey;for(Creature* c=a->mStickListHead;c;c=c->mNextSticker)if(c->isPiki()&&c->isStickToMouth())prey.push_back(static_cast<Piki*>(c));
 int killed=0;for(Piki* p:prey){const bool white=pc_p2_is_white(p);
  if(p2captorhost::heldBy(a,p)&&p->stimulate(InteractKill(a,0))){++killed;if(white)a->mStoredDamage+=300;}
 }
 s.held.clear();return killed;
}
void pc_p2_catfish_mouth_release(BTeki* a){auto found=actors.find(a);if(found!=actors.end())p2captorhost::release(a,found->second->held);}
void pc_p2_catfish_mouth_forget(BTeki* a){
 auto found=actors.find(a);if(found==actors.end())return;pc_p2_catfish_mouth_release(a);auto trees=found->second->trees;
 if(a->mCollInfo==trees.body)a->mCollInfo=trees.chassis;trees.chassis=nullptr;retiredTrees[a]=trees;actors.erase(found);
}
