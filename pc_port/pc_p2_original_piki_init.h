#pragma once
class Piki;

// Original source birth only. Hold across precisely one physical Piki::init.
// A nested, null or already-consumed route cannot suppress host RNG draws.
class PcOriginalPikiInitScope {
public:
    explicit PcOriginalPikiInitScope(Piki* body) noexcept;
    ~PcOriginalPikiInitScope();
    PcOriginalPikiInitScope(const PcOriginalPikiInitScope&) = delete;
    PcOriginalPikiInitScope& operator=(const PcOriginalPikiInitScope&) = delete;
    bool valid() const noexcept { return mActive; }
    bool consumed() const noexcept { return mConsumed; }
private:
    friend bool pc_p2_original_piki_init_consume(Piki*) noexcept;
    Piki* mBody = nullptr;
    bool mActive = false;
    bool mConsumed = false;
};
bool pc_p2_original_piki_init_consume(Piki* body) noexcept;
