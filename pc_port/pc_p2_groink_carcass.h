#pragma once
#include <array>
#include <cstddef>

struct P2GroinkCarcassConfig {
    float gaugeDelay = 0;
    float recoverySeconds = 1;
    float maxHealth = 1;
};
enum class P2GroinkCarcassCommand { ActivateGauge, KillPellet, RequestBirth, DeactivateGauge };
struct P2GroinkCarcassStep {
    bool valid = false;
    std::array<P2GroinkCarcassCommand,2> commands{};
    std::size_t count = 0;
};
class P2GroinkCarcass {
public:
    bool become(const P2GroinkCarcassConfig& config);
    void reset();
    P2GroinkCarcassStep step(float delta, bool pelletAlive, bool gaugeManager, bool activeTick=true);
    bool ready() const { return ready_; }
    float timer() const { return timer_; }
    float health() const { return health_; }
private:
    P2GroinkCarcassConfig config_{};
    bool ready_ = false;
    float timer_ = 0, health_ = 0;
};
