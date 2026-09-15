#include "pc_p2_generated_placement.h"
#include "pc_p2_sarai_manager.h"
#include "teki.h"
#include <cstdio>

bool pc_p2_generated_placement_bind(BTeki* actor, unsigned sourceId, unsigned seedTargetUid, unsigned generatorId)
{
    if (!actor || !sourceId) return false;
    switch (sourceId) {
    case 23: // Swooping Snitchbug (Sarai); lane 30.
        if (pc_p2_sarai_manager_bind_dynamic(actor, generatorId, seedTargetUid)) {
            std::printf("P2_GENERATED_PLACEMENT source_id=23 target=%u bound=1\n", seedTargetUid);
            std::fflush(stdout);
            return true;
        }
        return false;
    default:
        return false;
    }
}
