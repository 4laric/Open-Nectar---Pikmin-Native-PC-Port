#pragma once
#include "pc_p2_original_resource_contents.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace p2originalnumber {
constexpr std::size_t retainedLimit=65536;
struct Receipt {
 p2originalresource::ChildOutcome birthPayload;
 unsigned rootSourceType=0; // Actual authored Egg37 or captured Qurione16.
 bool consumed=false;
};
struct BirthPlan {Receipt receipt;};
struct ConsumePlan {const void* pellet=nullptr;std::uint64_t handle=0;Receipt receipt;};
enum class QueryResult {Missing,Present,Unavailable};
struct Report {std::string catalogFingerprint;std::vector<Receipt> receipts;std::size_t liveBodies=0;};
// Single-threaded retained RAM receipts, not source admission or a SAVE schema.
// Services authority must authenticate the pending callback/journal before
// preflightBirth. Only actual native success permits bindBirth; null allocation
// has no ledger transition. The real producer owns attempted-once failure policy.
// This ledger never creates, restores, or mutates EggContents or native bodies.
class Ledger {
public:
 explicit Ledger(std::string catalogFingerprint);
 Ledger(const Ledger&)=delete;
 Ledger& operator=(const Ledger&)=delete;
 bool preflightBirth(const p2originalresource::ChildOutcome& pending,unsigned rootSourceType,BirthPlan&,std::string&)const;
 bool bindBirth(const BirthPlan&,const void* actuallyBornPellet,std::uint64_t& handle,std::string&);
 bool owns(const void*)const noexcept;
 bool handle(const void*,std::uint64_t& out)const noexcept;
 QueryResult query(const void*,std::uint64_t,const p2originalresource::ContentsRecord& actualJournal,unsigned actualRootType,Receipt&,std::string&)const;
 bool prepareConsume(const void*,std::uint64_t,const p2originalresource::ContentsRecord& actualJournal,unsigned actualRootType,ConsumePlan&,std::string&)const;
 // Invoke the actual producer's consume exactly once BETWEEN prepare and
 // commit. Require its completed successful matching receipt NOW consumed.
 // Refusal is fail-closed; caller must not grant population before commit.
 bool commitConsumed(const ConsumePlan&,const p2originalresource::ContentsRecord& actualJournal,unsigned actualRootType,std::string&);
 bool retire(const void*,std::uint64_t,std::string&);
 bool unload(std::string&)const;
 bool freshSession(const std::string& catalogFingerprint,std::string&);
 Report report()const;
private:
 struct Binding {p2originalresource::ChildIdentity identity;std::uint64_t handle=0;};
 bool validBirth(const Receipt&)const;
 bool journalMatches(const Receipt&,const p2originalresource::ContentsRecord&,unsigned,bool consumed)const;
 std::string mCatalog;
 std::map<p2originalresource::ChildIdentity,Receipt> mReceipts;
 std::map<const void*,Binding> mBindings;
 std::uint64_t mNextHandle=1;
};
}
