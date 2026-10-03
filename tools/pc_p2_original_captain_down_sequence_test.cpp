#include "pc_p2_original_captain_down_sequence.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <limits>
#include <iostream>
using namespace p2original::down;
int main(int argc, char** argv) {
    assert(argc == 2);
    std::ifstream in(argv[1], std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), {});
    std::string error; std::vector<Event> events;
    auto sequence = Sequence::authenticate(bytes.data(), bytes.size(), error);
    assert(sequence && error.empty() && sequence->ticks() == 207);
    assert(!Sequence::authenticate(nullptr, bytes.size(), error));
    assert(!Sequence::authenticate(bytes.data(), bytes.size() - 1, error));
    auto bad = bytes;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        bad = bytes; bad[i] ^= 1;
        assert(!Sequence::authenticate(bad.data(), bad.size(), error));
    }
    bad = bytes; bad.push_back(0);
    assert(!Sequence::authenticate(bad.data(), bad.size(), error));
    Clock missing(nullptr); assert(!missing.start(events, error));
    Clock clock(sequence); assert(clock.start(events, error));
    assert(events.size() == 2 && events[0].kind == Kind::CameraCommand && events[0].value == 101);
    assert(events[1].kind == Kind::ActorDemoAnimation && events[1].tick == 0);
    assert(!clock.start(events, error));
    assert(!clock.advanceFinishing(0.1f, events, error));
    assert(clock.advanceStudio(196).empty());
    events = clock.advanceStudio(1); assert(events.size() == 1 && events[0].kind == Kind::SectionCommand && events[0].value == 0);
    assert(clock.advanceStudio(7).empty());
    events = clock.advanceStudio(1); assert(events.size() == 1 && events[0].value == 105 && events[0].tick == 205);
    events = clock.advanceStudio(1); assert(events.size() == 1 && events[0].value == 104 && clock.phase() == Phase::Studio);
    events = clock.advanceStudio(1); assert(events.size() == 1 && events[0].kind == Kind::StudioExhausted);
    assert(clock.phase() == Phase::Finishing && clock.fadeRemaining() == 2.0f);
    assert(clock.advanceStudio(100).empty());
    assert(!clock.advanceFinishing(-1, events, error));
    assert(!clock.advanceFinishing(std::numeric_limits<float>::quiet_NaN(), events, error));
    assert(!clock.advanceFinishing(std::numeric_limits<float>::infinity(), events, error));
    assert(clock.fadeRemaining() == 2.0f);
    assert(clock.advanceFinishing(0.9f, events, error) && events.empty()); // exactly 1.1
    assert(clock.advanceFinishing(0.01f, events, error) && events.size() == 1 && events[0].kind == Kind::FinishCallback);
    assert(clock.advanceFinishing(0.01f, events, error) && events.empty());
    assert(clock.advanceFinishing(2.0f, events, error) && events.size() == 1 && events[0].kind == Kind::Inactive);
    assert(!clock.advanceFinishing(0, events, error));
    Clock batch(sequence); assert(batch.start(events, error));
    events = batch.advanceStudio(std::numeric_limits<std::uint32_t>::max());
    assert(events.size() == 4 && events.back().kind == Kind::StudioExhausted && batch.tick() == 207);
    assert(batch.advanceFinishing(3, events, error) && events.size() == 1 && events[0].kind == Kind::FinishCallback && batch.phase() == Phase::Finishing);
    assert(batch.advanceFinishing(0, events, error) && events.size() == 1 && events[0].kind == Kind::Inactive);
    std::cout << "PASS authenticated s03 parser, source boundaries, guards and one-shot events\n";
}
