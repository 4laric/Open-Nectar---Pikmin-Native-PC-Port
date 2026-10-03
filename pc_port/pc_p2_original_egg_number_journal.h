#pragma once
#include "pc_p2_original_resource_contents.h"
#include <functional>
#include <optional>
namespace p2originalresource {
// Inject the actual selected-scene catalog/incarnation authority. No default
// root type, active-body lookup, fabricated UID or reconstructed journal.
class NumberRootAuthority {
public:
 virtual ~NumberRootAuthority()=default;
 virtual bool resolve(const SourceIdentity&,unsigned& rootSource,std::string&)const=0;
};
bool exactNumberFloat(float,float)noexcept;
class NumberJournalAuthority {
public:
 using Admission=std::function<bool(const ContentsRequirements&,std::string&)>;
 NumberJournalAuthority(EggContents&,const NumberRootAuthority&,Admission);
 bool begin(const SourceIdentity&,unsigned,const P2EggConfig&,const P2EggVec3&,std::string&);
 void end()noexcept;
 bool active()const noexcept{return mScope.has_value();}
 bool pending(const ChildOutcome&,unsigned&,ContentsRecord&,std::string&)const;
 bool completed(const ChildIdentity&,unsigned&,ContentsRecord&,std::string&)const;
 bool consumeAccepted(const ChildIdentity&,std::string&);
private:
 struct Scope {SourceIdentity root;unsigned type;ContentsRequirements required;};
 EggContents& mJournal;const NumberRootAuthority& mRoots;Admission mAdmission;
 std::optional<Scope> mScope;
};
}
