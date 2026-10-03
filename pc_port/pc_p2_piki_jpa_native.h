#pragma once
#include "pc_p2_piki_halo.h"
#include "pc_p2_source_body.h"
#include <memory>
class Graphics;
class Matrix4f;
namespace p2original { namespace pikiJPA {
// One actual selected scene owner supplies authoritative source/body lifetime
// resolution and source ParticleMgr cullByResFlg. No fallback/default context.
// selectedCurrent checks the complete authoritative session stamp, not only a
// session identifier or an unverified hash. position verifies immutable ancestry
// and current native lifetime/species; mutable recruitment bits are not ancestry.
class Scene {
public:
 virtual ~Scene()=default;
 virtual bool selectedCurrent(const SelectedIdentity&,std::string&)const=0;
 virtual bool active()const noexcept=0;
 virtual bool position(const PcP2SourceBody& retained,Position&,std::string&)const=0;
 virtual bool clipped(Position,unsigned actualJPAId,bool& result,std::string&)const=0;
 // Mandatory actual render-owner scope. begin refuses atomically before GX
 // mutations if a full state capture/phase barrier cannot be established.
 // end restores complete custom TEV/alpha/texgen/texture/matrix/vertex state;
 // failure retains its cleanup proof and must stop the caller's draw sequence.
 // No default success implementation and no ambient next-draw assumption.
 virtual bool beginHaloDraw(Graphics&,std::string&)=0;
 virtual bool endHaloDraw(Graphics&,std::string&)=0;
};
class NativeEffects {
public:
 explicit NativeEffects(Scene& actualScene);
 ~NativeEffects();
 bool prepare(const Bank&,const std::array<std::uint32_t,6>& actualEmitterSeeds,std::size_t capacity,std::string&);
 bool sharedIdleHalo(const PcP2SourceBody&,unsigned species,unsigned actualJPAId,std::string&);
 bool canRemoveIdleHalo(const PcP2SourceBody&,std::string&)const;
 bool removeIdleHalo(const PcP2SourceBody&,std::string&);
 // Called once for each actual selected JPA source frame, only while Active.
 bool sourceFrame(std::string&);
 // Requires the mandatory actual Scene begin/end boundary. Native restores
 // exact Z/blend/cull and Graphics caches before invoking end; the actual
 // renderer owner restores the remaining full state/phase scope.
 bool draw(Graphics&,std::string&);
 // Explicit prerequisites; refused operations create no owner or fake handle.
 bool sharedNageKira(const PcP2SourceBody&,std::string&);
 bool perbodyNageBlur(const PcP2SourceBody&,unsigned species,Matrix4f& actualBaseMatrix,std::string&);
 bool voice(const PcP2SourceBody&,unsigned actualPSMSoundId,unsigned priority,std::string&);
 std::size_t owners()const;
private:
 struct Impl;std::unique_ptr<Impl> m;
};
} }
