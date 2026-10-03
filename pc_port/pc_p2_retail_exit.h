#pragma once
#include "pc_p2_retail_scene.h"
#include <memory>
class Creature;class Graphics;
namespace p2retail {
struct ExitSpec {
 Snapshot floor;Anchor anchor;std::string transition,campaign,session,planBytes;
 std::uint64_t revision=0;
};
inline bool exitSpec(const FloorPlan& input,const Snapshot& floor,const std::string& campaign,
                     const std::string& session,std::uint64_t revision,ExitSpec& out,std::string& error){
 FloorPlan plan;
 if(!parseFloorPlan(input.authenticatedBytes,floor.scene.layoutSha256,plan,error))return false;
 const auto* cave=descriptor(plan.cave);
 if(!cave||floor.cave!=plan.cave||floor.floor!=plan.floor||floor.maxFloor!=cave->maxFloor
  ||floor.source!=cave->source||floor.sourceSha256!=plan.sourceSha256||floor.catalogSha256!=plan.catalogSha256
  ||!floor.story||!floor.inCave||floor.scene.seed.empty()||floor.scene.visit.empty()||!floor.scene.serial
  ||!hex64(campaign)||!hex64(session)||!revision
  ||input.exit.unit!=plan.exit.unit||input.exit.slot!=plan.exit.slot
  ||input.exit.x!=plan.exit.x||input.exit.y!=plan.exit.y||input.exit.z!=plan.exit.z
  ||input.exit.yawDegrees!=plan.exit.yawDegrees||input.transition!=plan.transition){
  error="retail_exit_selected_plan_context";return false;
 }
 ExitSpec next;next.floor=floor;next.anchor=plan.exit;next.transition=plan.transition;
 next.campaign=campaign;next.session=session;next.revision=revision;next.planBytes=plan.authenticatedBytes;
 out=std::move(next);error.clear();return true;
}
// Owned by the actual selected NativeFloor SceneOps. Physical presentation is
// authored; transform/identity come exclusively from authenticated source plan.
// No input, synthetic SAVE, roster allocation or transition grant is supplied.
class ExitLifecycle final {
public:
 ExitLifecycle();~ExitLifecycle();
 ExitLifecycle(const ExitLifecycle&)=delete;ExitLifecycle&operator=(const ExitLifecycle&)=delete;
 bool preflight(const SceneContext&,std::string&);
 bool birth(std::string&);
 bool commit(const Snapshot&,std::string&);
 bool canRelease(std::string&)const;
 bool release(std::string&);
 bool identity(const Creature*,ExitSpec&)const;
 Creature* body()const noexcept;
 bool owned(const SceneContext&)const noexcept;
 void draw(Graphics&);
private:struct Impl;std::unique_ptr<Impl> m;
};
}
bool pc_p2_retail_exit_preflight(const p2retail::SceneContext&,std::string&);
bool pc_p2_retail_exit_birth(const p2retail::SceneContext&,std::string&);
bool pc_p2_retail_exit_commit(const p2retail::SceneContext&,std::string&);
bool pc_p2_retail_exit_owned(const p2retail::SceneContext&) noexcept;
bool pc_p2_retail_exit_can_release(const p2retail::SceneContext&,std::string&);
bool pc_p2_retail_exit_release(const p2retail::SceneContext&,std::string&);
void pc_p2_retail_exit_draw(Graphics&);
