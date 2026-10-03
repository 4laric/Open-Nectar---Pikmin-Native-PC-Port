#pragma once
namespace p2original { namespace captain {
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
