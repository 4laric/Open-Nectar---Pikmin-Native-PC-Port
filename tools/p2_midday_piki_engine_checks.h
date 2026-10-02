#pragma once
#include "Piki.h"
#include "pc_midday_actor_archive.h"
#include <cstring>
#include <cmath>
// Component roundtrip on an already initialized real engine Piki. The caller
// prevents engine ticks throughout. This is not a campaign resume qualification.
inline bool pc_midday_test_piki(Piki& piki,pc_midday::LogicalResolver& resolver,double now,std::string& error){
 using namespace pc_midday;
 ActorBytes original;
 if(!capture_piki(piki,resolver,now,original,error))return false;
 ActorFields fields;if(!decode_actor_fields(original,fields,error))return false;
 auto* state=piki.mCurrentState;auto* top=piki.mActiveAction;
 auto change=[&](const char* key,float delta){
  auto it=fields.find(key);if(it==fields.end()||it->second.category!=FieldCategory::Scalar||it->second.scalar!=ScalarKind::F32){error="missing real Piki float field";return false;}
  u32 bits=static_cast<u32>(it->second.bits);float value;std::memcpy(&value,&bits,4);value+=delta;
  if(!std::isfinite(value)){error="invalid real Piki float";return false;}std::memcpy(&bits,&value,4);it->second.bits=bits;return true;
 };
 if(!change("piki.runtime.mDeathTimer",0.125f)||!change("piki.runtime.upperAnimation.mAnimationCounter",0.25f))return false;
 ActorBytes changed;
 if(!encode_actor_fields(fields,changed,error)||!validate_piki(changed,resolver,error)||!bind_piki(piki,changed,resolver,now,error))return false;
 ActorBytes observed;
 const bool exact=capture_piki(piki,resolver,now,observed,error)&&observed==changed&&piki.mCurrentState==state&&piki.mActiveAction==top;
 std::string restoreError;
 if(!bind_piki(piki,original,resolver,now,restoreError)){error="restoring original real Piki payload: "+restoreError;return false;}
 ActorBytes restored;if(!capture_piki(piki,resolver,now,restored,restoreError)||restored!=original){error="original real Piki bytes differ after restore: "+restoreError;return false;}
 if(!exact){error="real Piki typed timer/frame roundtrip mismatch or identity changed";return false;}
 // Reject a forged live child before any binding writes, then prove no change.
 fields.clear();if(!decode_actor_fields(original,fields,error))return false;
 fields["piki.action.child"].bits=31;ActorBytes corrupt;
 if(!encode_actor_fields(fields,corrupt,error))return false;
 std::string refused;if(validate_piki(corrupt,resolver,refused)||bind_piki(piki,corrupt,resolver,now,refused)){error="invalid Piki child accepted";return false;}
 restored.clear();if(!capture_piki(piki,resolver,now,restored,error)||restored!=original){error="rejected Piki payload changed engine state";return false;}
 return true;
}
