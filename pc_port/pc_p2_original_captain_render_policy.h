#pragma once
struct Graphics;
struct GameCoreSection;
namespace p2original { namespace captain {
// Only the native section draw boundary may establish a viewport number.
// Camera pointers are transform inputs, not the identity of a retail view.
class NativeViewScope {
 friend struct ::GameCoreSection;
 const Graphics* graphics;unsigned slot;NativeViewScope* previous;
 inline static thread_local NativeViewScope* active=nullptr;
 NativeViewScope(const Graphics& gfx,unsigned view):graphics(&gfx),slot(view),previous(active){active=this;}
 NativeViewScope(const NativeViewScope&)=delete;
 NativeViewScope& operator=(const NativeViewScope&)=delete;
public:
 ~NativeViewScope(){active=previous;}
 static bool current(const Graphics& gfx,unsigned& out){if(!active||active->graphics!=&gfx||active->slot>=2)return false;out=active->slot;return true;}
};
// The strict Captain MOD profile admits unscaled flattened materials. Inspect
// the owned native objects too; an ambient Graphics scale is a previous draw.
template<class Shape> bool unscaledMaterials(const Shape& shape){
 if(shape.mMaterialCount<1||shape.mMaterialCount>128||!shape.mMaterialList)return false;
 for(int i=0;i<shape.mMaterialCount;++i)if(shape.mMaterialList[i].mTextureInfo.mUseScale)return false;
 return true;
}
// DGX useMaterial can reuse an already-selected material. Clear prior external
// scale state explicitly so both its cached and newly-selected paths agree.
template<class Graphics> void selectUnscaledMaterials(Graphics& gfx){gfx.mCustomScale=nullptr;}
} }
