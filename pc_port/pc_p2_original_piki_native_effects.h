#pragma once
#include "pc_p2_original_piki_native_services.h"
#include "pc_p2_piki_jpa_manager.h"
class Graphics;
namespace p2retail {class SceneContext;}
namespace p2original {namespace piki {
// One process-stable concrete owner. Construction admits resources and owns
// the actual manager, never World activation or a replacement Body lifetime.
NativeEffects* nativeEffects(const captain::LoadedScene&,std::string&);
bool prepareNativeEffects(const p2retail::SceneContext&,std::string&);
bool canRetireNativeEffects(const p2retail::SceneContext&,std::string&);
bool retireNativeEffects(const p2retail::SceneContext&,std::string&);
// Source placement creates its emitters through this owner BEFORE startShared.
// Unsupported selected effects refuse before RNG; callers must propagate that
// refusal. Nonshared emitter clocks are currently unsupported, including extra
// halo emitters; this entry refuses rather than publishing descriptor-only FX.
// No seed setter, caller seed array or reconstructed admission census.
bool createNativeSceneEmitter(const captain::LoadedScene&,unsigned,
 pikiJPA::EmitterHandle&,std::string&);
bool removeNativeSceneEmitter(const captain::LoadedScene&,
 pikiJPA::EmitterHandle&,std::string&);
bool startNativeSharedEffects(const captain::LoadedScene&,std::string&);
bool nativeEffectsSourceFrame(const captain::LoadedScene&,std::string&);
bool drawNativeEffects(const captain::LoadedScene&,Graphics&,std::string&);
struct NativeEffectFrontier {std::uint32_t rng=0;std::uint64_t admissions=0;unsigned live=0;};
bool nativeEffectFrontier(const captain::LoadedScene&,NativeEffectFrontier&,std::string&);
}}
