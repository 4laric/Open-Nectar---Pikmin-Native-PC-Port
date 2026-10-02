// Standalone helper/clock/parser regression, no engine/game/SDL runtime.
// Invoke after owner admission in an exclusive private build environment.
#include "p2_coop_fixture_input_snapshot.h"
#include "p2_coop_fixture_step.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

static void require(bool ok, const char* reason) {
    if (!ok) { std::fprintf(stderr, "FAIL %s\n", reason); std::exit(1); }
}
static PcCoopGenerationDecision offer(PcCoopGenerationState& state,
    const char* text, unsigned long long seq, unsigned long long now) {
    return pc_coop_fixture_generation_accept(&state, text, unsigned(std::strlen(text)), seq, now, 1u << 21);
}
static void effective(PcCoopGenerationState& state, unsigned long long now, unsigned expected) {
    unsigned buttons = 99; int axes[4] = {};
    require(pc_coop_fixture_generation_effective(&state, now, &buttons, axes), "effective clock");
    require(buttons == expected, "effective buttons");
    if (!expected) for (int axis : axes) require(!axis, "expired/neutral axes");
}

static void step_tests() {
    const std::string epoch(32, 'a');
    auto packet = [&](unsigned seq, unsigned tick, unsigned frame, unsigned command, int role = 0) {
        return "STEP1 " + std::to_string(seq) + " " + std::to_string(tick) + " " + epoch
            + " " + std::to_string(role) + " " + std::to_string(frame) + " " + std::to_string(command) + " END\n";
    };
    auto apply = [&](PcCoopStepState& state, const std::string& text, unsigned seq, unsigned now, unsigned frame, unsigned command) {
        return pc_coop_step_accept(state, text.data(), unsigned(text.size()), seq, now, epoch, 0, frame, command);
    };
    // Emulate waitStep's two independent clocks: a large boot-origin offset
    // must not consume the SDL elapsed budget, but still governs grant freshness.
    for (unsigned long long offset : {0ull, 864000000ull}) {
        PcCoopStepState origin;
        const auto text = std::string("STEP1 1 ") + std::to_string(offset + 100) + " " + epoch + " 0 285 66 END\n";
        require(pc_coop_step_elapsed_ok(101,100,0,284)
            && pc_coop_step_accept(origin,text.data(),unsigned(text.size()),1,offset+101,epoch,0,284,66)==PC_COOP_STEP_RELEASE,
            "healthy post-read gate independent SDL/boot clock origins");
    }
    require(pc_coop_step_elapsed_ok(599,100,0,284), "499ms SDL elapsed allowed");
    require(!pc_coop_step_elapsed_ok(600,100,0,284), "500ms SDL post-read deadline refuses");
    require(!pc_coop_step_elapsed_ok(99,100,0,284), "SDL clock regression refuses");
    require(!pc_coop_step_elapsed_ok(60000,59999,0,284), "60s overall post-read refuses");
    require(!pc_coop_step_elapsed_ok(101,100,0,5000), "5000 frames elapsed guard refuses");
    PcCoopStepState state;
    require(apply(state, packet(1,100,285,66),1,100,284,66) == PC_COOP_STEP_RELEASE, "first exact next frame");
    require(apply(state, packet(1,100,285,66),1,101,285,67) == PC_COOP_STEP_WAIT, "spent permit waits even with newer SDL input");
    require(state.published_ms == 100 && state.frame == 285, "duplicate never regrants or refreshes");
    require(apply(state, packet(1,101,285,66),1,102,285,66) == PC_COOP_STEP_INVALID, "mutated duplicate refusal");
    require(apply(state, packet(2,102,286,67),2,102,285,66) == PC_COOP_STEP_INVALID, "permit command must equal selected SDL sequence");
    require(apply(state, packet(3,102,286,66),3,102,285,66) == PC_COOP_STEP_INVALID, "permit sequence contiguous");
    require(apply(state, packet(2,102,287,66),2,102,285,66) == PC_COOP_STEP_INVALID, "cannot grant two future frames");
    require(apply(state, packet(2,102,285,66),2,102,285,66) == PC_COOP_STEP_INVALID, "past/current frame refusal");
    require(apply(state, packet(2,102,286,66,1),2,102,285,66) == PC_COOP_STEP_INVALID, "foreign role refusal");
    auto foreign = packet(2,102,286,66);foreign.replace(foreign.find(epoch),32,std::string(32,'b'));
    require(apply(state, foreign,2,102,285,66) == PC_COOP_STEP_INVALID, "foreign epoch refusal");
    require(apply(state, packet(2,103,286,66),2,102,285,66) == PC_COOP_STEP_INVALID, "future clock refusal");
    require(apply(state, packet(2,102,286,66),2,603,285,66) == PC_COOP_STEP_INVALID, "expired unread grant refusal");
    require(apply(state, packet(2,102,286,66),2,101,285,66) == PC_COOP_STEP_INVALID, "regressing reader refusal");
    require(apply(state, packet(2,102,286,66),1,102,285,66) == PC_COOP_STEP_INVALID, "filename identity refusal");
    require(apply(state, packet(2,102,286,66)+"EXTRA",2,102,285,66) == PC_COOP_STEP_INVALID, "full EOF canonical refusal");
    auto crlf=packet(2,102,286,66);crlf.insert(crlf.size()-1,"\r");
    require(apply(state,crlf,2,102,285,66) == PC_COOP_STEP_INVALID, "CRLF refusal");
    require(apply(state,packet(2,102,286,66),2,102,286,66) == PC_COOP_STEP_INVALID, "missed logical frame hard refusal");
    require(apply(state,packet(2,102,286,66),2,602,285,66) == PC_COOP_STEP_RELEASE, "500ms inclusive freshness boundary");
    require(apply(state,packet(2,102,286,66),2,1000,286,66) == PC_COOP_STEP_WAIT, "spent packet cannot buy more progress after expiry");
    require(!pc_coop_step_epoch("a") && !pc_coop_step_epoch(std::string(32,'A')), "strict epoch grammar");
    require(apply(state,packet(3,1000,5000,66),3,1000,4999,66) == PC_COOP_STEP_INVALID, "frame ceiling");
    std::puts("PASS 28 standalone step permit/clock-origin controls; no engine/PAD/runtime acceptance");
}

