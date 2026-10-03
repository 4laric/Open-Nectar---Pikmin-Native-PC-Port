#pragma once
#include "pc_p2_original_number_native.h" // reviewed numeric owner PR199 dependency
#include "pc_p2_original_egg_number_journal.h"
#include "pc_p2_original_egg_native.h"
namespace p2originalresource {
// Scene-owned. Native/journal/root authority outlive this object; this object
// outlives every numeric body borrowing it. No factory installation occurs here.
class EggNumberAuthority final:public PcOriginalNumberAuthority {
public:
 EggNumberAuthority(p2original::egg::Native&,const NumberRootAuthority&);
 ~EggNumberAuthority()override;
 bool beginContents(const p2original::egg::Host&,const SourceIdentity&,unsigned,
                    const P2EggConfig&,const P2EggVec3&,const EggContents&,std::string&);
 void endContents()noexcept;
 bool pending(const ChildOutcome&,unsigned&,ContentsRecord&,std::string&)override;
 bool completed(const ChildIdentity&,unsigned&,ContentsRecord&,std::string&)override;
 bool consumeAccepted(const ChildIdentity&,std::string&)override;
private:
 p2original::egg::Native& mNative;NumberJournalAuthority mJournal;
};
}
