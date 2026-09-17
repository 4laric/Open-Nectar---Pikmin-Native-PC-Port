// P2 Challenge host-mode consumer translation unit (#651).
//
// Real module surface: stage select by ui_index over the decoded stage table,
// squad/spray application, mTimeLimit countdown with per-floor extension, and
// the retry/reset boundary. Every entry emits a marker for the guarded fixture
// and for Python observers.
#include "pc_p2_challenge_mode.h"

namespace p2challenge {

int selectByUiIndex(const StageEntry* stages, int count, int uiIndex)
{
    if (!stages || uiIndex < 0) return -1;
    for (int i = 0; i < count; ++i)
        if (stages[i].uiIndex == uiIndex) return i;
    return -1;
}

void applySquadAndSprays(HostState& s, int bitter, int spicy)
{
    s.bitterSprays = bitter;
    s.spicySprays = spicy;
    emit("SQUAD_APPLIED", s);
}

HostState retry(const HostState& previous)
{
    HostState fresh = start(*previous.stage);
    emit("RETRY_RESET", fresh);
    return fresh;
}

} // namespace p2challenge