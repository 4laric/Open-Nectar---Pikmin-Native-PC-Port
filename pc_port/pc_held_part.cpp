// Generic held ship part (#901). Contract: pc_held_part_policy.h.
#include "pc_held_part.h"

#include "pc_held_part_policy.h"
#include "pc_randomizer.h"

#include "FlowController.h"
#include "ID32.h"
#include "Pellet.h"
#include "PlayerState.h"
#include "RadarInfo.h"
#include "TekiPersonality.h"
#include "teki.h"

#include <cstdio>

namespace {

bool partExists(unsigned id)
{
    // PlayerState::existUfoParts: collected, cached on the ground for this
    // stage (GeneratorCache::hasUfoParts), or alive as a pellet right now.
    if (!playerState || !flowCont.mCurrentStage) return false;
    return playerState->existUfoParts(id);
}

unsigned generatorUid(BTeki* teki)
{
    return teki && teki->mGenerator ? pc_randomizer_generator_id(teki->mGenerator) : 0u;
}

} // namespace

unsigned pc_held_part_for_pellet_config(int pelletConfigIdx)
{
    if (!pelletMgr || pelletConfigIdx < 0) return 0;
    PelletConfig* config = pelletMgr->getConfigFromIdx(pelletConfigIdx);
    if (!config || !Pellet::isUfoPartsID(config->mModelId.mId)) return 0;
    return config->mModelId.mId;
}

void pc_held_part_log_assign(unsigned partId, unsigned source, unsigned target, int p1Boss)
{
    ID32 id(partId);
    std::printf("P2_HELD_PART_ASSIGN part=%s source_id=%u target=%u p1_boss=%d via=arena\n", id.mStringID, source,
                target, p1Boss);
    std::fflush(stdout);
}

bool pc_held_part_birth(BTeki* teki)
{
    if (!teki || !teki->mPersonality) return false;
    ID32& id = teki->mPersonality->mID;
    const bool isPart = Pellet::isUfoPartsID(id.mId);
    if (!isPart) return false;
    char name[5];
    std::snprintf(name, sizeof(name), "%s", id.mStringID);
    const bool exists = partExists(id.mId);
    if (!p2heldpart::keepAtBirth(isPart, exists)) {
        std::printf("P2_HELD_PART_CLEAR part=%s generator=%u teki=%d source_id=%u reason=exists\n", name,
                    generatorUid(teki), int(teki->mTekiType), pc_randomizer_p2_source_for(teki));
        std::fflush(stdout);
        id.setID('none');
        return false;
    }
    std::printf("P2_HELD_PART_HOLD part=%s generator=%u teki=%d source_id=%u\n", name, generatorUid(teki),
                int(teki->mTekiType), pc_randomizer_p2_source_for(teki));
    std::fflush(stdout);
    return true;
}

bool pc_held_part_claim_spawn_items(BTeki* teki)
{
    if (!teki || teki->mPcHeldPartDropped) return false;
    teki->mPcHeldPartDropped = true;
    if (teki->mPersonality && Pellet::isUfoPartsID(teki->mPersonality->mID.mId)) {
        pc_held_part_ensure_shape(teki->mPersonality->mID.mId);
        std::printf("P2_HELD_PART_DROP part=%s generator=%u teki=%d source_id=%u via=spawnItems health=%.1f\n",
                    teki->mPersonality->mID.mStringID, generatorUid(teki), int(teki->mTekiType),
                    pc_randomizer_p2_source_for(teki), double(teki->mHealth));
        std::fflush(stdout);
    }
    return true;
}

bool pc_held_part_drop(BTeki* teki, const char* via)
{
    if (!teki || !teki->mPersonality) return false;
    const unsigned id = teki->mPersonality->mID.mId;
    const bool isPart = Pellet::isUfoPartsID(id);
    // Cheap pre-check so the pellet scan in existUfoParts only runs on a
    // real holder death.
    if (teki->mPcHeldPartDropped || !isPart || teki->mHealth > 0.0f) return false;
    const p2heldpart::Drop drop = p2heldpart::onDeath(false, isPart, teki->mHealth, partExists(id));
    teki->mPcHeldPartDropped = true;
    ID32 name(id);
    if (drop == p2heldpart::Drop::AlreadyExists) {
        if (radarInfo) radarInfo->detachParts(teki);
        std::printf("P2_HELD_PART_SKIP part=%s generator=%u teki=%d reason=exists via=%s\n", name.mStringID,
                    generatorUid(teki), int(teki->mTekiType), via ? via : "?");
        std::fflush(stdout);
        return false;
    }
    pc_held_part_ensure_shape(id);
    teki->spawnPellets(int(id), PELCOLOR_Part, 1);
    if (radarInfo) radarInfo->detachParts(teki);
    const bool spawned = partExists(id);
    const Vector3f& pos = teki->getPosition();
    std::printf("P2_HELD_PART_DROP part=%s generator=%u teki=%d source_id=%u via=%s ok=%d x=%.1f z=%.1f\n",
                name.mStringID, generatorUid(teki), int(teki->mTekiType), pc_randomizer_p2_source_for(teki),
                via ? via : "?", spawned ? 1 : 0, double(pos.x), double(pos.z));
    std::fflush(stdout);
    return spawned;
}

void pc_held_part_ensure_shape(unsigned partId)
{
    if (!pelletMgr || !Pellet::isUfoPartsID(partId)) return;
    if (pelletMgr->pcEnsureShape(partId)) return;
    ID32 name(partId);
    std::printf("P2_HELD_PART_SHAPE part=%s ok=0\n", name.mStringID);
    std::fflush(stdout);
}