static void queue_tests() {
    unsigned char neutral[32] = {}, axis[16] = {}; axis[2] = 20;
    auto ready = [&]() {
        PcCoopQueueObserver q; q.enabled = true; q.role = 1;
        q.submit(284,1,neutral); q.submit(285,1,neutral); q.submit(286,1,neutral);
        q.advance(284,1,287,2,8,false,false,false,false,neutral); return q;
    };
    auto q=ready(); require(q.complete(284) && q.future_neutral(), "complete exact neutral future queue");
    q.slots[285%16].wire[2]=20;
    require(q.complete(284) && !q.future_neutral(), "queued future axis blocks settling even neutral applied frame");
    q=ready(); q.applied[16]=1; require(!q.complete(284), "actual current local wire must match submitted whole16 bytes");
    q=ready(); q.slots[285%16].seen=false; require(!q.complete(284), "missing future slot refuses coverage");
    q=ready(); q.slots[285%16].frame=301; require(!q.complete(284), "ring overwrite alias refuses coverage");
    q=ready(); q.submit(288,1,neutral); require(!q.complete(284), "special resume or submit gap refuses");
    q=ready(); q.submit(286,1,neutral); require(!q.complete(284), "duplicate submission refuses");
    q=ready(); q.submit(287,0,neutral); require(!q.complete(284), "foreign submit role refuses");
    q=ready(); q.advance(286,1,287,2,8,false,false,false,false,neutral); require(!q.complete(286), "skipped advance refuses");
    q=ready(); q.maximum=9; require(!q.complete(284), "source delay cap exact8");
    q=ready(); q.next_land=294; require(!q.complete(284), "more than8 queued frames refuses");
    q=ready(); q.delay=9; require(!q.complete(284), "delay beyond cap refuses");
    q=ready(); q.adaptive=true; require(!q.complete(284), "adaptive refuses");
    q=ready(); q.hold=true; require(!q.complete(284), "hold/resume refuses");
    q=ready(); q.speculative=true; require(!q.complete(284), "speculative refuses");
    q=ready(); q.rand_neutral=true; require(!q.complete(284), "rand neutral gate refuses");
    q=ready(); q.applied[10]=4; require(!q.complete(284), "peer applied HOLD flag refuses");
    q=ready(); q.slots[286%16].wire[10]=4; require(!q.complete(284), "future HOLD flag refuses");
    q=ready(); q.slots[286%16].wire[10]=3; q.slots[286%16].wire[8]=17; q.slots[286%16].wire[12]=99;
    require(q.complete(284) && q.future_neutral(), "real yaw/random fragment bytes retained without inventing controller actions");
    q=ready(); neutral[2]=9; require(q.slot(285)->wire[2]==0 && q.applied[2]==0, "callback copies input immediately no retained pointer"); neutral[2]=0;
    q=PcCoopQueueObserver(); q.enabled=true; q.role=1; q.submit(284,1,neutral); q.advance(284,1,285,0,8,false,false,false,false,neutral); require(q.complete(284) && q.future_neutral(), "empty actual future interval is explicit complete coverage");
    PcCoopQueueObserver disabled; disabled.submit(284,99,nullptr); require(!disabled.invalid, "disabled fixture observer is inert");
    require(!PcCoopQueueObserver::controls_neutral(axis), "actual nonzero sampled axis is action");
    q=ready(); q.next_land=286; require(!q.complete(284), "actual last submission frontier cannot be hidden");
    q=ready(); require(q.submit_serial==3, "count only genuine contiguous submissions");
    std::puts("PASS 25 standalone actual-wire queue coverage controls; no runtime acceptance");
}

