#include "pc_p2_second_captain.h"

#include "NaviMgr.h"

#include <cstdio>
#include <cstdlib>

// Lane 12 opt-in second-captain creation (#130). See the header for why the
// live spawn is gated shut on this port.

namespace pc_p2_captain {

bool second_captain_requested()
{
    const char* env = std::getenv("PIKMIN_P2_SECOND_CAPTAIN");
    if (!env || env[0] == '\0') return false;
    if (env[0] == '0' && env[1] == '\0') return false;
    return true;
}

bool second_captain_live_allowed()
{
    // Closed on purpose. A live second Navi requires, at minimum:
    //   * per-captain follow/whistle AI and split-squad control routing,
    //   * a per-captain camera and Kontroller binding (Navi ctor already makes
    //     Kontroller(naviID + 1); P2 input mapping is not ported),
    //   * survivor-gated GAMEEND_NaviDown (naviState.cpp:3190 sets a global),
    //   * HUD/AI consumers to use getActiveNavi() instead of getNavi().
    // Shipping the additive helpers and closed spawn path keeps default play
    // byte-identical while a later slice ports those systems.
    return false;
}

int navi_capacity()
{
    return (second_captain_requested() && second_captain_live_allowed()) ? 2 : 1;
}

bool prepare_second_captain_assets(NaviMgr* mgr)
{
    if (!mgr) return false;
    if (!second_captain_live_allowed()) return false;
    return mgr->ensureSecondNaviShapeObject();
}

Navi* birth_second_captain(NaviMgr* mgr)
{
    if (!mgr || !second_captain_live_allowed()) return nullptr;
    if (!mgr->ensureSecondNaviShapeObject()) return nullptr;
    // NaviMgr::create(2) already constructed the second Navi object; birth()
    // simply activates the next free slot.
    Navi* navi = static_cast<Navi*>(mgr->birth());
    if (navi) {
        std::printf("[Pikmin Randomizer] second captain spawned at slot %d\n", navi->getNaviIndex());
        std::fflush(stdout);
    }
    return navi;
}

} // namespace pc_p2_captain
