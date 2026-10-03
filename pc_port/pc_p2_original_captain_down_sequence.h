#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace p2original { namespace down {
// GPVE01 rev0 user/Mukki/movie/s03_orimadown/demo.szs:demo.stb.
// No scene, actor, controller, renderer or public authorization flag is owned here.
enum class Kind { CameraCommand, SectionCommand, ActorDemoAnimation,
                  ActorCommand, StudioExhausted, FinishCallback, Inactive };
// value is a parsed STB command (ActorDemoAnimation = command 1), not an
// animation ID/resource index. Presentation and actor user-data binding belong
// to the movie owner; JSND/light/particle paragraphs are not executed here.
struct Event { Kind kind; std::uint32_t tick; std::uint32_t value; };
class Sequence {
public:
    static std::shared_ptr<const Sequence> authenticate(const void* bytes,
        std::size_t size, std::string& error);
    static const char* sourceSha256();
    std::uint32_t ticks() const { return ticks_; }
    const std::vector<Event>& events() const { return events_; }
private:
    Sequence() = default;
    std::uint32_t ticks_ = 0;
    std::vector<Event> events_;
};
enum class Phase { Idle, Studio, Finishing, Inactive };
class Clock {
public:
    explicit Clock(std::shared_ptr<const Sequence> sequence);
    // Tick zero is emitted only here; a Clock can be started once.
    bool start(std::vector<Event>& emitted, std::string& error);
    std::vector<Event> advanceStudio(std::uint32_t ticks);
    // Matches MoviePlayer::update Finishing branch; callback and inactive are
    // distinct updates even when one delta crosses both thresholds.
    bool advanceFinishing(float deltaSeconds, std::vector<Event>& emitted,
                          std::string& error);
    Phase phase() const { return phase_; }
    std::uint32_t tick() const { return tick_; }
    float fadeRemaining() const { return fadeRemaining_; }
private:
    std::shared_ptr<const Sequence> sequence_;
    Phase phase_ = Phase::Idle;
    std::uint32_t tick_ = 0;
    std::size_t nextEvent_ = 0;
    float fadeRemaining_ = 0.0f;
    bool callbackDelivered_ = false;
    void emitThrough(std::uint32_t tick, std::vector<Event>& emitted);
};
} }
