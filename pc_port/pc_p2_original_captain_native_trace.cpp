#include "pc_p2_original_captain_native_trace.h"
#include "pc_p2_original_room_trace.h"
#include "pc_p2_retail_rooms.h"
#include "pc_p2_retail_scene.h"
// Strong scene-owned original geometry/flags, not legacy MapMgr queries.
const p2retail::SourceRoomGeometry* pc_p2_retail_scene_source_geometry(const p2retail::SceneContext&,std::uint64_t,std::uint64_t) noexcept;
const p2retail::SourceFloorParameters* pc_p2_retail_scene_floor_parameters(const p2retail::SceneContext&,std::uint64_t,std::uint64_t) noexcept;
bool pc_p2_retail_scene_visit_room(const p2retail::SceneContext&,std::uint64_t,std::uint64_t,Navi*,int,std::string&);
namespace p2original {namespace captain {namespace bodyphases {
namespace {
namespace rt=p2originalnumber::roomTrace;
bool fail(std::string& e,const char* message){e=message;return false;}
rt::Vec3 numeric(Vec3 v){return {v.x,v.y,v.z};}
Vec3 source(rt::Vec3 v){return {v.x,v.y,v.z};}
class NativeTrace;
struct RoomReceiver {const NativeTrace* trace;const BodyBorrowerGuard* guard;const Owner* owner;const Navi* actor;int room;};
const RoomReceiver* roomReceiver=nullptr;
class NativeTrace final:public SourceSceneTrace {
public:
 const p2retail::SceneContext* context=nullptr;const LoadedScene* loaded=nullptr;
 const p2retail::SourceRoomGeometry* geometry=nullptr;const p2retail::SourceFloorParameters* parameters=nullptr;
 std::uint64_t serial=0,revision=0;std::string campaign,session,catalog;rt::Mesh mesh;
 const LoadedScene& scene()const override{return *loaded;}
 bool current(std::string& e)const {
  const auto* s=pc_p2_original_captain_loaded_scene();const auto* w=pc_p2_original_captain_world();
  if(!s||s!=loaded||!w||!serial||pc_p2_retail_scene_prepared()!=context||s->incarnation()!=serial||w->incarnation()!=serial
   ||s->selectedCampaign()!=campaign||w->selectedCampaign()!=campaign||s->selectedFingerprint()!=session||w->selectedFingerprint()!=session
   ||s->sourceCatalog()!=catalog||w->sourceCatalog()!=catalog||(w->phase()!=Phase::Loading&&w->phase()!=Phase::GameWorldActive)
   ||context->snapshot().scene.serial!=serial||context->selectionRevision()!=revision
   ||context->campaignSha256()!=campaign||context->sessionSha256()!=session
   ||pc_p2_retail_scene_source_geometry(*context,serial,revision)!=geometry
   ||pc_p2_retail_scene_floor_parameters(*context,serial,revision)!=parameters)
   return fail(e,"original captain map trace lost actual scene geometry/parameter owner");
  return true;
 }
 bool floor(FloorHandle handle,FloorFacts& out,std::string& e)const override {
  if(!current(e)||handle.incarnation!=serial||!handle.triangle)return fail(e,"original captain floor handle expired");
  // Compare identities before dereferencing: native P1 triangle pointers and
  // pointers from another selected scene cannot qualify source ground facts.
  for(const auto& t:mesh.triangles)if(t.identity.original==handle.triangle){
   FloorFacts next;next.slip=(t.identity.mapCode>>4)&3u;next.contents=t.identity.mapCode&15u;
   next.planeNormal=source(t.geometry.face.normal);if(!current(e))return false;out=next;return true;
  }
  return fail(e,"original captain floor is outside selected source triangle table");
 }
 struct Borrow final:rt::Owner {
  const NativeTrace& trace;BodyBorrowerGuard guard;
  Borrow(const NativeTrace& t,BodyBorrowerGuard g):trace(t),guard(std::move(g)){}
  bool current(const rt::Mesh& m,std::string& e)override{return &m==&trace.mesh&&trace.current(e)&&guard.current(e);}
  bool hidden(const rt::Mesh& m,rt::Hidden& out,std::string& e)override {
   if(!current(m,e))return false;
   if(trace.parameters->hasHiddenCollision)return fail(e,"actual original hidden sentinel/bounds producer unavailable");
   out={};return true;
  }
 };
 bool map(Navi& n,TraceInfo& out,float rate,std::string& e)override {
  BodyBorrowerGuard guard;if(!current(e)||!BodyBorrowerGuard::capture(*context,&n,guard,e))return false;
  Borrow owner(*this,std::move(guard));rt::Move move;
  move.sphere.position=numeric(out.sphere.center);move.sphere.radius=out.sphere.radius;
  move.sphere.velocity=numeric(out.velocity);move.sphere.restitution=out.traceRadius;
  rt::Report report;
  // FakePiki supplies no rigid contact delegate. Its room/bounce/wall callbacks
  // occur after map returns and velocity is published by the common owner.
  if(!rt::traceMap(mesh,owner,move,rate,nullptr,report,e)||!owner.current(mesh,e))return false;
  TraceInfo next=out;next.sphere.center=source(move.sphere.position);next.velocity=source(move.sphere.velocity);
  next.floor=move.hasFloor?FloorHandle{move.floor.original,serial}:FloorHandle{};
  next.wall=move.hasWall?FloorHandle{move.wall.original,serial}:FloorHandle{};
  next.floorNormal=source(move.floorNormal);next.wallNormal=source(move.wallNormal);next.roomIndex=move.roomIndex;
  out=next;return true;
 }
 bool platforms(Navi&,TraceInfo&,float,std::string& e)override{return fail(e,"actual original PlatMgr/OBB trace producer unavailable");}
 bool constrain(Navi& n,Sphere&,std::string& e)override {
  BodyBorrowerGuard guard;if(!current(e)||!BodyBorrowerGuard::capture(*context,&n,guard,e))return false;
  if(parameters->hasHiddenCollision)return fail(e,"actual original hidden bounds clamp producer unavailable");
  return guard.current(e);
 }
 bool room(Navi& n,int room,std::string& e)override {
  auto* owner=pc_p2_original_captain_body_phase_owner(&n);BodyBorrowerGuard guard;
  if(roomReceiver||!owner||!current(e)||!owner->roomVisitCurrent(&n,*this,room,e)
   ||!BodyBorrowerGuard::capture(*context,&n,guard,e))return fail(e,"source room visit lacks genuine trace/body phase");
  RoomReceiver receiver{this,&guard,owner,&n,room};roomReceiver=&receiver;
  struct End {~End(){roomReceiver=nullptr;}} end;
  if(!pc_p2_retail_scene_visit_room(*context,serial,revision,&n,room,e))return false;
  return current(e)&&guard.current(e)&&owner->roomVisitCurrent(&n,*this,room,e);
 }
};
}
bool nativeRoomVisitCurrent(const p2retail::SceneContext& ctx,std::uint64_t serial,std::uint64_t revision,const Navi* n,int room,std::string& e){
 const auto* call=roomReceiver;
 if(!call||call->actor!=n||call->room!=room||call->trace->context!=&ctx||call->trace->serial!=serial||call->trace->revision!=revision
  ||pc_p2_original_captain_body_phase_owner(n)!=call->owner)return fail(e,"source scene visit has no actual captain room receiver");
 return call->trace->current(e)&&call->guard->current(e)&&call->owner->roomVisitCurrent(n,*call->trace,room,e)&&roomReceiver==call;
}
std::unique_ptr<SourceSceneTrace> createNativeTrace(const p2retail::SceneContext& ctx,std::string& e){
 auto* s=pc_p2_original_captain_loaded_scene();auto* w=pc_p2_original_captain_world();
 if(!s||!w||w->phase()!=Phase::Loading)return fail(e,"original captain trace construction requires actual Loading scene"),nullptr;
 auto trace=std::make_unique<NativeTrace>();trace->context=&ctx;trace->loaded=s;trace->serial=s->incarnation();trace->revision=ctx.selectionRevision();
 trace->campaign=s->selectedCampaign();trace->session=s->selectedFingerprint();trace->catalog=s->sourceCatalog();
 trace->geometry=pc_p2_retail_scene_source_geometry(ctx,trace->serial,trace->revision);
 trace->parameters=pc_p2_retail_scene_floor_parameters(ctx,trace->serial,trace->revision);
 if(!trace->geometry||!trace->parameters){fail(e,"actual selected source geometry/floor parameters unavailable");return {};}
 if(!trace->current(e))return {};
 std::vector<rt::Vec3> vertices;std::vector<rt::TriangleInput> triangles;
 for(const auto& v:trace->geometry->vertices)vertices.push_back({v[0],v[1],v[2]});
 for(const auto& t:trace->geometry->triangles)triangles.push_back({t.abc,&t,int(t.roomIndex),t.mapcode});
 if(!rt::prepare(vertices,triangles,trace->mesh,e)||!trace->current(e))return {};
 return trace;
}
}}}
bool pc_p2_original_captain_room_visit_current(const p2retail::SceneContext& ctx,std::uint64_t serial,std::uint64_t revision,const Navi* n,int room,std::string& e){
 return p2original::captain::bodyphases::nativeRoomVisitCurrent(ctx,serial,revision,n,room,e);
}
