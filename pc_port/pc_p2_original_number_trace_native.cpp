#include "pc_p2_original_number_trace_native.h"
#include "MapMgr.h"
#include "Pellet.h"
#include "DynColl.h"
#include "Shape.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
namespace {
using namespace p2originalnumber;
using rigid::Vec3;using rigid::Trace;
Vec3 vec(const Vector3f& v){return {v.x,v.y,v.z};}
Vector3f native(Vec3 v){return Vector3f(v.x,v.y,v.z);}
bool fail(std::string& e,const char* text){e=text;return false;}
bool valid(const Trace& t){return std::isfinite(t.position.x)&&std::isfinite(t.position.y)&&std::isfinite(t.position.z)&&
 std::isfinite(t.velocity.x)&&std::isfinite(t.velocity.y)&&std::isfinite(t.velocity.z)&&std::isfinite(t.radius)&&t.radius>0&&
 std::isfinite(t.restitution)&&t.restitution>=0;}
bool triangle(const CollTriInfo& raw,const Vector3f* vertices,int count,motion::Triangle& out,std::string& e){
 if(!vertices||count<=0)return fail(e,"source trace vertex table unavailable");
 for(unsigned i=0;i<3;++i){if(raw.mVertexIndices[i]>=unsigned(count))return fail(e,"source trace triangle vertex out of bounds");out.vertices[i]=vec(vertices[raw.mVertexIndices[i]]);}
 out.normal=vec(raw.mTriangle.mNormal);out.offset=raw.mTriangle.mOffset;
 // Native collision export reverses source winding for its inward-edge planes.
 // Restore source CA x BA winding against the retained actual plane normal.
 const auto a=out.vertices[0],b=out.vertices[1],c=out.vertices[2];
 const Vec3 ba{b.x-a.x,b.y-a.y,b.z-a.z},ca{c.x-a.x,c.y-a.y,c.z-a.z};
 const Vec3 n{ca.y*ba.z-ca.z*ba.y,ca.z*ba.x-ca.x*ba.z,ca.x*ba.y-ca.y*ba.x};
 if(n.x*out.normal.x+n.y*out.normal.y+n.z*out.normal.z<0)std::swap(out.vertices[1],out.vertices[2]);
 return true;
}
bool resolve(Pellet* body,Trace& trace,CollTriInfo* raw,const Vector3f* vertices,int count,Shape* model,DynCollShape* platform,
 rigid::ContactReceiver* receiver,PcOriginalNumberContacts& contacts,std::string& e){
 motion::Triangle t;if(!triangle(*raw,vertices,count,t,e))return false;
 motion::Contact contact;const auto result=motion::intersect(t,trace.position,trace.radius,trace.hardIntersect,contact);
 if(result==motion::Intersection::Invalid)return fail(e,"source trace nonfinite triangle/contact");
 if(result==motion::Intersection::Miss)return true;
 if(receiver&&!receiver->contact(contact.point,contact.normal,e))return false;
 if(contact.normal.y>=0.6f){contacts.floor=raw;contacts.floorNormal=contact.normal;contacts.floorModel=model;contacts.floorPlatform=platform;}
 else if(std::fabs(contact.normal.y)<=0.70710677f){contacts.wall=raw;contacts.wallNormal=contact.normal;}
 Vec3 velocity;if(!motion::restitution(trace.velocity,contact.normal,trace.restitution,velocity))return fail(e,"source trace response nonfinite");
 trace.velocity=velocity;
 trace.position.x+=contact.normal.x*contact.overlap;trace.position.y+=contact.normal.y*contact.overlap;trace.position.z+=contact.normal.z*contact.overlap;
 if(!valid(trace))return fail(e,"source trace corrected sphere nonfinite");
 // Actual item/platform contact receives its real triangle and body state.
 // The source platform pass has no extra movement or P1 elasticity override.
 if(platform&&body){
  if(platform->mLastContactTick!=mapMgr->mCurrTraceTick){platform->mLastContactTick=mapMgr->mCurrTraceTick;++platform->mContactTickCount;}
  Vector3f position=native(trace.position),v=native(trace.velocity);platform->touchCallback(raw->mTriangle,position,v);
 }
 return true;
}
bool mapCandidates(Shape* model,const Trace& trace,std::vector<unsigned>& indices,std::string& e){
 if(!model||!model->mTriList||model->mTriCount<=0||model->mTriCount>1048576||!model->mCollGroups||
    !std::isfinite(model->mGridSize)||model->mGridSize<=0||model->mGridSizeX<=0||model->mGridSizeY<=0)return fail(e,"actual source map collision grid unavailable");
 const float startX=(trace.position.x-trace.radius-model->mCourseExtents.mMin.x)/model->mGridSize;
 const float endX=(trace.position.x+trace.radius-model->mCourseExtents.mMin.x)/model->mGridSize;
 const float startZ=(trace.position.z-trace.radius-model->mCourseExtents.mMin.z)/model->mGridSize;
 const float endZ=(trace.position.z+trace.radius-model->mCourseExtents.mMin.z)/model->mGridSize;
 if(!std::isfinite(startX)||!std::isfinite(endX)||!std::isfinite(startZ)||!std::isfinite(endZ))return fail(e,"source trace grid extent nonfinite");
 // Source casts toward zero, clamps even wholly out-of-bounds spheres, and
 // visits x then z. Native grid storage is z-major; traversal need not be.
 const auto cell=[](float value,int size){return value<0?0:value>=float(size)?size-1:int(value);};
 const int minX=cell(startX,model->mGridSizeX),minZ=cell(startZ,model->mGridSizeY);
 const int maxX=cell(endX,model->mGridSizeX),maxZ=cell(endZ,model->mGridSizeY);
 indices.clear();
 const auto base=reinterpret_cast<std::uintptr_t>(model->mTriList),end=base+std::size_t(model->mTriCount)*sizeof(CollTriInfo);
 for(int x=minX;x<=maxX;++x)for(int z=minZ;z<=maxZ;++z){
  // mNextCollGroup is mutable native solver scratch. Never follow it into
  // another actor's platform list; this pass uses the real static grid cell.
  const auto* group=model->mCollGroups[x+z*model->mGridSizeX];if(!group)continue;
  if(group->mTriCount<0||(group->mTriCount&&!group->mTriangleList))return fail(e,"source static grid cell corrupt");
  for(int i=0;i<group->mTriCount;++i){const auto p=reinterpret_cast<std::uintptr_t>(group->mTriangleList[i]);
   if(p<base||p>=end||(p-base)%sizeof(CollTriInfo))return fail(e,"source static grid contains foreign triangle");
   indices.push_back(unsigned((p-base)/sizeof(CollTriInfo)));
  }
 }
 // Preserve list order and repeated entries: each contact mutates the sphere.
 // Qualification still requires proof these converted cells retain SOURCE lists.
 return true;
}
}
bool pc_p2_original_number_trace_map(Pellet* body,Trace& trace,float dt,rigid::ContactReceiver* receiver,PcOriginalNumberContacts& contacts,std::string& e){
 if(!mapMgr||!valid(trace))return fail(e,"actual source map/trace unavailable");
 const float speed=std::sqrt(trace.velocity.x*trace.velocity.x+trace.velocity.y*trace.velocity.y+trace.velocity.z*trace.velocity.z);
 unsigned count=0;float step=0;if(!motion::stepCount(dt,speed,trace.radius,count,step))return fail(e,"source trace timestep invalid");
 ++mapMgr->mCurrTraceTick;std::vector<unsigned> indices;
 for(unsigned i=0;i<count;++i){
  trace.position.x+=trace.velocity.x*step;trace.position.y+=trace.velocity.y*step;trace.position.z+=trace.velocity.z*step;
  if(!valid(trace)||!mapCandidates(mapMgr->mMapModel,trace,indices,e))return false;
  for(const auto index:indices)if(!resolve(body,trace,&mapMgr->mMapModel->mTriList[index],mapMgr->mMapModel->mVertexList,
      mapMgr->mMapModel->mVertexCount,mapMgr->mMapModel,nullptr,receiver,contacts,e))return false;
 }
 e.clear();return true;
}
bool pc_p2_original_number_trace_platforms(Pellet* body,Trace& trace,rigid::ContactReceiver* receiver,PcOriginalNumberContacts& contacts,std::string& e){
 if(!mapMgr||!mapMgr->mCollShapeList||!valid(trace))return fail(e,"actual source platform list/trace unavailable");
 BoundBox box(native({trace.position.x-trace.radius,trace.position.y-trace.radius,trace.position.z-trace.radius}),
              native({trace.position.x+trace.radius,trace.position.y+trace.radius,trace.position.z+trace.radius}));
 FOREACH_NODE(DynCollShape,mapMgr->mCollShapeList->mChild,platform){
  if(platform->mCreature==body)continue;
  // P2 PlatAttacher is an item/map platform path, never enemy corpse chassis.
  if(platform->mCreature&&(platform->mCreature->isTeki()||platform->mCreature->isBoss()))continue;
  if(!box.intersects(platform->mBoundingBox))continue;
  if(!platform->mCollisionModel||!platform->mJointVisibility||platform->mCollGroupCount<0||
      (platform->mCollGroupCount&&!platform->mCollGroupList))return fail(e,"actual source platform geometry unavailable");
  for(int j=0;j<platform->mCollGroupCount;++j){auto* group=platform->mCollGroupList[j];if(!group)continue;
   if(group->mJointIndex<0||group->mJointIndex>=platform->mCollisionModel->mJointCount)return fail(e,"actual source platform joint out of bounds");
   if(!platform->mJointVisibility[group->mJointIndex])continue;
   if(group->mTriCount<0||(group->mTriCount&&!group->mTriangleList))return fail(e,"actual source platform triangles unavailable");
   for(int i=0;i<group->mTriCount;++i)if(!resolve(body,trace,group->mTriangleList[i],platform->mVertexList,
       platform->mCollisionModel->mVertexCount,platform->mCollisionModel,platform,receiver,contacts,e))return false;
  }
 }
 e.clear();return true;
}
