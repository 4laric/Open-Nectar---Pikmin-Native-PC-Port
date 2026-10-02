#pragma once
#include <set>
#include <utility>
#include <cstddef>

// A prepare/commit pair spans the existing kill call. Committing consumes the
// token, so replay cannot damage twice and a recycled actor address can be
// prepared again during a later, live mouth event.
class P2WhitePoisonEvents {
public:
    bool prepare(const void* predator, const void* victim) {
        if (!predator || !victim) return false;
        return mPending.insert(std::make_pair(predator, victim)).second;
    }
    bool commit(const void* predator, const void* victim) {
        return mPending.erase(std::make_pair(predator, victim)) == 1;
    }
    void forgetPredator(const void* predator) {
        for (auto it = mPending.begin(); it != mPending.end();) {
            if (it->first == predator) it = mPending.erase(it); else ++it;
        }
    }
    void reset() { mPending.clear(); }
    std::size_t pendingCount() const { return mPending.size(); }
    // Copies pending logical tokens under the agreed save fence; does not consume them.
    std::set<std::pair<const void*, const void*>> capturePending() const { return mPending; }
private:
    std::set<std::pair<const void*, const void*>> mPending;
};
