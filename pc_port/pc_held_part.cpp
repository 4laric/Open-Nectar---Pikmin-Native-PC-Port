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

// Four-character part id ('uf06'); ID32::mStringID is host byte order.
struct PartName {
    char s[5];
    explicit PartName(unsigned id)
    {
        for (int i = 0; i < 4; ++i) {
            const char c = char((id >> (24 - 8 * i)) & 0xff);
            s[i] = (c >= 32 && c < 127) ? c : '?';
        }
        s[4] = 0;
    }
};

unsigned generatorUid(BTeki* teki)
{
    return teki && teki->mGenerator ? pc_randomizer_generator_id(teki->mGenerator) : 0u;
}

} // namespace

unsigned pc_held_part_p2_source(BTeki* teki)
{
    if (!teki) return 0;
    // Family modules key their binding by the PelletView base; the seed binds
    // by generator uid. Either one marks a P2-bound actor.
    if (const unsigned source = pc_randomizer_p2_source_for(static_cast<PelletView*>(teki))) return source;
    const unsigned uid = generatorUid(teki);
    return uid ? pc_randomizer_p2_source_for_id(uid) : 0u;
}

unsigned pc_held_part_for_pellet_config(int pelletConfigIdx)
{
    if (!pelletMgr || pelletConfigIdx < 0) return 0;
    PelletConfig* config = pelletMgr->getConfigFromIdx(pelletConfigIdx);
    if (!config || !Pellet::isUfoPartsID(config->mModelId.mId)) return 0;
    return config->mModelId.mId;
}

void pc_held_part_log_assign(unsigned partId, unsigned source, unsigned target, int p1Boss)
{
    std::printf("P2_HELD_PART_ASSIGN part=%s source_id=%u target=%u p1_boss=%d via=arena\n", PartName(partId).s, source,
                target, p1Boss);
    std::fflush(stdout);
}

bool pc_held_part_birth(BTeki* teki)
{
    if (!teki || !teki->mPersonality) return false;
    ID32& id = teki->mPersonality->mID;
    const bool isPart = Pellet::isUfoPartsID(id.mId);
    if (!isPart) return false;
    const PartName partName(id.mId);
    const char* name = partName.s;
    const bool exists = partExists(id.mId);
    if (!p2heldpart::keepAtBirth(isPart, exists)) {
        std::printf("P2_HELD_PART_CLEAR part=%s generator=%u teki=%d source_id=%u reason=exists\n", name,
                    generatorUid(teki), int(teki->mTekiType), pc_held_part_p2_source(teki));
        std::fflush(stdout);
        id.setID('none');
        return false;
    }
    std::printf("P2_HELD_PART_HOLD part=%s generator=%u teki=%d source_id=%u\n", name, generatorUid(teki),
                int(teki->mTekiType), pc_held_part_p2_source(teki));
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
                    PartName(teki->mPersonality->mID.mId).s, generatorUid(teki), int(teki->mTekiType),
                    pc_held_part_p2_source(teki), double(teki->mHealth));
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
    const PartName name(id);
    if (drop == p2heldpart::Drop::AlreadyExists) {
        if (radarInfo) radarInfo->detachParts(teki);
        std::printf("P2_HELD_PART_SKIP part=%s generator=%u teki=%d reason=exists via=%s\n", name.s,
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
                name.s, generatorUid(teki), int(teki->mTekiType), pc_held_part_p2_source(teki),
                via ? via : "?", spawned ? 1 : 0, double(pos.x), double(pos.z));
    std::fflush(stdout);
    return spawned;
}

bool pc_held_part_transfers(unsigned heldId, int parameter0, const void* generator)
{
    if (!generator || parameter0 != 0 || !Pellet::isUfoPartsID(heldId) || !pc_randomizer_p2_bridge()) return false;
    return pc_randomizer_p2_source_for_id(pc_randomizer_generator_id(generator)) != 0;
}

void pc_held_part_ensure_shape(unsigned partId)
{
    if (!pelletMgr || !Pellet::isUfoPartsID(partId)) return;
    if (pelletMgr->pcEnsureShape(partId)) return;
    std::printf("P2_HELD_PART_SHAPE part=%s ok=0\n", PartName(partId).s);
    std::fflush(stdout);
}
