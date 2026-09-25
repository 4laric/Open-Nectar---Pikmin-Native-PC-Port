// Engine-free fixtures for pc_p2_uji_policy.h (UjiA 12 / UjiB 13 / Tobi 14).
// Build: g++ -std=gnu++17 -Wall -Wextra -Werror -Ipc_port tools/p2_uji_test.cpp -o p2_uji_test.exe
#include <cstdio>
#include <cstdlib>
#include "pc_p2_uji_policy.h"

using namespace p2uji_policy;

static int gChecks = 0;
static void require(bool ok, const char* what) {
    ++gChecks;
    if (!ok) {
        std::printf("FAIL p2_uji_test: %s\n", what);
        std::fflush(stdout);
        std::_Exit(1);
    }
}

static In sighted() {
    In in;
    in.health = 100.0f;
    in.targetInSight = true;
    return in;
}

int main() {
    // Identity: source ids, hosts, parms.
    require(sourceIdFor(UJIA) == 12, "UjiA source 12");
    require(sourceIdFor(UJIB) == 13, "UjiB source 13");
    require(sourceIdFor(TOBI) == 14, "Tobi source 14");
    require(hostTypeFor(UJIA) == 18, "UjiA host KabekuiA");
    require(hostTypeFor(UJIB) == 19, "UjiB host KabekuiB");
    require(hostTypeFor(TOBI) == 20, "Tobi host KabekuiC");
    require(parmsFor(UJIA).life == 100.0f, "UjiA retail life 100");
    require(parmsFor(UJIA).bridgeDamage == 25.0f, "UjiA retail fp01 25");

    // Stay -> Appear on sight (all kinds).
    for (int k = 0; k < 3; ++k) {
        Fsm fsm;
        Out out;
        Parms p = parmsFor((Kind)k);
        require(fsm.state == UJI_STAY, "spawn buried");
        require(fsm.tick(sighted(), p, (Kind)k, out), "sight reveals");
        require(fsm.state == UJI_APPEAR, "stay->appear");
        // Appear -> Move after appearTime (30 ticks).
        In idle;
        idle.health = 100.0f;
        for (int i = 0; i < 30; ++i) fsm.tick(idle, p, (Kind)k, out);
        require(fsm.state == UJI_MOVE, "appear completes to move");
    }

    // Move -> Attack1 in range; UjiA returns to Move, others chain.
    {
        Fsm fsm;
        Out out;
        Parms p = parmsFor(UJIA);
        fsm.state = UJI_MOVE;
        In in = sighted();
        in.targetInRange = true;
        fsm.tick(in, p, UJIA, out);
        require(fsm.state == UJI_ATTACK1, "UjiA engages attack1");
        in.targetInRange = false; // target leaves: attack recovers to move
        for (int i = 0; i < 40; ++i) fsm.tick(in, p, UJIA, out);
        require(fsm.state == UJI_MOVE, "UjiA attack1 recovers to move");
    }
    {
        Fsm fsm;
        Out out;
        Parms p = parmsFor(UJIB);
        fsm.state = UJI_MOVE;
        In in = sighted();
        in.targetInRange = true;
        fsm.tick(in, p, UJIB, out);
        require(fsm.state == UJI_ATTACK1, "UjiB engages attack1");
        for (int i = 0; i < 40; ++i) fsm.tick(in, p, UJIB, out);
        require(fsm.state == UJI_ATTACK2, "UjiB chains attack2");
        for (int i = 0; i < 40; ++i) fsm.tick(in, p, UJIB, out);
        require(fsm.state == UJI_EAT, "UjiB chains eat");
        in.targetInRange = false; // target leaves: eat recovers to move
        for (int i = 0; i < 70; ++i) fsm.tick(in, p, UJIB, out);
        require(fsm.state == UJI_MOVE, "UjiB eat recovers to move");
    }

    // Tobi flies after flyTime without a target, then lands.
    {
        Fsm fsm;
        Out out;
        Parms p = parmsFor(TOBI);
        fsm.state = UJI_MOVE;
        In idle;
        idle.health = 100.0f;
        for (int i = 0; i < 130; ++i) fsm.tick(idle, p, TOBI, out);
        require(fsm.state == UJI_FLY, "Tobi takes off");
        require(Fsm::clipFor(UJI_FLY, TOBI) != nullptr, "fly clip names");
        for (int i = 0; i < 130; ++i) fsm.tick(idle, p, TOBI, out);
        require(fsm.state == UJI_MOVE, "Tobi lands");
    }

    // GoHome/Dive/Stay when far from home.
    {
        Fsm fsm;
        Out out;
        Parms p = parmsFor(UJIA);
        fsm.state = UJI_MOVE;
        In in = sighted();
        in.farFromHome = true;
        fsm.tick(in, p, UJIA, out);
        require(fsm.state == UJI_GOHOME, "far move goes home");
        for (int i = 0; i < 130; ++i) fsm.tick(in, p, UJIA, out);
        require(fsm.state == UJI_DIVE, "gohome times out to dive");
        in.targetInSight = false; // no target: dive reburies and stays buried
        for (int i = 0; i < 40; ++i) fsm.tick(in, p, UJIA, out);
        require(fsm.state == UJI_STAY, "dive reburies");
    }

    // Death from any state mints the dead clip.
    {
        Fsm fsm;
        Out out;
        Parms p = parmsFor(UJIB);
        fsm.state = UJI_ATTACK2;
        In in;
        in.health = 0.0f;
        require(fsm.tick(in, p, UJIB, out), "death changes motion");
        require(fsm.state == UJI_DEAD, "health 0 kills");
        require(out.downEffect, "death drops effect flag");
    }

    std::printf("PASS p2_uji_test checks=%d\n", gChecks);
    return 0;
}
