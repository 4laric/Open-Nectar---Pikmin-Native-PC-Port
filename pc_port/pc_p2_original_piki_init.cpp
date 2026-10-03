#include "pc_p2_original_piki_init.h"
namespace {
thread_local PcOriginalPikiInitScope* activeScope = nullptr;
}
PcOriginalPikiInitScope::PcOriginalPikiInitScope(Piki* body) noexcept {
    if (!body || activeScope) return;
    mBody = body;
    mActive = true;
    activeScope = this;
}
PcOriginalPikiInitScope::~PcOriginalPikiInitScope() {
    if (mActive && activeScope == this) activeScope = nullptr;
}
bool pc_p2_original_piki_init_consume(Piki* body) noexcept {
    if (!body || !activeScope || !activeScope->mActive || activeScope->mConsumed
        || activeScope->mBody != body) return false;
    activeScope->mConsumed = true;
    return true;
}
