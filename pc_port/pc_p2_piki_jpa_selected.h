#pragma once
#include "pc_p2_piki_jpa_bank.h"
namespace p2retail {class SceneContext;}
namespace p2original {namespace captain {class LoadedScene;} namespace pikiJPA {
// Selected resource read only. Does not install emitters, create bodies or grant
// Scene/World activity. All outputs remain unchanged after a refused read.
bool selectedBank(const p2retail::SceneContext&,Bank&,std::string& retainedProvenance,std::string&);
// Full versioned actual SceneIdentity + selection revision; checked SHA fields
// use lossless unpadded base64url, visit/serial/revision retain exact values.
// Immutable provenance is descriptive; Bank validates all16 fixed source hashes.
// This is not a packet hash
// relabelled as a session. Too-large stamps refuse without truncation.
bool selectedIdentity(const p2retail::SceneContext&,SelectedIdentity&,std::string&);
// Actual canonical descriptor/SceneContext checks precede retained dereference.
// This is resource identity, not an Active predicate or renderer scope grant.
bool currentIdentity(const p2retail::SceneContext&,const captain::LoadedScene&,
                     const SelectedIdentity&,std::string&);
} }
