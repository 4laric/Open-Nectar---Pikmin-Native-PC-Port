// Real engine objects, synthetic typed payload roundtrip. This is NOT a
// fresh-process campaign resume or a gameplay qualification.
#include "system.h"
#include "App.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "PikiMgr.h"
#include "PikiState.h"
#include "GameStat.h"
#include "FormationMgr.h"
#include <cstring>
#include <vector>
#include "Node.h"
#include "MoviePlayer.h"
#include "pc_midday_actor_archive.h"
#include "p2_midday_piki_engine_checks.h"
#include "pc_bbft.h"
#include "pc_window.h"
#include "pc_gpu_preference.h"
#include "pc_coop.h"
#include "pc_randomizer.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
#include <SDL2/SDL.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
using namespace pc_midday;
namespace {
bool strongStorageOnly=false;
void require(bool b,const std::string& message){if(!b){std::printf("FAIL MIDDAY_ACTOR_ENGINE %s\n",message.c_str());std::fflush(nullptr);std::_Exit(1);}}
struct Resolver : LogicalResolver {
    std::map<std::pair<RefKind,const void*>,u64> ids;
    std::map<std::pair<RefKind,u64>,void*> objects;
    std::map<std::string,LogicalRef> identifiedRoles;
    u64 next=1;
    Navi* owner=nullptr;
    Creature* typedOwner=nullptr;
    bool identify(const char* key,RefKind k,const void* p,LogicalRef& ref,std::string&) override {
        if(!p){ref={};return true;}auto identity=std::make_pair(k,p);
        if(k==RefKind::SlotListener) {
            std::string occupant=key;auto suffix=occupant.rfind(".listener");
            if(suffix==std::string::npos)return false;
            occupant.replace(suffix,9,".occupant");
            auto found=identifiedRoles.find(occupant);if(found==identifiedRoles.end()||!found->second.owner)return false;
            ref=found->second;ids[identity]=ref.owner;objects[{k,ref.owner}]=const_cast<void*>(p);return true;
        }
        // Formation resources use the owning captain's incarnation, matching
        // the compiled Piki action ownerLink contract.
        if(k==RefKind::CPlate || k==RefKind::FormationMgr) {
            auto* piki=dynamic_cast<Piki*>(typedOwner);
            Navi* captain=piki?piki->mNavi:owner;
            if(!captain || (k==RefKind::CPlate ? p!=captain->mPlateMgr : p!=captain->mFormMgr))return false;
            LogicalRef captainRef;std::string error;
            if(!identify("fixture.captain",RefKind::Creature,captain,captainRef,error))return false;
            ids[identity]=captainRef.owner;objects[{k,captainRef.owner}]=const_cast<void*>(p);ref=captainRef;return true;
        }
        if(!ids.count(identity)){ids[identity]=next;objects[{k,next}]=const_cast<void*>(p);++next;}
        ref={ids[identity],0,0};identifiedRoles[key]=ref;return true;
    }
    bool validate(const char* key,RefKind k,const LogicalRef& r,std::string&) const override{if(r.resource||r.slot||!objects.count({k,r.owner}))return false;
        void* p=objects.at({k,r.owner});std::string role=key;
        if(role=="demon.captain"||role=="escape.captain"||role=="state.mNavi"||role.find(".captain")!=std::string::npos)return p==owner;
        if(role=="state.mTargetPiki"||role=="state.mHeldThrowPiki"||role=="state.mPendingThrowPiki")return dynamic_cast<Piki*>(static_cast<Creature*>(p))!=nullptr;
        return true;}
    // Structural fixture only: pointers were identified from real typed fields
    // in this process. Production restore must validate the persisted catalog.
    bool validateTyped(const FieldSchema& d,const LogicalRef& r,std::string& e) const override {
        if(d.targetType.empty())return false;
        if(!r.owner&&!r.resource&&!r.slot)return d.nullable;
        if(!validate(d.key.c_str(),d.reference,r,e))return false;
        auto p=objects.at({d.reference,r.owner});
        if(d.ownership==ReferenceOwnership::Self)return p==(typedOwner?typedOwner:owner);
        if(d.reference==RefKind::Creature&&d.targetType=="Piki")return dynamic_cast<Piki*>(static_cast<Creature*>(p))!=nullptr;
        if(d.reference==RefKind::Creature&&d.targetType=="Navi")return dynamic_cast<Navi*>(static_cast<Creature*>(p))!=nullptr;
        return true;
    }
    bool resolve(const char* key,RefKind k,const LogicalRef& r,void*& p,std::string& e) override {if(!validate(key,k,r,e))return false;p=objects[{k,r.owner}];return true;}
    bool identifyHandle(const char* key,RefKind,u32 value,LogicalRef& ref,std::string& e) override {if(value==0){ref={};return true;}e="fixture does not invent path handle identity";return false;}
    bool resolveHandle(const char* key,RefKind,const LogicalRef& ref,u32& value,std::string&) override{if(!ref.owner&&!ref.resource&&!ref.slot){value=0;return true;}return false;}
};
void value(ActorFields& fields,const char* key,ScalarKind kind,u64 bits){ActorField f;f.scalar=kind;f.bits=bits;fields[key]=f;}
struct StorageCensus:StrongStorageVisitor {
 std::map<std::string,const void*> keys;std::set<const void*> slots;
 bool visit(const char* key,const StrongStorageSlot& slot,std::string& error)override{
  if(!slot.storage||!slot.owner||!slot.ownerType||!*slot.ownerType||!slot.member||!*slot.member||!keys.emplace(key,slot.storage).second||!slots.insert(slot.storage).second){error="duplicate or missing storage metadata";return false;}return true;
 }
};
void runStorage(){
 std::string error;const double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
 std::vector<Navi*> captains;std::vector<Piki*> pikis;std::map<Creature*,int> counts;std::set<const void*> allSlots;
 for(int i=0;i<naviMgr->getNaviCount();++i){auto* n=naviMgr->getNavi(i);require(n&&n->getCurrState()&&n->mHealth>1,"initialized healthy captain");captains.push_back(n);counts[n]=n->mCount;}
 Iterator it(pikiMgr);for(it.first();!it.isDone();it.next()){auto* p=static_cast<Piki*>(*it);require(p&&p->mHealth>0&&counts.emplace(p,p->mCount).second,"unique live Piki");pikis.push_back(p);}
 require(!captains.empty()&&pikis.size()==20,"actual captain and20 Piki inventory");
 auto check=[&](const std::vector<FieldSchema>& schema,const StorageCensus& census){size_t expected=0;for(auto& d:schema)if(d.strength==ReferenceStrength::StrongCreature){++expected;require(census.keys.count(d.key)==1,"exact strong schema key present");}require(expected==census.keys.size(),"no extra storage keys");for(auto slot:census.slots)require(allSlots.insert(slot).second,"globally unique native wrapper address");};
 for(size_t i=0;i<captains.size();++i){auto& n=*captains[i];Resolver resolver;resolver.owner=&n;resolver.typedOwner=&n;ActorBytes before,after;ActorFields fields;std::vector<FieldSchema> schema;StorageCensus census;
  require(capture_navi(n,resolver,now,before,error)&&decode_actor_fields(before,fields,error)&&navi_schema(fields,schema,error)&&visit_navi_strong_storage(n,fields,census,error),"Navi storage: "+error);check(schema,census);require(capture_navi(n,resolver,now,after,error)&&before==after,"Navi observation unchanged");std::printf("MIDDAY_STRONG_ACTOR family=Navi index=%zu slots=%zu keys_exact=1 unchanged=1\n",i,census.slots.size());}
 for(size_t i=0;i<pikis.size();++i){auto& p=*pikis[i];Resolver resolver;resolver.owner=p.mNavi;resolver.typedOwner=&p;ActorBytes bytes;ActorFields fields;std::vector<FieldSchema> schema;StorageCensus census;
  require(pc_midday_test_piki(p,resolver,now,error,true),"Piki storage/selection: "+error);require(capture_piki(p,resolver,now,bytes,error)&&decode_actor_fields(bytes,fields,error)&&piki_schema(fields,schema,error)&&visit_piki_strong_storage(p,fields,census,error),"Piki census: "+error);check(schema,census);std::printf("MIDDAY_STRONG_ACTOR family=Piki index=%zu slots=%zu keys_exact=1 selector_independent=1 unchanged=1\n",i,census.slots.size());}
 // Standalone real default constructor only; no owner insertion or reference callbacks.
 FormPoint point;StorageCensus form;require(visit_formpoint_strong_storage(point,form,error)&&form.keys.size()==1&&form.keys.at("formpoint.mOwner")==&point.mOwner&&point.mOwner.mPtr==nullptr,"actual empty FormPoint wrapper");require(allSlots.insert(&point.mOwner).second,"FormPoint storage unique");
 for(auto& entry:counts)require(entry.first->mCount==entry.second,"live actor reference count unchanged");
 std::printf("PASS MIDDAY_STRONG_STORAGE captains=%zu pikis=20 slots=%zu formpoint_exercised=1 formpoint_empty=1 counts_unchanged=1 synthetic_bind=0 fresh_process_resume=0\n",captains.size(),allSlots.size());std::fflush(nullptr);std::_Exit(0);
}
void run(Navi& n) {
    Resolver resolver;resolver.owner=&n;std::string error;double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    ActorBytes original;require(capture_navi(n,resolver,now,original,error),"actual Navi capture: "+error);
    LogicalRef self;require(resolver.identify("self",RefKind::Creature,&n,self,error),"fixture logical self");
    Iterator pikis(pikiMgr);pikis.first();require(!pikis.isDone(),"real Piki required");
    auto* piki=static_cast<Piki*>(*pikis);
    resolver.typedOwner=piki;
    require(pc_midday_test_piki(*piki,resolver,now,error),"actual Piki timer/frame/action component: "+error);
    resolver.typedOwner=&n;
    ActorFields originalFields;require(decode_actor_fields(original,originalFields,error),"original fields");
    // First allocate two tokens in the existing empty constructor-owned pool.
    // No engine tick executes while these synthetic component payloads are bound.
    for(int id=0;id<38;++id) {
        ActorFields fields=originalFields;
        for(auto it=fields.begin();it!=fields.end();) {if(it->first.rfind("state.",0)==0)it=fields.erase(it);else ++it;}
        value(fields,"current",ScalarKind::S32,id);value(fields,"last",ScalarKind::S32,0xffffffff);
        value(fields,"demon.policy.phase",ScalarKind::S32,id==36?1:0);
        value(fields,"demon.listeners.count",ScalarKind::U32,2);
        if(id==25){value(fields,"state.current",ScalarKind::S32,0);value(fields,"state.last",ScalarKind::S32,0xffffffff);}
        fields["demon.generation"].bits=2;fields["demon.policy.generation"].bits=2;fields["demon.serial"].bits=2;
        std::vector<FieldSchema> schema;require(navi_schema(fields,schema,error),"real Navi schema");
        for(const auto& s:schema)if(!fields.count(s.key)) {
            ActorField f;f.category=s.category;f.scalar=s.scalar;f.reference=s.reference;
            if(s.category!=FieldCategory::Scalar&&!s.nullable)f.target=self;
            fields[s.key]=f;
        }
        if(id==36)fields["demon.captain"].target=self;
        if(id==37)fields["escape.captain"].target=self;
        fields["demon.expected"].bits=29;
        fields["demon.listeners.0.generation"].bits=1;fields["demon.listeners.0.serial"].bits=1;
        fields["demon.listeners.1.generation"].bits=2;fields["demon.listeners.1.serial"].bits=2;
        ActorBytes bytes;require(encode_actor_fields(fields,bytes,error)&&validate_navi(bytes,resolver,error),"real Navi payload valid: "+error);
        if(id==0)require(allocate_navi_subobjects(n,bytes,resolver,now,error),"token allocation: "+error);
        require(bind_navi(n,bytes,resolver,now,error),"actual Navi bind: "+error);
        require(n.getCurrState() && n.getCurrState()->getID()==id && n.mStateMachine->mLastStateID==-1,"direct current/last bind");
        ActorBytes roundtrip;require(capture_navi(n,resolver,now,roundtrip,error)&&bytes==roundtrip,"actual Navi exact roundtrip: "+error);
        auto* token=navi_listener_at(n,0);u32 index=99;
        require(token&&navi_listener_index(n,token,index)&&index==0,"stable inactive listener subobject");
        if(id==0){auto* before=n.getCurrState();const float health=n.mHealth;PaniAnimKeyEvent event(0);token->animationKeyUpdated(event);require(n.getCurrState()==before&&n.mHealth==health,"stale inactive listener inert");}
    }
    std::printf("PASS MIDDAY_ACTOR_ENGINE navi_states=38 real_piki_component=1 stable_tokens=2 direct_bind_structural=1 synthetic_payload=1 fresh_process_resume=0\n");std::fflush(nullptr);std::_Exit(0);
}
class TestApp final : public PlugPikiApp {
    bool walkRestored=false;
    Navi* restoredNavi=nullptr;
    float restoredHealth=0;
    std::chrono::steady_clock::time_point started=std::chrono::steady_clock::now();
public:
    int idle() override {
        require(std::chrono::steady_clock::now()-started<std::chrono::seconds(55),"bounded readiness");
        if(naviMgr&&naviMgr->getActiveNavi()) {
            auto* n=naviMgr->getActiveNavi();
            if(GameStat::orimaDead || n->mHealth<=1 || (n->getCurrState()&&n->getCurrState()->getID()==NAVISTATE_Dead)) {
                std::printf("P2_FIXTURE_CAPTAIN_DOWN outcome=BLOCKED\n");std::fflush(nullptr);std::_Exit(86);
            }
        }
        int result=PlugPikiApp::idle();
        if(gameflow.mMoviePlayer&&gameflow.mMoviePlayer->mIsActive){gameflow.mMoviePlayer->requestSkip();return result;}
        if(!pc_randomizer_ready()||!naviMgr||!pikiMgr||gameflow.mPauseAll||gameflow.mIsUIOverlayActive)return result;
        auto* n=naviMgr->getActiveNavi();if(!n||!n->getCurrState()||n->getCurrState()->getID()!=NAVISTATE_Walk)return result;
        int count=0;Iterator it(pikiMgr);for(it.first();!it.isDone();it.next())++count;
        if(count!=20)return result;
        if(strongStorageOnly)runStorage();
        if(!walkRestored) {
            Resolver resolver;resolver.owner=n;std::string error;ActorBytes original;
            const double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
            require(capture_navi(*n,resolver,now,original,error)&&validate_navi(original,resolver,error)&&bind_navi(*n,original,resolver,now,error),"real Walk original restore: "+error);
            walkRestored=true;restoredNavi=n;restoredHealth=n->mHealth;
            return result; // Ordinary next tick uses only this real original payload.
        }
        require(n==restoredNavi&&n->mHealth==restoredHealth&&n->getCurrState()->getID()==NAVISTATE_Walk,"actual Walk next tick after restore");
        std::printf("MIDDAY_ACTOR_REAL_WALK_NEXT_TICK same_actor=1 fresh_process=0\n");
        run(*n);return result;
    }
};
}
int main(int argc,char** argv) {
    for(int i=1;i<argc;++i)if(std::strcmp(argv[i],"--strong-storage-only")==0){strongStorageOnly=true;for(int j=i;j+1<argc;++j)argv[j]=argv[j+1];--argc;argv[argc]=nullptr;--i;}
    SDL_setenv("PIKMIN_RANDOMIZER_TEST_BACKGROUND","1",1);SDL_setenv("SDL_AUDIODRIVER","dummy",1);SDL_SetMainReady();pc_gpu_preference_apply();pc_bbft_init(argc,argv);
    require(pc_randomizer_enabled(),"ordinary randomizer assets required");
    if(!pc_window_init("Midday actor component fixture",960,540))return 3;
    pc_settings_init();pc_window_set_display_mode(0);pc_window_set_window_size(960,540);pc_window_center();
    int width=0,height=0;SDL_GetWindowSize(SDL_GL_GetCurrentWindow(),&width,&height);require(width==960&&height==540,"960x540 baseline");
    pc_coop_set_pending(false);gsys->Initialise();pc_settings_p2d_init();nodeMgr=new NodeMgr();gsys->run(new TestApp());return 0;
}
