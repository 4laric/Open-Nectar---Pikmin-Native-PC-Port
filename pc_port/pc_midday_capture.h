#pragma once
#include "pc_midday_codec.h"
#include <functional>
#include <map>
#include <set>

namespace pc_midday {
enum class Family : uint32_t { Captain=0x6801, Pikmin, Enemy, Boss, Cargo, WorldItem, Structure, Projectile };
enum class Global : uint32_t { SceneClock=0x6811, BirthLedger, StockEconomy, GameplayRng, LogicalAudio, APLedger, CaveGraph, Jobs };
enum class Role { AnyActor, CaptainOwner, CarryTarget, WorkTarget, Predator };
enum class TypedSpecies { NativeRed, NativeYellow, NativeBlue, Purple, White, Bulbmin, Unknown };
enum class Completeness { Unsupported, Invalid, Complete };
struct Lifetime { uint64_t id=0; Family family=Family::WorldItem; };
class BirthLedger {
    uint64_t next_=1;
    std::map<const void*,Lifetime> live_;
    std::set<uint64_t> retired_;
public:
    bool birth(const void*, Family, uint64_t& id, std::string&);
    bool retire(const void*, std::string&);
    const Lifetime* lookup(const void*) const;
    uint64_t nextId() const { return next_; }
    size_t liveCount() const { return live_.size(); }
    const std::set<uint64_t>& tombstones() const { return retired_; }
    // Addresses are local lookup keys only, never encoded. Restore supplies new
    // addresses for existing saved IDs and persisted next/tombstone state.
    bool rebind(const void*, Lifetime, std::string&);
    bool restoreCounter(uint64_t, const std::set<uint64_t>&, std::string&);
};
struct Reference { const void* target=nullptr; Role role=Role::AnyActor; };
struct Observation {
    const void* address=nullptr; // local lookup only
    Family family=Family::WorldItem;
    TypedSpecies species=TypedSpecies::Unknown;
    uint32_t nativeObjectType=0;
    uint32_t enemySpecies=0;
    int state=-1, captainSlot=-1, legacyColor=-1, maturity=-1;
    float health=0,maxHealth=0,position[3]{},velocity[3]{};
    std::vector<Reference> references;
    std::string missingState; // real scalar preview never implies faithful FSM
};
struct Census {
    std::vector<Observation> actors;
    std::set<Family> enumeratedFamilies;
    uint64_t frameBefore=0,frameAfter=0;
    bool agreedReadOnlyFence=false;
    unsigned expectedCaptains=1;
};
struct AdapterOutput {
    Completeness status=Completeness::Unsupported;
    Bytes state;
    std::string reason;
};
using ActorAdapter=std::function<AdapterOutput(const Observation&)>;
struct Globals { Global family; AdapterOutput output; };
AdapterOutput captureBirthLedger(const BirthLedger&);
// Fails before replacing output on incomplete census, untracked lifetimes,
// unsupported typed adapters/global sections or invalid typed reference roles.
bool capture(const Census&, const BirthLedger&, const std::map<Family,ActorAdapter>&,
             const std::vector<Globals>&, const Binding&, uint64_t generation,
             uint64_t dayEnd, Snapshot&, Coverage&, std::string&);
}
