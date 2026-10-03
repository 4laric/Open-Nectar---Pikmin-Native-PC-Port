#pragma once
#include "pc_p2_original_piki_native_services.h"
namespace p2original {namespace piki {
// Read-only borrows from the ONE process-retained Body factory. The Captain
// owns NativeCaptainReader/PlateSource; the Body factory constructs and retains
// the actual Plate storage around that source. No registry or setter supplies
// substitutes. A null result means unavailable and must refuse the consumer.
//
// Borrowing requires exact canonical LoadedScene + retained prepared Stage,
// creating thread/serial/selection and actual selected-bank ownership. Partial
// body creation can expose its genuinely constructed Plate/Services objects;
// this grants no 20-body admission, active command or source body lifetime.
// Borrowed objects are process-stable; their per-scene state remains retained
// through checked Body/Captain retirement before native resource/heap reuse.
// Callers requery on each scene-bound operation and never cache scene authority.
// Animator borrows require its genuine selected-bank preparation; partial
// construction does not expose a callable animator or grant body admission.
NativeAnimator* nativeAnimator(const captain::LoadedScene&,std::string&);
NativeEffects* nativeEffects(const captain::LoadedScene&,std::string&);
Plate* nativePlate(const captain::LoadedScene&,std::string&);
NativeServices* nativeServices(const captain::LoadedScene&,std::string&);
PhysicalSource* nativePhysicalSource(const captain::LoadedScene&,std::string&);
} }
namespace p2retail {class SceneContext;}
namespace p2original {namespace piki {
// Resource construction only, in the actual canonical Loading phase. All four
// producers MUST be process-stable genuine owners, not temporary adapters.
// Partial construction is process-retained for checked factory cleanup. This
// neither allocates twenty bodies nor implements Stage admission/retirement.
bool prepareNativeComposition(const p2retail::SceneContext&,NativeCaptainReader&,
 NativeEffects&,NativeTaskEnvironment&,PhysicalSource&,std::string&);
} }
