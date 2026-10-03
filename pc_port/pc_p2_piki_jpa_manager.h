#pragma once
#include "pc_p2_piki_jpa_bank.h"
#include <array>
#include <memory>
namespace p2original { namespace pikiJPA {
// Source successful-admission frontier, not a substitute for a particle renderer.
class Manager;
class Emitter final {
public:
 unsigned resourceId()const{return mId;} unsigned poolIndex()const{return mSlot;}
 unsigned group()const{return mGroup;} std::uint32_t seed()const{return mSeed;}
 std::uint64_t admission()const{return mAdmission;}
 const Bank& bank()const{return *mBank;}
 const std::vector<unsigned char>& resourceBytes()const{return *mBank->bytes(mRole);}
 Emitter(const Emitter&)=delete;Emitter& operator=(const Emitter&)=delete;
private:
 friend class Manager;
 Emitter(unsigned id,unsigned slot,unsigned group,std::uint32_t seed,std::uint64_t admission,std::shared_ptr<const Bank> bank,const char* role):mId(id),mSlot(slot),mGroup(group),mSeed(seed),mAdmission(admission),mBank(std::move(bank)),mRole(role){}
 unsigned mId,mSlot,mGroup;std::uint32_t mSeed;std::uint64_t mAdmission;
 std::shared_ptr<const Bank> mBank;const char* mRole;
};
using EmitterHandle=std::shared_ptr<const Emitter>;
class Manager final {
public:
 static constexpr unsigned capacity=300;
 // Immutable owned selected bytes; no path reopen or external seed setter.
 explicit Manager(const Bank&);
 ~Manager();
 Manager(const Manager&)=delete;Manager& operator=(const Manager&)=delete;
 bool create(unsigned sourceId,EmitterHandle& out,std::string& error);
 bool erase(EmitterHandle& owned,std::string& error);
 bool owns(const EmitterHandle&)const;
 // Same fixed-manager campaign only, zero live emitters, all16 identical raw
 // roles. Atomic bank replacement never resets RNG/admission/free-list history.
 bool rebindSelectedBank(const Bank&,std::string& error);
 // Caller retires actual particle/render owners before these source deletions.
 // Source forceDeleteAll visits groups ascending, each group's tail first.
 void killAll();void reset(){killAll();}
 std::uint32_t frontier()const{return mFrontier;}
 std::uint64_t admissions()const{return mAdmissions;}
 unsigned live()const{return mLive;}unsigned freeCount()const{return capacity-mLive;}
 const Bank& bank()const{return *mBank;}
private:
 void retire(unsigned slot);
 std::shared_ptr<const Bank> mBank;
 std::array<EmitterHandle,capacity> mSlots{};
 std::array<int,capacity> mNext{},mPrev{};
 std::array<int,9> mHead{},mTail{};
 int mFree=-1;unsigned mLive=0;std::uint32_t mFrontier=0;std::uint64_t mAdmissions=0;
};
} }
