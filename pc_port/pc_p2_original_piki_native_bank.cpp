#include "pc_p2_original_piki_native_bank.h"
#include "pc_p2_original_selected_shape_stream.h"
#include "pc_p2_retail_starting_piki.h"
#include "pc_p2_retail_scene.h"
#include "pc_randomizer.h"
#include "netplay/pc_netplay_sha256.h"
#include "Shape.h"
#include "Texture.h"
#include "system.h"
#include "sysNew.h"
#include "gl/pc_gfx.h"
#include <memory>
#include <map>
#include <stdexcept>
namespace p2original { namespace piki {
namespace {
bool fail(std::string& error,const char* text){error=text;return false;}
struct Owner {
    std::map<std::string,std::string> inputs;
    std::string campaign,session;
    const p2retail::SceneContext* context=nullptr;
    System* system=nullptr;
    std::uint64_t serial=0,revision=0;
    std::map<std::string,Shape*> shapes;
    bool committed=false;
    PikiPcAllocationArena arena;
};

bool busy=false;
class Operation {
public:
    Operation(){if(!busy){busy=true;entered=true;}}
    ~Operation(){if(entered)busy=false;}
    bool valid()const{return entered;}
private:bool entered=false;
};
bool sameStage(const Owner& value) noexcept {
    const auto* context=pc_p2_retail_scene_prepared();
    return context&&context==value.context&&gsys&&gsys==value.system
        &&context->stage()&&context->map()&&context->nativeSerial()==value.serial
        &&context->selectionRevision()==value.revision;
}
bool current(const Owner& value) noexcept {
    return sameStage(value)&&pc_randomizer_original_session()
        &&pc_randomizer_original_selection_revision()==value.revision
        &&pc_randomizer_original_campaign()==value.campaign
        &&value.context->campaignSha256()==value.campaign
        &&value.context->sessionSha256()==value.session;
}
class SystemView {
public:
    explicit SystemView(System& system):s(system),shape(s.mCurrentShape),base1(s.mTextureBase1),base2(s.mTextureBase2),heap(s.mActiveHeapIdx){
        s.setHeap(SYSHEAP_App);s.setTextureBase("","");
    }
    ~SystemView(){s.mCurrentShape=shape;s.setTextureBase(base1,base2);s.setHeap(heap);}
private:System& s;Shape* shape;const char* base1;const char* base2;int heap;
};
bool noExternalResources(const Shape& shape){
    if(shape.mFallbackTexAttrCount||shape.mAttrListMatCount||shape.mTextureNameList
        ||shape.mLightGroup.mChild||shape.mRouteGroup.mChild||shape.mCollisionInfo.mChild)return false;
    for(int i=0;i<shape.mTexAttrCount;++i)
        if((shape.mTexAttrList[i].mTextureIndex&0x8000)||shape.mTexAttrList[i].mTextureName)return false;
    return true;
}
bool dispose(Owner& value,std::string& error){
    if(!sameStage(value)||gsys->mIsRendering||!value.arena.canReleaseStorage())
        return fail(error,"White Shape disposal requires its retained physical Stage and stopped rendering");
    // Remove only this actual allocation owner's display-list/vertex references.
    // Foreign Stage resources keep their cache and current array bindings.
    pc_gfx_forget_owned_native_storage([](const void* pointer,void* owner) {
        return static_cast<Owner*>(owner)->arena.owns(pointer);
    }, &value);
    auto* head=&gsys->mGfxobjInfo;
    for(auto* info=head->mNext;info!=head;){
        auto* next=info->mNext;
        if(value.arena.owns(info)){
            if(info->mAttached){info->detach();info->mAttached=false;}
            if(info->mId.mId=='_tex'){
                auto* texture=static_cast<TexobjInfo*>(info)->mTexture;
                if(texture&&texture->mTexObj)pc_gfx_release_texture(texture->mTexObj);
            }
            info->remove();
        }
        info=next;
    }
    if(!value.arena.releaseStorage())return fail(error,"White Shape storage release refused");
    value.shapes.clear();value.committed=false;
    return true;
}
}
struct NativeBodyBank::Impl { std::unique_ptr<Owner> owner; };
NativeBodyBank::NativeBodyBank():m(new Impl){}
NativeBodyBank::~NativeBodyBank()=default;
NativeBodyBank& NativeBodyBank::instance(){
 // Deliberately retained: only checked release destroys a live native graph.
 // Process exit reclaims its address space; a static destructor must not erase
 // this census before graphics/System teardown order has been established.
 static auto* bank=new NativeBodyBank();return *bank;
}
bool NativeBodyBank::prepare(const p2retail::SceneContext& context,std::string& error){
 Operation operation;if(!operation.valid())return fail(error,"reentrant source Body bank load");
 if(m->owner)return fail(error,"source Body bank retains prior graph");
 if(pc_p2_retail_scene_prepared()!=&context||!context.stage()||!context.map()
    ||!context.nativeSerial()||!context.selectionRevision()||!gsys||gsys->mIsRendering
    ||context.phase()!=p2retail::ScenePhase::Prepared)
  return fail(error,"source Body bank requires the actual prepared Stage boundary");
 p2retail::StartingPikiInputs selected;
 if(!p2retail::selectedStartingPiki(context,selected,error))return false;
 auto candidate=std::make_unique<Owner>();candidate->context=&context;candidate->system=gsys;
 candidate->serial=context.nativeSerial();candidate->revision=context.selectionRevision();
 candidate->campaign=context.campaignSha256();candidate->session=context.sessionSha256();
 // The selected parser verifies this full closure. Re-read through the actual
 // retained SDK and compare its exact digest before retaining native inputs.
 for(const auto& member:selected.bank){
  std::string bytes;if(!pc_randomizer_original_input(member.first,bytes,error)||bytes.empty()||bytes.size()>16*1024*1024)return false;
  unsigned char digest[32];pc_netplay_sha::sha256(bytes.data(),bytes.size(),digest);
  const char* hex="0123456789abcdef";std::string actual;actual.reserve(64);
  for(auto value:digest){actual+=hex[value>>4];actual+=hex[value&15];}
  if(actual!=member.second)return fail(error,"source Body bank retained role digest differs");
  candidate->inputs.emplace(member.first,std::move(bytes));
 }
 if(!piki::current(*candidate))return fail(error,"source Body bank changed selected Stage while reading");
 // Retain partial ownership before native constructors register graphics.
 m->owner=std::move(candidate);bool loaded=false;
 try{
  SystemView system(*gsys);
  for(const auto& file:m->owner->inputs){
   if(file.first.size()<4||file.first.compare(file.first.size()-4,4,".mod"))continue;
   Shape* shape=nullptr;
   {PikiPcAllocationCapture capture(m->owner->arena);
    if(!capture.valid())throw std::runtime_error("source Body bank native allocation capture refused");
    SelectedShapeStream stream(file.second,file.first.c_str());
    shape=new Shape();shape->mName=StdSystem::stringDup(file.first.c_str());gsys->mCurrentShape=shape;
    shape->read(stream);
    if(!noExternalResources(*shape))throw std::runtime_error("source Body Shape requests foreign resource");
    shape->resolveTextureNames();shape->initialise();shape->initIni(false);shape->optimize();
   }
   m->owner->shapes.emplace(file.first,shape);
  }
  loaded=m->owner->shapes.size()==171&&piki::current(*m->owner);
  if(!loaded)error="source Body bank native model closure/Stage changed";
 }catch(const std::exception& e){error=e.what();}catch(...){error="source Body bank native loading threw";}
 if(!loaded){std::string cleanup;if(dispose(*m->owner,cleanup))m->owner.reset();else error+="; retained cleanup: "+cleanup;return false;}
 m->owner->committed=true;return true;
}
bool NativeBodyBank::current()const noexcept{return m->owner&&m->owner->committed&&piki::current(*m->owner);}
Shape* NativeBodyBank::shape(const std::string& role)const noexcept{
 if(busy||!current())return nullptr;auto found=m->owner->shapes.find(role);return found==m->owner->shapes.end()?nullptr:found->second;
}
const std::string* NativeBodyBank::bytes(const std::string& role)const noexcept{
 if(busy||!current())return nullptr;auto found=m->owner->inputs.find(role);return found==m->owner->inputs.end()?nullptr:&found->second;
}
bool NativeBodyBank::owned()const noexcept{return bool(m->owner);}
bool NativeBodyBank::canRelease(std::string& error)const{
 if(busy)return fail(error,"source Body bank operation in progress");
 return !m->owner||(sameStage(*m->owner)&&!gsys->mIsRendering&&m->owner->arena.canReleaseStorage())||fail(error,"source Body bank lost actual native cleanup boundary");
}
bool NativeBodyBank::release(std::string& error){
 Operation operation;if(!operation.valid())return fail(error,"reentrant source Body bank release");
 if(!m->owner)return true;if(!dispose(*m->owner,error))return false;m->owner.reset();return true;
}
bool NativeBodyBank::retired(std::string& error)const{return !m->owner||fail(error,"source Body bank graph retained");}
} }