int main(int argc, char** argv) {
    step_tests();
    queue_tests();
    PcCoopGenerationState state = {};
    require(offer(state, "SDL2 1 100 2 1100 0 0 0 END\n", 1, 100) == PC_COOP_GENERATION_NEW_FRESH, "first fresh");
    effective(state, 600, 2);
    require(offer(state, "SDL2 1 100 2 1100 0 0 0 END\n", 1, 601) == PC_COOP_GENERATION_DUPLICATE, "duplicate");
    effective(state, 601, 0);
    require(state.published_ms == 100, "duplicate no refresh");
    require(offer(state, "SDL2 1 100 0 0 0 0 0 END\n", 1, 602) == PC_COOP_GENERATION_INVALID, "mutated duplicate refused");
    require(offer(state, "SDL2 2 100 2 0 0 0 0 END\n", 2, 700) == PC_COOP_GENERATION_NEW_EXPIRED, "stale unread consumed");
    effective(state, 700, 0);
    require(offer(state, "SDL2 1 100 2 1100 0 0 0 END\n", 1, 701) == PC_COOP_GENERATION_OLDER, "older ignored");
    effective(state, 701, 0);
    require(offer(state, "SDL2 3 702 2 1 2 3 4 END\n", 3, 702) == PC_COOP_GENERATION_NEW_FRESH, "fresh held heartbeat");
    effective(state, 702, 2);
    require(offer(state, "SDL2 4 703 0 0 0 0 0 END\n", 4, 703) == PC_COOP_GENERATION_NEW_FRESH, "fresh neutral");
    effective(state, 703, 0);
    require(offer(state, "SDL2 5 704 0 0 0 0 0 END\n", 6, 704) == PC_COOP_GENERATION_INVALID, "name mismatch");
    require(offer(state, "SDL2 5 705 0 0 0 0 0 END\n", 5, 704) == PC_COOP_GENERATION_INVALID, "future tick");
    require(offer(state, "SDL2 5 702 0 0 0 0 0 END\n", 5, 704) == PC_COOP_GENERATION_INVALID, "publication regression");
    require(offer(state, "SDL2 5 704 0 32768 0 0 0 END\n", 5, 704) == PC_COOP_GENERATION_INVALID, "axis bounds");
    require(offer(state, "SDL2 5 704 2097152 0 0 0 0 END\n", 5, 704) == PC_COOP_GENERATION_INVALID, "button bounds");
    require(offer(state, "SDL2 5 704 0 0 0 0 0", 5, 704) == PC_COOP_GENERATION_INVALID, "partial input");
    require(offer(state, "SDL2 05 704 0 0 0 0 0 END\n", 5, 704) == PC_COOP_GENERATION_INVALID, "noncanonical input");
    require(offer(state, "SDL2 5 704 0 0 0 0 0 END\r\n", 5, 704) == PC_COOP_GENERATION_INVALID, "CRLF rejected");
    char oversized[256] = {};
    require(pc_coop_fixture_generation_accept(&state, oversized, 256, 5, 704, 1u<<21) == PC_COOP_GENERATION_INVALID, "capacity refusal");
    unsigned buttons = 0; int axes[4] = {};
    require(!pc_coop_fixture_generation_effective(&state, 702, &buttons, axes), "reader clock regression refused");
#if defined(_WIN32)
    require(argc == 2, "fresh owned output directory argument");
    const std::string directory(argv[1]);
    require(directory.find("output") != std::string::npos, "private output only");
    require(CreateDirectoryA(directory.c_str(), nullptr), "exclusive fresh test directory");
    unsigned long long now = 0;
    require(pc_coop_fixture_input_clock(&now), "Windows boot clock");
    char bytes[256]; unsigned count = 99; PcCoopGenerationInfo info = {};
    require(pc_coop_fixture_generation_snapshot(directory.c_str(), bytes, 256, &count, &info) == PC_COOP_SNAPSHOT_MISSING, "empty queue missing");
    require(count == 0 && !info.sequence, "missing queue does not fabricate generation");
    const std::string pending = directory + "/g00000000000000000001.pending";
    HANDLE file = CreateFileA(pending.c_str(), GENERIC_WRITE, 7, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE, "exclusive partial pending");
    DWORD written = 0;
    require(WriteFile(file, "SDL2 1", 6, &written, nullptr) && written == 6, "partial pending bytes");
    require(CloseHandle(file), "pending creator close");
    require(pc_coop_fixture_generation_snapshot(directory.c_str(), bytes, 256, &count, &info) == PC_COOP_SNAPSHOT_MISSING, "partial pending invisible");
    const std::string ready = directory + "/g00000000000000000002.sdl";
    const std::string command = "SDL2 2 " + std::to_string(now) + " 0 0 0 0 0 END\n";
    file = CreateFileA(ready.c_str(), GENERIC_WRITE, 7, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE, "exclusive owned ready test fixture");
    require(WriteFile(file, command.data(), DWORD(command.size()), &written, nullptr) && written == command.size(), "complete synthetic ready bytes");
    require(CloseHandle(file), "ready creator close");
    require(pc_coop_fixture_generation_snapshot(directory.c_str(), bytes, 256, &count, &info) == PC_COOP_SNAPSHOT_OK, "ready snapshot");
    require(info.sequence == 2 && count == command.size() && !std::memcmp(bytes, command.data(), count), "snapshot raw identity");
    const std::string wrong = directory + "/unknown.sdl";
    file = CreateFileA(wrong.c_str(), GENERIC_WRITE, 7, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE && CloseHandle(file), "unknown owned entry");
    require(pc_coop_fixture_generation_snapshot(directory.c_str(), bytes, 256, &count, &info) == PC_COOP_SNAPSHOT_INVALID_ENTRY, "unknown entry refusal");
#else
    (void)argc; (void)argv;
    unsigned long long now = 99;
    require(!pc_coop_fixture_input_clock(&now), "non-Windows queue explicitly unsupported");
#endif
    std::puts("PASS standalone generation helper/parser only; no native gameplay acceptance");
    return 0;
}
