#pragma once
#include "pc_p2_original_number_ledger.h"
#include "pc_p2_original_number_profile.h"
class Pellet;class GoalItem;
// Implemented by the actual admitted source Services owner. Pending authority
// exists only inside NativeHost.contents' begin/end scope. Completed reads and
// accepted consumption use that SAME owner's retained journal, never a copy.
class PcOriginalNumberAuthority {
public:
 virtual ~PcOriginalNumberAuthority();
 virtual bool pending(const p2originalresource::ChildOutcome&,unsigned& actualRootType,p2originalresource::ContentsRecord&,std::string&)=0;
 virtual bool completed(const p2originalresource::ChildIdentity&,unsigned& actualRootType,p2originalresource::ContentsRecord&,std::string&)=0;
 // false leaves the journal unchanged; true publishes consumed synchronously.
 virtual bool consumeAccepted(const p2originalresource::ChildIdentity&,std::string&)=0;
};
enum class PcOriginalNumberBirth {Born,PoolExhausted,Fault};
// Before each fresh producer generation/RNG. Reachable Five refuses while the
// literal source LOD/rigid adapter is absent. An unforced nonnumeric union is inert.
bool pc_p2_original_number_resources(const p2originalresource::ContentsRequirements&,std::string&);
PcOriginalNumberBirth pc_p2_original_number_birth(const p2originalresource::ChildOutcome&,PcOriginalNumberAuthority&,Pellet*& out,std::string&);
bool pc_p2_original_number_tag(const Pellet*)noexcept;
const p2originalnumber::Profile* pc_p2_original_number_profile(const Pellet*)noexcept;
p2originalnumber::QueryResult pc_p2_original_number_query(const Pellet*,p2originalnumber::Receipt&,std::string&);
bool pc_p2_original_number_consume(Pellet*,GoalItem*,unsigned& actualYield,std::string&);
void pc_p2_original_number_collision(Pellet*);
// Owns numeric FSM/animation and one source trace; true bypasses P1's update.
// The pre-RNG resource hold remains until actual source scene tracing is admitted.
bool pc_p2_original_number_update(Pellet*);
bool pc_p2_original_number_unload(std::string&);
// Read-only before provider disposal or SAVE. Requires actual bodies retired;
// it does not destroy receipts, authority objects or App-heap resources.
bool pc_p2_original_number_preflight_teardown(std::string&);
bool pc_p2_original_number_authority_idle(const PcOriginalNumberAuthority*)noexcept;
bool pc_p2_original_number_new_session(std::string&);
