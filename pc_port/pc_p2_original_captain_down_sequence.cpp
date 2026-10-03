#include "pc_p2_original_captain_down_sequence.h"
#include "netplay/pc_netplay_sha256.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace p2original { namespace down {
namespace {
std::uint16_t be16(const unsigned char* p) { return (p[0] << 8) | p[1]; }
std::uint32_t be32(const unsigned char* p) {
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
           (std::uint32_t(p[2]) << 8) | p[3];
}
}
const char* Sequence::sourceSha256() {
    return "8053c63b356918a53777607a96798c33c79773ef0990a78de66b5853ba8f82e1";
}
std::shared_ptr<const Sequence> Sequence::authenticate(const void* bytes,
        std::size_t size, std::string& error) {
    error.clear();
    if (!bytes || size != 884) { error = "s03 STB member size mismatch"; return {}; }
    std::uint8_t hash[32]; pc_netplay_sha::sha256(bytes, size, hash);
    if (pc_netplay_sha::hex(hash, 32) != sourceSha256()) {
        error = "s03 STB SHA-256 mismatch"; return {};
    }
    const auto* b = static_cast<const unsigned char*>(bytes);
    if (std::memcmp(b, "STB\0", 4) || be16(b + 4) != 0xfeff ||
        be16(b + 6) != 3 || be32(b + 8) != size || be32(b + 12) != 11) {
        error = "unsupported authenticated STB header"; return {};
    }
    std::shared_ptr<Sequence> result(new Sequence);
    std::size_t block = 32;
    // Retail research: JSystem/JStudio/stb-data-parse.cpp:24 and stb.cpp:244.
    // MoviePlayer studio tick scale is 0.03333333507180214 (moviePlayer.cpp:437),
    // but the owner supplies actual studio ticks, not a guessed wall-clock timer.
    for (unsigned i = 0; i != 11; ++i) {
        if (block + 12 > size) { error = "truncated STB block"; return {}; }
        const std::size_t length = be32(b + block);
        const std::size_t idSize = be16(b + block + 10);
        const std::size_t content = block + 12 + ((idSize + 3) & ~std::size_t(3));
        if (length < 12 || length > size - block || content > block + length) {
            error = "invalid STB block bounds"; return {};
        }
        const std::string id(reinterpret_cast<const char*>(b + block + 12), idSize - 1);
        std::uint32_t tick = 0;
        std::size_t pos = content;
        bool ended = false;
        while (pos + 4 <= block + length) {
            const auto head = be32(b + pos);
            const auto type = head >> 24, param = head & 0xffffff;
            pos += 4;
            if (!type) { ended = true; break; }
            if (type == 2) { tick += param; continue; }
            if (type != 0x80 || param > block + length - pos) {
                error = "unsupported STB sequence instruction"; return {};
            }
            const auto end = pos + param;
            // This authenticated movie uses short-form paragraph headers only.
            while (pos + 4 <= end) {
                const auto n = be16(b + pos), tag = be16(b + pos + 2);
                pos += 4;
                if (n > end - pos) { error = "invalid STB paragraph"; return {}; }
                if (tag == 0x759 && n == 4) {
                    const auto command = be32(b + pos);
                    if (id == "+GameCamera") result->events_.push_back({Kind::CameraCommand, tick, command});
                    else if (id == "+MovieCommand") result->events_.push_back({Kind::SectionCommand, tick, command});
                    else if (id == "*orima") result->events_.push_back({command == 1 ? Kind::ActorDemoAnimation : Kind::ActorCommand, tick, command});
                }
                pos += (n + 3) & ~std::size_t(3);
            }
            if (pos != end) { error = "STB paragraph alignment mismatch"; return {}; }
        }
        if (!ended || tick != 207) { error = "STB exhaustion mismatch"; return {}; }
        result->ticks_ = std::max(result->ticks_, tick);
        block += length;
    }
    if (block != size) { error = "trailing STB bytes"; return {}; }
    std::stable_sort(result->events_.begin(), result->events_.end(),
        [](const Event& a, const Event& b) { return a.tick < b.tick; });
    return result;
}
Clock::Clock(std::shared_ptr<const Sequence> sequence) : sequence_(std::move(sequence)) {}
void Clock::emitThrough(std::uint32_t tick, std::vector<Event>& out) {
    while (nextEvent_ < sequence_->events().size() &&
           sequence_->events()[nextEvent_].tick <= tick)
        out.push_back(sequence_->events()[nextEvent_++]);
}
bool Clock::start(std::vector<Event>& out, std::string& error) {
    out.clear(); error.clear();
    if (!sequence_ || phase_ != Phase::Idle) { error = "missing sequence or already started clock"; return false; }
    phase_ = Phase::Studio; emitThrough(0, out); return true;
}
std::vector<Event> Clock::advanceStudio(std::uint32_t ticks) {
    std::vector<Event> out;
    if (phase_ != Phase::Studio || !ticks) return out;
    tick_ += std::min(ticks, sequence_->ticks() - tick_);
    emitThrough(tick_, out);
    if (tick_ == sequence_->ticks()) {
        out.push_back({Kind::StudioExhausted, tick_, 0});
        phase_ = Phase::Finishing; fadeRemaining_ = 2.0f;
    }
    return out;
}
bool Clock::advanceFinishing(float delta, std::vector<Event>& out, std::string& error) {
    out.clear(); error.clear();
    if (phase_ != Phase::Finishing || !std::isfinite(delta) || delta < 0.0f) {
        error = "invalid finishing update"; return false;
    }
    fadeRemaining_ -= delta;
    // Source moviePlayer.cpp:583-618. Do not merge these two branches.
    if (fadeRemaining_ < 1.1f && !callbackDelivered_) {
        callbackDelivered_ = true; out.push_back({Kind::FinishCallback, tick_, 0});
    } else if (fadeRemaining_ <= 0.0f) {
        phase_ = Phase::Inactive; out.push_back({Kind::Inactive, tick_, 0});
    }
    return true;
}
} }
