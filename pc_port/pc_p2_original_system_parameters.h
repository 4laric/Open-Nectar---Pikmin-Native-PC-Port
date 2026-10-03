#pragma once
#include <cstdint>
#include <string>
namespace p2original {
struct AIConstantsParameters {
 float gravity=0;
 // Actual AIConstants ctor assignments. Gravity has no ctor assignment.
 std::int32_t dopeCount=2,debt=10000;
 float cameraAngle=180;
};
struct TimeParameters {
 // Every field is REQUIRED from selected bytes; these zeros are output
 // sentinels, never TimeMgr constructor/resource fallback defaults.
 float dayStart=0,dayEnd=0,dayLengthSeconds=0;
 float morningStart=0,midMorning=0,morningEnd=0;
 float eveningStart=0,midEveningStart=0,midEveningEnd=0,eveningEnd=0;
 float sundownAlert=0,countdown=0;
};
// Strict, bounded raw retail TEXT readers only. No file loading, selected
// authority, native lifetime, TimeMgr creation or GameSystem readiness grant.
// Comments/# and brace-as-delimiter semantics follow actual Stream text.
// Unlike permissive retail readers, ambiguous duplicates, unknown fields,
// partial numeric tokens, unbalanced scopes/truncation and unsafe values refuse.
// AI requires gravity; optional fields retain ACTUAL ctor assignments. Time
// requires all twelve float(size4) IDs and _eof; no omitted-field defaults.
// Failed outputs stay unchanged. Numeric conversion uses classic decimal
// syntax, independent of host locale; no scanf prefix acceptance or NaN/Inf.
bool parseAIConstantsParameters(const std::string&,AIConstantsParameters&,std::string&);
bool parseTimeParameters(const std::string&,TimeParameters&,std::string&);
}
