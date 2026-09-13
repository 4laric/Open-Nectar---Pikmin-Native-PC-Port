#include "pc_p2_retail_player.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

static p2retail::Motion motion(const char* name, int duration,
                               std::initializer_list<p2retail::Event> events)
{
    p2retail::Motion result;
    result.name = name;
    result.duration = duration;
    result.events.assign(events.begin(), events.end());
    return result;
}

int main()
{
    const auto catchFly = motion("waitact2.bca", 50, {{0, 0}, {39, 1}});
    p2retail::Player player;
    assert(player.start(catchFly));
    for (int i = 0; i < 39; ++i)
        assert(player.advance(1.0f, [](p2retail::Event) {}) == p2retail::Update::Ok);
    assert(player.frame() == 39.0f);
    assert(player.advance(1.0f, [](p2retail::Event event) { assert(event.type == 1); }) == p2retail::Update::Ok);
    assert(player.frame() == 0.0f); // CatchFly loops until the host calls finishMotion.
    player.finishMotion();
    bool ended = false;
    for (int i = 0; i < 50; ++i)
        assert(player.advance(1.0f, [&](p2retail::Event event) {
            assert(event.type == 1 || event.type == 1000);
            ended |= event.type == 1000;
        }) == p2retail::Update::Ok);
    assert(ended && player.completed());

    const auto fallMeck = motion("waitact1.bca", 30, {{8, 2}, {19, 3}, {25, 4}});
    assert(player.start(fallMeck));
    bool released = false, fallEnded = false;
    for (int i = 0; i < 30; ++i)
        assert(player.advance(1.0f, [&](p2retail::Event event) {
            if (event.type == 3) {
                assert(player.frame() == 20.0f && !released);
                released = true;
            }
            if (event.type == 1000) fallEnded = true;
        }) == p2retail::Update::Ok);
    assert(released && fallEnded);
    std::puts("p2_demon_host_clock_test PASS");
}
