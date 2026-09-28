#pragma once

// Engine-free Uji family policy: Female Sheargrub (UjiA, 12), Male Sheargrub
// (UjiB, 13), Shearwig (Tobi, 14). Source: Ujia/Ujib/TobiState.cpp +
// Ujia.h/Ujib.h/Tobi.h StateID enums in native/pikmin2-research
// (plugProjectNishimuraU). Retail general parms from EnemyParmsBase
// (life 100, move 80, territory 200, sight 200); UjiA proper fp01 bridge
// damage 25 (Ujia.h:118). Species enemyparm.txt overrides are preserved on
// disc (experimental/pikmin2_uji_assets.py) but not re-parsed here; the
// per-species defaults below are the retail EnemyParmsBase defaults with the
// documented species tweaks (male/flying toughness, fly speed), all marked
// as port values where they differ from a measured retail number.
namespace p2uji_policy {

enum Kind { UJIA = 0, UJIB = 1, TOBI = 2 };

enum State {
    UJI_DEAD = 0,
    UJI_STAY = 2,
    UJI_APPEAR = 3,
    UJI_DIVE = 4,
    UJI_MOVE = 5,
    UJI_GOHOME = 9,
    UJI_ATTACK1 = 10,
    UJI_ATTACK2 = 11,
    UJI_EAT = 12,
    UJI_FLY = 13,
};

struct Parms {
    float life = 100.0f;
    float moveSpeed = 80.0f;
    float sight = 200.0f;
    float territory = 200.0f;
    float homeRadius = 100.0f;
    float appearTime = 1.0f;   // port value (source plays the appear motion)
    float attackTime = 1.2f;   // port value (source ends on motion end)
    float attackRange = 60.0f; // port value (bridge-bite reach)
    float bridgeDamage = 25.0f; // retail Ujia proper fp01
    float flyTime = 4.0f;       // port value (Tobi airborne window)
    float eatTime = 2.0f;       // port value (UjiB/Tobi Eat carry window)
};

inline Parms parmsFor(Kind kind) {
    Parms p;
    if (kind == UJIB) {
        p.life = 120.0f; // port value: male tougher than female
    } else if (kind == TOBI) {
        p.life = 150.0f;      // port value: flying shearwig tougher
        p.moveSpeed = 120.0f;  // port value: Tobi fly speed
    }
    return p;
}

inline int sourceIdFor(Kind kind) {
    return kind == UJIA ? 12 : kind == UJIB ? 13 : 14;
}

inline int hostTypeFor(Kind kind) {
    return kind == UJIA ? 18 : kind == UJIB ? 19 : 20; // TEKI_KabekuiA/B/C
}

struct In {
    float health = 100.0f;
    bool targetInSight = false;
    bool targetInRange = false;
    bool farFromHome = false;
};

struct Out {
    bool motionChanged = false;
    bool downEffect = false;
};

// Header-only FSM: Stay (buried) -> Appear -> Move -> Attack1 (-> Attack2 ->
// Eat for UjiB; Fly loop for Tobi) -> Move; Dive/GoHome when far from home;
// Dead on health 0. Transcribes UjiaState.cpp / UjibState.cpp (+Attack2/Eat)
// / TobiState.cpp (+Fly).
class Fsm {
  public:
    State state = UJI_STAY;
    float stateTime = 0.0f;

    void reset() {
        state = UJI_STAY;
        stateTime = 0.0f;
    }

    // Returns true when the motion clip changes (caller switches bank clip).
    bool tick(const In& in, const Parms& parms, Kind kind, Out& out) {
        out.motionChanged = false;
        out.downEffect = false;
        if (in.health <= 0.0f && state != UJI_DEAD) {
            state = UJI_DEAD;
            stateTime = 0.0f;
            out.motionChanged = true;
            out.downEffect = true;
            return true;
        }
        if (state == UJI_DEAD) return false;
        stateTime += 1.0f / 30.0f; // fixed-step tick (30 fps retail clock)
        switch (state) {
        case UJI_STAY:
            if (in.targetInSight) {
                state = UJI_APPEAR;
                stateTime = 0.0f;
                out.motionChanged = true;
                return true;
            }
            return false;
        case UJI_APPEAR:
            if (stateTime >= parms.appearTime) {
                state = UJI_MOVE;
                stateTime = 0.0f;
                out.motionChanged = true;
                return true;
            }
            return false;
        case UJI_MOVE:
            if (in.farFromHome) {
                state = UJI_GOHOME;
                stateTime = 0.0f;
                out.motionChanged = true;
                return true;
            }
            if (in.targetInRange) {
                state = UJI_ATTACK1;
                stateTime = 0.0f;
                out.motionChanged = true;
                return true;
            }
            if (kind == TOBI && stateTime >= parms.flyTime) {
                state = UJI_FLY;
                stateTime = 0.0f;
                out.motionChanged = true;
                return true;
            }
            return false;
        case UJI_GOHOME:
            if (!in.farFromHome) {
                state = UJI_MOVE;
                stateTime = 0.0f;
                out.motionChanged = true;
                return true;
            }
            if (stateTime >= parms.flyTime) {
                state = UJI_DIVE;
                stateTime = 0.0f;
                out.motionChanged = true;
                return true;
            }
            return false;
        case UJI_DIVE:
            if (stateTime >= parms.appearTime) {
                state = UJI_STAY;
                stateTime = 0.0f;
                out.motionChanged = true;
                return true;
            }
            return false;
        case UJI_ATTACK1:
            if (stateTime >= parms.attackTime) {
                if (kind == UJIA) {
                    state = UJI_MOVE;
                } else {
                    state = UJI_ATTACK2;
                }
                stateTime = 0.0f;
                out.motionChanged = true;
                return true;
            }
            return false;
        case UJI_ATTACK2:
            if (stateTime >= parms.attackTime) {
                state = (kind == UJIA) ? UJI_MOVE : UJI_EAT;
                stateTime = 0.0f;
                out.motionChanged = true;
                return true;
            }
            return false;
        case UJI_EAT:
            if (stateTime >= parms.eatTime) {
                state = UJI_MOVE;
                stateTime = 0.0f;
                out.motionChanged = true;
                return true;
            }
            return false;
        case UJI_FLY:
            if (stateTime >= parms.flyTime) {
                state = UJI_MOVE;
                stateTime = 0.0f;
                out.motionChanged = true;
                return true;
            }
            if (in.targetInRange) {
                state = UJI_ATTACK1;
                stateTime = 0.0f;
                out.motionChanged = true;
                return true;
            }
            return false;
        default:
            return false;
        }
    }

    static const char* clipFor(State s, Kind kind) {
        switch (s) {
        case UJI_DEAD: return "dead";
        case UJI_STAY: return "dive";
        case UJI_APPEAR: return "appear";
        case UJI_DIVE: return "dive";
        case UJI_MOVE: return "move";
        case UJI_GOHOME: return "move";
        case UJI_ATTACK1: return "attack1";
        case UJI_ATTACK2: return "attack2";
        case UJI_EAT: return "eat";
        case UJI_FLY: return kind == TOBI ? "fly" : "move";
        default: return "move";
        }
    }
};

} // namespace p2uji_policy
