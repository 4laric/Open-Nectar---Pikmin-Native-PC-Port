#undef NDEBUG
// Guarded fixture proving the REAL engine Bomb birth arm plus #616 provider
// membership and the #577 source-93 payload binding (issue #684).
//
// Build (no engine, no lease):
//   g++ -std=c++17 -Wall -Wextra -Werror -Ipc_port -DP2_BOMB_MGR_BIRTH_NO_HOST
//       -DP2_BOMB_BIRTH_ARM_STANDALONE
//       tools/p2_bomb_manager_arm_fixture.cpp
//       pikmin2-research/src/plugProjectYamashitaU/generalEnemyMgr.cpp
//       pc_port/pc_p2_bombsarai_blast.cpp -o p2_bomb_manager_arm_fixture
// This compiles and runs the VERBATIM GeneralEnemyMgr::createEnemyMgr Bomb /
// BombOtakara dispatch arms from the owned engine-surface file (not a
// reimplementation): the real switch on real source IDs routes to the real
// provider notifier. Simulation of the arms is explicitly forbidden here.
// Captain safety (#632): engine-free proof (no captain/Navi/HP); the guard
// predicate below mirrors scripts/p2_fixture_captain_guard.h and is asserted
// as a pure check. Any future runtime consumer must adopt that header before
// launch; its hash is recorded in the lane packet, not claimed as a run.
#define P2_BOMB_MGR_BIRTH_NO_HOST
#include "pc_p2_bomb_mgr_birth.cpp"
#include "pc_p2_bomb_payload_actor.cpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

// Forward declaration matching the owned engine-surface TU (standalone mode
// provides the stub GeneralEnemyMgr whose createEnemyMgr is the verbatim
// dispatch). Defined in generalEnemyMgr.cpp; linked, not reimplemented.
class GeneralEnemyMgr {
public:
    static void createEnemyMgr(unsigned char viewNum, int enemyID, int limit);
};

// ---- Strong engine-hook notifier (the arms call this directly) ----
static std::vector<int> g_hook_notifications;

void pc_p2_bomb_birth_hook_notify(int enemyID)
{
    g_hook_notifications.push_back(enemyID);
    std::printf("P2_BOMB_BIRTH_HOOK_NOTIFY enemyID=%d\n", enemyID);
    std::fflush(stdout);
}

// Real source IDs (Game/enemyInfo.h): Bomb 36, BombOtakara 93.
static const int kEnemyID_Bomb = 36;
static const int kEnemyID_BombOtakara = 93;
static const int kEnemyID_Chappy = 2;

static P2BombSaraiVec3 joint(float x, float y, float z)
{
    P2BombSaraiVec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

// Captain-guard predicate mirror (scripts/p2_fixture_captain_guard.h).
static bool captain_down(bool orimaDead, bool deadState, float hp)
{
    return orimaDead || deadState || !std::isfinite(hp) || hp <= 1.0f;
}

int main()
{
    // 1. REAL dispatch: verbatim createEnemyMgr arms route Bomb-family IDs
    // to the notifier; all other IDs stay dark.
    {
        GeneralEnemyMgr::createEnemyMgr(0, kEnemyID_Bomb, 8);
        GeneralEnemyMgr::createEnemyMgr(0, kEnemyID_BombOtakara, 8);
        GeneralEnemyMgr::createEnemyMgr(0, kEnemyID_Chappy, 8);
        assert(g_hook_notifications.size() == 2);
        assert(g_hook_notifications[0] == kEnemyID_Bomb);
        assert(g_hook_notifications[1] == kEnemyID_BombOtakara);
        std::puts("PASS real-arm-dispatch");
    }
    // 2. Provider membership + birth + #577 payload handoff (source 93 path
    // uses the same provider birth + payload handle contract).
    {
        P2BombMgr mgr(2);
        P2BombPayloadConfig config;
        P2BombMgrHandle bad = mgr.birth(7, joint(0, 0, 0), config);
        assert(!p2_bomb_mgr_handle_valid(bad));
        mgr.registerCarrier(7);
        P2BombMgrHandle h = mgr.birth(7, joint(1, 2, 3), config);
        assert(p2_bomb_mgr_handle_valid(h));
        assert(mgr.isLive(h));
        assert(p2_bomb_payload_handle_valid(mgr.payloadHandle(h)));
        assert(mgr.findLive(7).slot == h.slot);
        std::puts("PASS birth-and-payload-handoff");
    }
    // 3. Forget/reset clear without stale handles.
    {
        P2BombMgr mgr(2);
        mgr.registerCarrier(9);
        P2BombPayloadConfig config;
        P2BombMgrHandle h = mgr.birth(9, joint(0, 0, 0), config);
        assert(p2_bomb_mgr_handle_valid(h));
        assert(mgr.onCarrierGone(h));
        assert(!mgr.isLive(h));
        mgr.reset();
        assert(mgr.activeCount() == 0);
        assert(mgr.registeredCount() == 0);
        std::puts("PASS forget-reset-clean");
    }
    // 4. Captain-guard predicate mirror (no captain here; pure check).
    {
        assert(!captain_down(false, false, 100.0f));
        assert(captain_down(true, false, 100.0f));
        assert(captain_down(false, true, 100.0f));
        assert(captain_down(false, false, 1.0f));
        assert(captain_down(false, false, 0.0f));
        assert(captain_down(false, false, std::nanf("")));
        std::puts("PASS captain-guard-mirror");
    }
    std::puts("ALL_FIXTURE_PASS");
    return 0;
}
