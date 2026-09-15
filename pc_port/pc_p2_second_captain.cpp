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

// Lane 12 (#130): the live spawn gate is now open (request-gated only). The
// second captain's model/shadow/plate/cursor render (Navi::refresh for
// mNaviID != 0) and the inactive-captain Kontroller skip are in place, so a
// requested second captain no longer ships as an invisible/input-mirroring
// actor. Default single-captain play never reaches navi_capacity() > 1 because
// second_captain_requested() is false unless PIKMIN_P2_SECOND_CAPTAIN is set.
bool second_captain_live_allowed()
{
    return true;
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
    Navi* first = mgr->getNavi();
    if (!first) return nullptr;
    // NaviMgr::create(2) already constructed the second Navi object; birth()
    // simply activates the next free slot. The full live setup (init, reset,
    // shared camera, spawn placement) is finished by GameCoreSection::finalSetup
    // once the stage managers all exist (effectMgr, mapMgr, etc.), matching how
    // the first captain is initialised.
    Navi* navi = static_cast<Navi*>(mgr->birth());
    if (navi) {
        std::printf("[Pikmin Randomizer] second captain spawned at slot %d\n", navi->getNaviIndex());
        std::fflush(stdout);
    }
    return navi;
}

} // namespace pc_p2_captain
