#include "pc_p2_original_white_shapes.h"
#include "pc_p2_original_selected_shape_stream.h"
#include "pc_p2_original_white_selected.h"
#include "pc_p2_retail_scene.h"
#include "Shape.h"
#include "Texture.h"
#include "Stream.h"
#include "system.h"
#include "sysNew.h"
#include "gl/pc_gfx.h"
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <map>

namespace p2original {
namespace {
bool fail(std::string& error,const char* text){error=text;return false;}
struct Owner {
    WhiteSelectedKit kit;
    const p2retail::SceneContext* context=nullptr;
    System* system=nullptr;
    std::uint64_t serial=0,revision=0;
    std::map<std::string,Shape*> shapes;
    bool committed=false;
    PikiPcAllocationArena arena;
};
std::unique_ptr<Owner> owner;
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
    return sameStage(value)&&whiteSelectedKitCurrent(value.kit)
        &&value.context->campaignSha256()==value.kit.campaign()
        &&value.context->sessionSha256()==value.kit.session();
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
bool prepareWhiteShapes(std::string& error){
    Operation operation;if(!operation.valid())return fail(error,"reentrant White Shape resource operation refused");
    if(owner)return fail(error,"White Shape owner must be released before another load");
    const auto* context=pc_p2_retail_scene_prepared();
    if(!context||!context->stage()||!context->map()||!context->nativeSerial()||!context->selectionRevision()
        ||!gsys||gsys->mIsRendering||context->phase()==p2retail::ScenePhase::Releasing)
        return fail(error,"White Shapes require an actual selected owned Stage loading boundary");
    auto candidate=std::make_unique<Owner>();
    if(!readWhiteSelectedKit(candidate->kit,error))return false;
    candidate->context=context;candidate->system=gsys;candidate->serial=context->nativeSerial();candidate->revision=context->selectionRevision();
    if(!current(*candidate))return fail(error,"White kit differs from actual Stage selection");
    // Publish cleanup ownership before the first native allocation/registration.
    // Physical consumers still cannot obtain a shape until complete success.
    owner=std::move(candidate);
    bool loaded=false;
    try{
        SystemView system(*gsys);
        for(const auto& file:owner->kit.files()){
            if(file.first.size()<4||file.first.compare(file.first.size()-4,4,".mod"))continue;
            Shape* shape=nullptr;
            {
                PikiPcAllocationCapture capture(owner->arena);
                if(!capture.valid())throw std::runtime_error("White Shape allocation capture refused");
                SelectedShapeStream stream(file.second,file.first.c_str());
                shape=new Shape();shape->mName=StdSystem::stringDup(file.first.c_str());gsys->mCurrentShape=shape;
                shape->read(stream);
                if(!noExternalResources(*shape))throw std::runtime_error("White Shape requests unselected external resources");
                shape->resolveTextureNames();shape->initialise();shape->initIni(false);shape->optimize();
            }
            owner->shapes.emplace(file.first,shape);
        }
        loaded=owner->shapes.size()==39&&current(*owner);
        if(!loaded)error="White Shape count/selected Stage changed during native load";
    }catch(const std::exception& exception){error=exception.what();}
    catch(...){error="White Shape native loading threw";}
    if(!loaded){std::string cleanup;if(dispose(*owner,cleanup))owner.reset();else error+="; retained cleanup owner: "+cleanup;return false;}
    owner->committed=true;return true;
}
Shape* whiteShape(const std::string& role) noexcept {
    if(busy||!owner||!owner->committed||!current(*owner))return nullptr;
    const auto found=owner->shapes.find(role);return found==owner->shapes.end()?nullptr:found->second;
}
bool whiteShapesOwned() noexcept {return bool(owner);}
bool whiteShapesCanRelease(std::string& error){
    if(busy)return fail(error,"White Shape load/disposal is in progress");
    if(!owner)return true;
    return (sameStage(*owner)&&!gsys->mIsRendering&&owner->arena.canReleaseStorage())
        ||fail(error,"White Shapes lost their retained native Stage cleanup boundary");
}
bool releaseWhiteShapes(std::string& error){
    Operation operation;if(!operation.valid())return fail(error,"reentrant White Shape resource release refused");
    if(!owner)return true;
    if(!dispose(*owner,error))return false;
    owner.reset();return true;
}
bool whiteShapesRetired(std::string& error){return !owner||fail(error,"White native Shape graph is still owned");}
}
