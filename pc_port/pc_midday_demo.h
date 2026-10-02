#pragma once
#include "pc_midday_capture.h"
#include "pc_midday_constructor.h"
#include "pc_midday_restore.h"
#include "pc_midday_player_core.h"
#include <array>
#include <memory>
struct DemoFlags;
class Creature;
namespace pc_midday {
struct DemoStageTag;
struct DemoDescriptor {uint16_t index=0;int16_t movie=0;uint16_t part=0;bool text=false;};
struct DemoFields {
 std::array<uint8_t,32> stored{};
 std::vector<DemoDescriptor> descriptors;
 int16_t current=-1;float timer=0;uint64_t target=0;
};
bool encodeDemo(const DemoFields&,Bytes&,std::string&);
bool decodeDemo(const Bytes&,DemoFields&,std::string&);
bool captureDemo(const DemoFlags&,const BirthLedger&,const PlayerCoreReadFence&,Bytes&,std::string&);
// Owns only this DemoFlags resource and its arrays/descriptors. Borrowed names
// need the pinned compiled content lifetime; target actors belong to whole scene.
class IsolatedDemo {
 struct Impl;std::unique_ptr<Impl>impl_;
public:
 IsolatedDemo();~IsolatedDemo();
 bool prepare(const DemoFlags&compiledContent,const Bytes&,const std::map<uint64_t,Creature*>&,const RestoreGate&,ConstructorFence&,std::string&);
 DemoFlags* resource()const;
};
}
