#pragma once
#include "Piki.h"
#include "PikiAI.h"
#include "pc_midday_piki_storage.h"
#include "pc_midday_actor_archive.h"
#include <cstring>
#include <cmath>
// Component roundtrip on an already initialized real engine Piki. The caller
// prevents engine ticks throughout. This is not a campaign resume qualification.
inline bool pc_midday_test_piki(Piki& piki,pc_midday::LogicalResolver& resolver,double now,std::string& error,bool storageOnly=false){
 using namespace pc_midday;
 ActorBytes original;
 if(!capture_piki(piki,resolver,now,original,error))return false;
 ActorFields fields;if(!decode_actor_fields(original,fields,error))return false;
 struct Census:StrongStorageVisitor{std::map<std::string,const void*> keys;std::set<const void*> slots;bool visit(const char* key,const StrongStorageSlot& slot,std::string& e)override{if(!slot.storage||!slot.owner||!slot.ownerType||!slot.member||!keys.emplace(key,slot.storage).second||!slots.insert(slot.storage).second){e="duplicate/missing real strong storage";return false;}return true;}} census,changedSelection;
 if(!visit_piki_strong_storage(piki,fields,census,error))return false;std::vector<FieldSchema> storageSchema;if(!piki_schema(fields,storageSchema,error))return false;size_t strongCount=0;for(auto& d:storageSchema)if(d.strength==ReferenceStrength::StrongCreature){++strongCount;if(!census.keys.count(d.key)){error="real strong schema/storage key missing";return false;}}if(strongCount!=census.keys.size()){error="extra real strong storage slot";return false;}
 // Simulate a freshly allocated object's unrelated selection without callbacks.
 auto savedChild=piki.mActiveAction->mCurrActionIdx;piki.mActiveAction->mCurrActionIdx=savedChild==-1?0:-1;
 bool freshMapping=visit_piki_strong_storage(piki,fields,changedSelection,error);piki.mActiveAction->mCurrActionIdx=savedChild;
 if(!freshMapping||census.keys!=changedSelection.keys){error="strong storage mapping used live action selection";return false;}
 if(storageOnly){ActorBytes unchanged;if(!capture_piki(piki,resolver,now,unchanged,error)||unchanged!=original){error="storage observation changed Piki payload";return false;}return true;}
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
 // Family binding must not install the observed historical count. The complete
 // scene backend separately rejects a forged census before publication.
 fields.clear();if(!decode_actor_fields(original,fields,error))return false;
 const int nativeCount=piki.mCount;
 fields["creature.referenceCount"].bits=nativeCount?0:1;
 ActorBytes observedCount;
 if(!encode_actor_fields(fields,observedCount,error)||!bind_piki(piki,observedCount,resolver,now,error)||piki.mCount!=nativeCount){error="family binding wrote observed reference count";return false;}
 restored.clear();if(!capture_piki(piki,resolver,now,restored,error)||restored!=original){error="count observation changed actual actor state";return false;}
 // Reject a forged live child before any binding writes, then prove no change.
 fields.clear();if(!decode_actor_fields(original,fields,error))return false;
 fields["piki.action.child"].bits=31;ActorBytes corrupt;
 if(!encode_actor_fields(fields,corrupt,error))return false;
 std::string refused;if(validate_piki(corrupt,resolver,refused)||bind_piki(piki,corrupt,resolver,now,refused)){error="invalid Piki child accepted";return false;}
 restored.clear();if(!capture_piki(piki,resolver,now,restored,error)||restored!=original){error="rejected Piki payload changed engine state";return false;}
 return true;
}
