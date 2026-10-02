#pragma once
#include "pc_midday_actor_archive.h"
#include "pc_midday_restore.h"
#include <functional>
class AnimContext;
class AnimData;
namespace pc_midday {
// Each canonical shared context is encoded once. Consumers retain its identity;
// content identity does not make this mutable context immutable.
struct AnimationContextRecord {
 LogicalRef data;
 float frame=0,speed=30;
};
// Mandatory content/factory metadata check, safe before scene allocation. Must
// verify exact AnimData descriptor and legal frame interval, including null-data
// constructor contexts. No live pointer dereference or mutation is needed.
using AnimationContentCheck=std::function<bool(const LogicalRef&,float,std::string&)>;
// Built from the actual installed, content-bound AnimData allocations before
// scene begin. These immutable descriptor facts are checked alongside Binding's
// content digest; wire input cannot choose a different clip's frame bound.
class AnimationContentIndex {
 std::map<u64,u32> frames_;
public:
 bool declare(const LogicalRef&,u32 frames,std::string&);
 bool validate(const LogicalRef&,float frame,std::string&)const;
};
bool declareAnimationData(AnimationContentIndex&,const LogicalRef&,const AnimData&,std::string&);
std::vector<FieldSchema> animationContextSchema();
bool encodeAnimationContext(const AnimationContextRecord&,const LogicalResolver&,const AnimationContentCheck&,ActorBytes&,std::string&);
bool decodeAnimationContext(const ActorBytes&,const LogicalResolver&,const AnimationContentCheck&,AnimationContextRecord&,std::string&);
bool captureAnimationContext(const AnimContext&,LogicalResolver&,const AnimationContentCheck&,ActorBytes&,std::string&);
// Caller supplies a freshly allocated, unpublished canonical resource. Decode,
// typed content resolution and all validation precede the three named writes.
// Publishing the resource with the world remains the backend's final transaction.
bool stageAnimationContext(AnimContext&,const ActorBytes&,LogicalResolver&,const AnimationContentCheck&,const RestoreGate&,std::string&);
}
