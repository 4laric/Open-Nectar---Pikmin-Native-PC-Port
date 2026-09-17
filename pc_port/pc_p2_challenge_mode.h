# pragma once
// P2 Challenge host-mode consumer module (#651). Header-inline so the fixture
// can use it without extra CMake objects; shared CMake/CTest wiring still
// requires #186 review.
#include <cstdio>

namespace p2challenge {

constexpr float kTimeLimitEpsilon = 0.0f;

struct StageEntry {
    const char* caveId;
    int uiIndex;
    int floorCount;
    float floorSeconds[8];
    int roster[7][3];
    int bitterSprays;
    int spicySprays;
};

struct HostState {
    const StageEntry* stage;
    int floorIndex;
    float timeLeft;
    int population;
    int pokos;
    int bitterSprays;
    int spicySprays;
    bool ended;
    const char* endState; // "timeout"|"extinction"|"give_up"|"captain_down"|nullptr
};

int selectByUiIndex(const StageEntry* stages, int count, int uiIndex);
void applySquadAndSprays(HostState& s, int bitter, int spicy);
HostState retry(const HostState& previous);

inline HostState start(const StageEntry& stage)
{
    HostState s{};
    s.stage = &stage;
    s.floorIndex = 0;
    s.timeLeft = stage.floorSeconds[0];
    s.population = 0;
    for (int c = 0; c < 7; ++c)
        for (int h = 0; h < 3; ++h) s.population += stage.roster[c][h];
    s.pokos = 0;
    s.bitterSprays = stage.bitterSprays;
    s.spicySprays = stage.spicySprays;
    s.ended = false;
    s.endState = nullptr;
    return s;
}

inline void tick(HostState& s, float dt)
{
    if (s.ended) return;
    s.timeLeft -= dt;
    if (s.timeLeft <= kTimeLimitEpsilon) {
        s.timeLeft = 0.0f;
        s.ended = true;
        s.endState = "timeout";
    }
}

inline bool descend(HostState& s)
{
    if (s.ended) return false;
    const int next = s.floorIndex + 1;
    if (next >= s.stage->floorCount) return false;
    s.floorIndex = next;
    s.timeLeft = s.stage->floorSeconds[next];
    return true;
}

inline void end(HostState& s, const char* reason)
{
    if (s.ended) return;
    s.ended = true;
    s.endState = reason;
}

inline int score(const HostState& s)
{
    return s.pokos * 10 + static_cast<int>(s.timeLeft) + s.population * 10;
}

inline void emit(const char* what, const HostState& s)
{
    std::printf("P2_CHALLENGE_MODE_%s cave=%s ui_index=%d floor=%d time_left=%.3f "
                "population=%d pokos=%d bitter=%d spicy=%d end=%s score=%d\n",
                what, s.stage->caveId, s.stage->uiIndex, s.floorIndex, s.timeLeft,
                s.population, s.pokos, s.bitterSprays, s.spicySprays,
                s.endState ? s.endState : "none", score(s));
    std::fflush(nullptr);
}

} // namespace p2challenge