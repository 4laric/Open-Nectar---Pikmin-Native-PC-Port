#pragma once
#include <string>
class UpdateContext;
class UpdateMgr;
namespace p2original { namespace piki {
// Internal physical bootstrap owner, used while the singular factory holds the
// actual Stage creating thread. This does NOT supply a source update-context
// eligibility flag or activate a body. Native pool objects may already contain
// registrations made by createObject; those are observed/borrowed, not added
// twice or removed as though this bootstrap created them.
class HostUpdateBinding final {
public:
 enum class Phase { Empty, Borrowed, Registered, Uncertain };
 HostUpdateBinding()=default;
 ~HostUpdateBinding();
 HostUpdateBinding(const HostUpdateBinding&)=delete;
 HostUpdateBinding& operator=(const HostUpdateBinding&)=delete;
 bool acquire(UpdateContext&,UpdateMgr* actualManager,bool searchPiki,std::string&);
 bool current(std::string&)const;
 bool canRelease(std::string&)const;
 bool release(std::string&);
 Phase phase()const noexcept{return mPhase;}
 bool owned()const noexcept{return mPhase!=Phase::Empty;}
private:
 UpdateContext* mContext=nullptr;
 UpdateMgr* mManager=nullptr;
 int mSlot=-1;
 bool mPiki=false;
 Phase mPhase=Phase::Empty;
};
} }
