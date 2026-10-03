#pragma once
#include "pc_p2_original_system_parameters.h"
namespace p2retail {class SceneContext;}
namespace p2original {
struct SelectedSystemParameters {
 AIConstantsParameters ai;
 TimeParameters time;
};
// Read both exact selected SYSTEM roles through original_input authentication,
// then strict retail parsing. Requires the actual current creating-thread Stage
// throughout each read/parse. Failed outputs stay unchanged. No file fallback,
// optional-role default, caller authority callback or gameplay grant exists.
// This pair is resource data, not a cached current-scene proof. Its lifecycle
// consumer must retain/recheck its genuine Stage before using it later.
bool readSelectedSystemParameters(const p2retail::SceneContext&,
 SelectedSystemParameters&,std::string& error);
}
