// Overworld course membership link probe (#802).
//
// Minimal replacement-main probe (isolated fixture build only; never part of
// the game target): parses the REAL process argv through the integrated
// module (pc_pikipelago_overworld_course_parse), requires the course flag
// and the registration helper, and exits PASS. Linking this TU against the
// separately compiled module object proves PC_PORT_SOURCES membership would
// resolve the pc_pikipelago_overworld_course_* symbols (no undefined refs).
//
// This probe performs no engine boot and observes no ticks, so there is no
// captain to guard; the headed #738 smoke run carries the #632 guard.
// Unguarded runs are refused: --allow-unguarded exits 2.
#include "pc_p2_overworld_course.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--allow-unguarded")) {
            std::fprintf(stderr, "unguarded runs refused\n");
            return 2;
        }
    }
    std::printf("P2_OVERWORLD_COURSE_MEMBERSHIP_LINKED\n");
    std::fflush(stdout);
    pc_pikipelago_overworld_course_parse(argc, argv);
    const char* course = pc_pikipelago_overworld_course_id();
    if (!course) {
        std::printf("P2_OVERWORLD_COURSE_ABSENT reason=no_course_flag_in_module\n");
        std::fflush(stdout);
        return 2;
    }
    if (!pc_pikipelago_overworld_course_register()) {
        std::printf("FAIL P2_OVERWORLD_COURSE_MEMBERSHIP registration\n");
        std::fflush(stdout);
        return 1;
    }
    std::printf("P2_OVERWORLD_COURSE_FLAG course=%s index=%d\n", course, pc_pikipelago_overworld_course());
    std::printf("P2_OVERWORLD_COURSE_REGISTERED course=%s\n", course);
    std::puts("PASS P2_OVERWORLD_COURSE_MEMBERSHIP");
    std::fflush(stdout);
    return 0;
}
