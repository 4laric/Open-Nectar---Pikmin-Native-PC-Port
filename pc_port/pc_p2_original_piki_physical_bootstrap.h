#pragma once
#include "pc_p2_original_piki_pool.h"
#include "pc_p2_original_piki_host_context.h"
#include <array>
namespace p2retail {class SceneContext;}
namespace p2original {namespace piki {
// Process-retained factory member. Owns only named host initialization, native
// context observations/registrations and its one work-counter contribution.
// It never runs P1 init/FSM/animation or grants source World/Brain admission.
class NativePhysicalBootstrap final {
public:
 NativePhysicalBootstrap()=default;
 ~NativePhysicalBootstrap();
 NativePhysicalBootstrap(const NativePhysicalBootstrap&)=delete;
 NativePhysicalBootstrap& operator=(const NativePhysicalBootstrap&)=delete;
 bool initialize(const p2retail::SceneContext&,PoolTicket,std::string&);
 bool current(std::string&)const;
 // Canonical Runtime/animator/effect consumers must first retire and forget the
 // association. This method independently refuses a remaining actual handle.
 // Pool status/census is a separate owner; success does not release its ticket.
 bool dispose(std::string&);
 bool owned()const noexcept{return stage!=nullptr;}
 bool initialized()const noexcept{return complete;}
 bool matchesTicket(PoolTicket t)const noexcept{return ticket.body==t.body&&ticket.allocation==t.allocation&&t.body&&t.allocation;}
private:
 friend bool poolReleasePhysical(PoolTicket,NativePhysicalBootstrap&,std::string&);
 // Only the real Pool helper, after exact raw-slot retirement and removal of
 // its retained census, can make this owner reusable/destructible.
 void poolRetired()noexcept;
 bool retainedStageCurrent(std::string&)const;
 bool physicalResourcesEmpty()const noexcept;
 const p2retail::SceneContext* stage=nullptr;
 PoolTicket ticket;
 OriginalPikiBody source;
 std::uint64_t serial=0,revision=0;
 std::array<HostUpdateBinding,4> contexts;
 bool counter=false,complete=false;
};
}}
