#pragma once
#include "pc_p2_original_piki_runtime.h"

namespace p2original { namespace piki {
class PlateSource;

// The actual SourceCaptain owner implements this narrow read-only boundary.
// The singular Body Services composer borrows it; this is not another Services
// singleton or an actor/world installation API. The producer must remain alive
// until every body/runtime/plate consumer has completed checked retirement.
class NativeCaptainReader {
public:
 virtual ~NativeCaptainReader()=default;
 // Pointer-identical to the canonical retained LoadedScene. Loading is valid
 // for initialized-body bootstrap; actions require the genuine Active World.
 virtual const captain::LoadedScene& scene()const=0;
 // Actual selected source Navi parameter bytes retained by the Captain bank.
 // No P1 parameter object, caller-supplied text or source-default substitution.
 virtual const std::string& naviParameterBytes()const=0;
 // Read literal source fields, including the already sampled rhnd pose and
 // source ThrowWait/Throw/command/C-stick/Formation/follow state. The producer
 // validates exact captain roster/lifetime and canonical scene before/after
 // observation. Refusal leaves output unchanged; host P1 fields are not facts.
 virtual bool frame(const Navi*,CaptainFrame&,std::string&)const=0;
 // Actual source updateCPlate/makeCStick decisions and instantiated source
 // CPlate parameters. The composer owns the Plate storage, not this producer.
 virtual PlateSource& plateSource()const=0;
};
} }
