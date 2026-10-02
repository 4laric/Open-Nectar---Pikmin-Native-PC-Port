#if defined(PIKI_PC_PORT)
#include "pc_midday_collision.h"
#include "Collision.h"
using namespace pc_midday;
struct PcMiddayCollisionAccess {
 static bool storage(CollInfo&o,CollPart*p,u32*ids,u16 n){if(!n||n>1024||!p||!ids||o.mMaxParts!=n)return false;o.mCollParts=p;o.mPartIDs=ids;for(u16 i=0;i<n;++i)p[i].mParentInfo=&o;return true;}
 static bool fields(CollInfo&o,ActorArchive&a){
 CollInfo* identity=&o;if(!a.ref("identity",RefKind::CollInfo,identity)||identity!=&o)return a.fail("collision identity binding mismatch");
 u16 count=a.mode()==Mode::Capture?o.mPartsCount:0,cap=a.mode()==Mode::Capture?o.mMaxParts:0;
 if(!a.scalar("count",ScalarKind::U16,&count)||!a.scalar("capacity",ScalarKind::U16,&cap)||cap>1024||count>cap||cap!=o.mMaxParts||(count&&(!o.mCollParts||!o.mPartIDs)))return a.fail("collision factory storage mismatch");
 if(!a.field("defaultStorage",o.mUseDefaultMaxParts)||!a.ref("shape",RefKind::Shape,o.mShape))return false;
 for(u16 i=0;i<count;++i){PrefixArchive p(a,("part."+std::to_string(i)).c_str());auto& c=o.mCollParts[i];
 if(!p.field("id",o.mPartIDs[i])||!p.field("radius",c.mRadius)||!p.field("center",c.mCentre)||!p.field("update",c.mIsUpdateActive)||!p.field("stick",c.mIsStickEnabled)||!p.field("next",c.mNextIndex)||!p.field("child",c.mFirstChildIndex)||!p.field("type",c.mPartType)||!p.ref("descriptor",RefKind::ObjCollInfo,c.mCollInfo)||!p.ref("parent",RefKind::CollInfo,c.mParentInfo)||!p.ref("updater",RefKind::CollPartUpdater,c.mPartUpdater))return false;
 for(int r=0;r<4;++r)for(int col=0;col<4;++col)if(!p.field(("matrix."+std::to_string(r)+"."+std::to_string(col)).c_str(),c.mJointMatrix.mMtx[r][col]))return false;
 if(a.mode()!=Mode::Capture&&c.mParentInfo!=&o)return a.fail("collision parent binding mismatch");
 }if(a.mode()==Mode::Apply)o.mPartsCount=count;return true;
 }
};
namespace pc_midday {
bool collision_bind_storage(CollInfo&o,CollPart*p,u32*i,u16 n){return PcMiddayCollisionAccess::storage(o,p,i,n);}
bool collision_fields(CollInfo&o,ActorArchive&a){return PcMiddayCollisionAccess::fields(o,a);}
}
#endif
