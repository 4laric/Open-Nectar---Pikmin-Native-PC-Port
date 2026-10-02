#include "pc_midday_creature.h"
namespace pc_midday {
namespace {
void scalar(std::vector<FieldSchema>& s,const std::string& k,ScalarKind t){s.push_back(FieldSchema::value(("creature."+k).c_str(),t));}
void vector(std::vector<FieldSchema>& s,const std::string& k){for(auto x:{".x",".y",".z"})scalar(s,k+x,ScalarKind::F32);}
void ref(std::vector<FieldSchema>& s,const std::string& k,RefKind t,const char* type,bool nullable=true,ReferenceOwnership owner=ReferenceOwnership::AnyLive,ReferenceStrength strength=ReferenceStrength::Weak){s.push_back(FieldSchema::ref(("creature."+k).c_str(),t,nullable,type,owner,"",strength));}
bool bad(std::string& e,const char* m){if(e.empty())e=m;return false;}
}
bool creature_schema(const ActorFields& f,std::vector<FieldSchema>& out,std::string& e){
 std::vector<FieldSchema>s;
 scalar(s,"objectType",ScalarKind::S32);
 scalar(s,"referenceCount",ScalarKind::S32);
 int references=0;if(f.count("creature.referenceCount")&&(!actor_i32(f,"creature.referenceCount",references,e)||references<0))return bad(e,"invalid native Creature reference count");
 scalar(s,"mRebirthDay",ScalarKind::S32);
 scalar(s,"mHealth",ScalarKind::F32);
 scalar(s,"mMaxHealth",ScalarKind::F32);
 scalar(s,"mWaterFxTimer",ScalarKind::U8);
 scalar(s,"mFaceDirection",ScalarKind::F32);
 scalar(s,"mCreatureFlags",ScalarKind::U32);
 scalar(s,"mGroundOffset",ScalarKind::F32);
 scalar(s,"mRopePosRatio",ScalarKind::F32);
 scalar(s,"mPelletStickSlot",ScalarKind::S32);
 scalar(s,"mHasCollChangedVelocity",ScalarKind::U32);
 scalar(s,"mCollisionOccurred",ScalarKind::U32);
 scalar(s,"mSize",ScalarKind::F32);
 scalar(s,"mCollisionRadius",ScalarKind::F32);
 scalar(s,"mIsFrozen",ScalarKind::U32);
 scalar(s,"mIsBeingDamaged",ScalarKind::Bool);
 scalar(s,"_30",ScalarKind::U8);
 scalar(s,"_298",ScalarKind::U32);
 scalar(s,"mGrid.mGridPositionX",ScalarKind::S16);
 scalar(s,"mGrid.mGridPositionY",ScalarKind::S16);
 scalar(s,"mGrid.mGridPositionZ",ScalarKind::S16);
 scalar(s,"mGrid.mWidth",ScalarKind::S16);
 scalar(s,"mGrid.mHeight",ScalarKind::S16);
 scalar(s,"mGrid.mNeighbourSize",ScalarKind::U16);
 vector(s,"mFixedPosition");
 vector(s,"mVelocity");
 vector(s,"mSRT.s");
 vector(s,"mSRT.r");
 vector(s,"mSRT.t");
 vector(s,"mTargetVelocity");
 vector(s,"_B0");
 vector(s,"mVolatileVelocity");
 vector(s,"mPrevAngularVelocity");
 vector(s,"mAttachPosition");
 vector(s,"mLastPosition");
 vector(s,"mCollAttachment.mCollSpacePosition");
 vector(s,"mPlatformAdjustDelta");
 ref(s,"mFormPoint",RefKind::FormPoint,"FormPoint");
 ref(s,"mGenerator",RefKind::Generator,"Generator");
 ref(s,"mRopeListHead",RefKind::Creature,"Creature");
 ref(s,"mRope",RefKind::Creature,"Creature");
 ref(s,"mNextRopeHolder",RefKind::Creature,"Creature");
 ref(s,"mPrevRopeHolder",RefKind::Creature,"Creature");
 ref(s,"mStickListHead",RefKind::Creature,"Creature");
 ref(s,"mStickTarget",RefKind::Creature,"Creature");
 ref(s,"mStickPart",RefKind::CollPart,"CollPart");
 ref(s,"mNextSticker",RefKind::Creature,"Creature");
 ref(s,"mPrevSticker",RefKind::Creature,"Creature");
 ref(s,"mFormMgr",RefKind::FormationMgr,"FormationMgr");
 ref(s,"mCollInfo",RefKind::CollInfo,"CollInfo");
 ref(s,"mProps",RefKind::CreatureProp,"CreatureProp",false);
 ref(s,"mCollPlatform",RefKind::DynCollObject,"DynCollObject");
 ref(s,"mCollNormal",RefKind::Vector3,"Vector3f");
 ref(s,"mPikiPlatformTriangle",RefKind::CollTriInfo,"CollTriInfo");
 ref(s,"mGroundTriangle",RefKind::CollTriInfo,"CollTriInfo");
 ref(s,"mPreviousTriangle",RefKind::CollTriInfo,"CollTriInfo");
 ref(s,"mHoldingCreature.mPtr",RefKind::Creature,"Creature",true,ReferenceOwnership::AnyLive,ReferenceStrength::StrongCreature);
 ref(s,"mGrabbedCreature.mPtr",RefKind::Creature,"Creature",true,ReferenceOwnership::AnyLive,ReferenceStrength::StrongCreature);
 for(auto q:{"rotation","preGrab","grabDelta"}){vector(s,std::string(q)+".v");scalar(s,std::string(q)+".s",ScalarKind::F32);}
 for(auto m:{"constraint","world"})for(int i=0;i<4;++i)for(int j=0;j<4;++j)scalar(s,std::string(m)+"."+std::to_string(i)+"."+std::to_string(j),ScalarKind::F32);
 for(auto c:{"searchUpdate","optUpdate"}){ref(s,std::string(c)+".manager",RefKind::UpdateMgr,"UpdateMgr");scalar(s,std::string(c)+".slot",ScalarKind::S32);scalar(s,std::string(c)+".piki",ScalarKind::Bool);}
 scalar(s,"airResistance",ScalarKind::F32);scalar(s,"grabProgress",ScalarKind::F32);ref(s,"collisionModel",RefKind::Shape,"Shape");
 scalar(s,"search.capacity",ScalarKind::S16);scalar(s,"search.count",ScalarKind::S16);scalar(s,"search.last",ScalarKind::S32);scalar(s,"search.maxDistance",ScalarKind::F32);
 auto signed16=[&](const char* k,int& n){auto it=f.find(k);if(it==f.end()||it->second.category!=FieldCategory::Scalar||it->second.scalar!=ScalarKind::S16||it->second.bits>65535)return false;u16 v=u16(it->second.bits);n=v>=32768?int(v)-65536:int(v);return true;};
 int capacity=0,count=0,last=0,type=0;
 if(!signed16("creature.search.capacity",capacity)||!signed16("creature.search.count",count)||capacity<0||capacity>4096||count<0||count>capacity||!actor_i32(f,"creature.search.last",last,e)||last< -1||last>=capacity||!actor_i32(f,"creature.objectType",type,e)||type<0)return bad(e,"invalid Creature/search discriminator");
 for(int i=0;i<capacity;++i){auto p=std::string("search.")+std::to_string(i)+".";ref(s,p+"target",RefKind::Creature,"Creature",i>=count,ReferenceOwnership::AnyLive,ReferenceStrength::StrongCreature);scalar(s,p+"distance",ScalarKind::F32);scalar(s,p+"iteration",ScalarKind::S32);}
 // Local relation checks precede resolver/scene allocation; reciprocal graph
 // membership and concrete subobject types are the scene resolver's contract.
 auto absent=[&](const char* k){auto i=f.find(std::string("creature.")+k);return i==f.end()||(i->second.target.owner==0&&i->second.target.resource==0&&i->second.target.slot==0);};
 if(!absent("mStickPart")&&absent("mStickTarget"))return bad(e,"stick part lacks target");
 if((!absent("mNextSticker")||!absent("mPrevSticker"))&&absent("mStickTarget"))return bad(e,"sticker links lack target");
 if((!absent("mNextRopeHolder")||!absent("mPrevRopeHolder"))&&absent("mRope"))return bad(e,"rope links lack rope");
 out.swap(s);return true;
}
bool validate_creature(const ActorBytes& b,const LogicalResolver& r,int expectedObjectType,std::string& e){ActorFields f;std::vector<FieldSchema>s;int type=-1;return decode_actor_fields(b,f,e)&&actor_i32(f,"creature.objectType",type,e)&&(type==expectedObjectType||bad(e,"Creature factory type mismatch"))&&creature_schema(f,s,e)&&validate_actor_fields(f,s,r,e);}
}
