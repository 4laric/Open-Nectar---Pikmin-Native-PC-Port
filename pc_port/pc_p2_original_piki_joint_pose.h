#pragma once
#include "Vector.h"
#include <string>
namespace p2original {namespace piki {
// Pure bounded source decoder. Authentication/retention belongs to NativeAnimator.
// Full ANF1 getTransform uses int(frame+0.5), per-channel terminal clamping;
// there is no interpolation. Requires a genuine unparented BMD joint0 and
// complete animated root channels; missing data never supplies a zero root.
bool sourceRootTranslation(const std::string& model,const std::string& bca,
 float sourceFrame,Vector3f&,std::string&);
} }
