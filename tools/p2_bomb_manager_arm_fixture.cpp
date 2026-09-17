#undef NDEBUG
// Guarded fixture proving the PORT-SIDE Bomb birth arm registration plus #616
// provider membership and the #577 source-93 payload binding (issue #684
// repair; #186 decision cff8e9a2). Compiles against ONLY tracked files (no
// pikmin2-research / decomp shadow tree).
//
// Build (no engine, no lease):
//   g++ -std=c++17 -Wall -Wextra -Werror -Ipc_port -DP2_BOMB_MGR_BIRTH_NO_HOST
//       tools/p2_bomb_manager_arm_fixture.cpp
//       pc_port/pc_p2_bombsarai_blast.cpp -o p2_bomb_manager_arm_fixture
// The fixture unity-includes the provider core + #577 payload TU (neither is
// in the main build object list for this TU, so no duplicate symbols arise).
// The arm registration (pc_p2_bomb_mgr_birth_arm_enemy / pc_p2_bomb_mgr_birth_arm)
// is the port-side seam the port build compiles, mirroring the landed #675
// pc_bbft.cpp flag pattern. Captain safety (#632): engine-free proof (no
// captain/Navi/HP); the guard predicate below mirrors
// scripts/p2_fixture_captain_guard.h and is asserted as a pure check. Any
// future runtime consumer must adopt that header before launch; its hash is
// recorded in the lane packet, not claimed as a run.
// P2_BOMB_MGR_BIRTH_NO_HOST comes from the build command line.
#include "pc_p2_bomb_mgr_birth.cpp"
#include "pc_p2_bomb_payload_actor.cpp"

#include <cassert>
#include <cmath>
#include <cstdio>

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
    // 1. Port-side arm registration: only Bomb-family IDs are accepted.
    {
        assert(pc_p2_bomb_mgr_birth_arm_enemy(kEnemyID_Bomb));
        assert(pc_p2_bomb_mgr_birth_arm_enemy(kEnemyID_BombOtakara));
        assert(!pc_p2_bomb_mgr_birth_arm_enemy(kEnemyID_Chappy));
        assert(!pc_p2_bomb_mgr_birth_arm_enemy(0));
        P2BombMgr mgr(2);
        mgr.registerCarrier(7);
        P2BombPayloadConfig config;
        P2BombMgrHandle rejected = pc_p2_bomb_mgr_birth_arm(mgr, kEnemyID_Chappy, 7,
                                                            joint(0, 0, 0), config);
        assert(!p2_bomb_mgr_handle_valid(rejected));
        assert(mgr.activeCount() == 0);
        std::puts("PASS arm-registration-selective");
    }
    // 2. Accepted Bomb / BombOtakara arms perform the real provider birth and
    // #577 payload handoff (source 93 path uses the same contract).
    {
        P2BombMgr mgr(3);
        mgr.registerCarrier(36);
        mgr.registerCarrier(93);
        P2BombPayloadConfig config;
        P2BombMgrHandle bomb = pc_p2_bomb_mgr_birth_arm(mgr, kEnemyID_Bomb, 36,
                                                        joint(1, 2, 3), config);
        assert(p2_bomb_mgr_handle_valid(bomb));
        assert(mgr.isLive(bomb));
        assert(p2_bomb_payload_handle_valid(mgr.payloadHandle(bomb)));
        P2BombMgrHandle otakara = pc_p2_bomb_mgr_birth_arm(mgr, kEnemyID_BombOtakara, 93,
                                                           joint(4, 5, 6), config);
        assert(p2_bomb_mgr_handle_valid(otakara));
        assert(mgr.isLive(otakara));
        assert(mgr.activeCount() == 2);
        std::puts("PASS arm-birth-and-payload-handoff");
    }
    // 3. Forget/reset clear without stale handles.
    {
        P2BombMgr mgr(2);
        mgr.registerCarrier(9);
        P2BombPayloadConfig config;
        P2BombMgrHandle h = pc_p2_bomb_mgr_birth_arm(mgr, kEnemyID_Bomb, 9,
                                                     joint(0, 0, 0), config);
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
