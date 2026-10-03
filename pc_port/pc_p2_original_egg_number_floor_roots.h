#pragma once
#include "pc_p2_original_egg_number_journal.h"
#include "pc_p2_retail_scene.h"

namespace p2originalresource {
// The actual scene owner must outlive this borrower. No ambient campaign hash,
// reconstructed source UID, live-parent pointer or caller-authored floor proof.
class NumberFloorRoots final : public NumberRootAuthority {
public:
 explicit NumberFloorRoots(const p2retail::SceneContext&);
 bool resolve(const SourceIdentity&,unsigned&,std::string&)const override;
private:
 bool selected()const noexcept;
 const p2retail::SceneContext* mOwner;
 p2retail::Snapshot mFloor;
 std::string mCampaign,mSession;
 std::uint64_t mRevision,mSerial;
};
}
