#pragma once

// Native host types for the admitted campaign modules. This only chooses the
// engine vehicle; family setup still must bind its source behavior and assets.
// Never infer admission or a legal placement from this table.
namespace p2campaign {
inline int hostType(unsigned source, int original, bool protectedSpawn) {
    if (protectedSpawn) return original;
    switch (source) {
    case 9: case 44: case 59: case 60: case 61: case 62: case 79:
        return 3; // TEKI_Chappy (3; 4 is TEKI_Swallow): typed beetle/dwarf/dweevil/Skitter Leaf hosts
    // inst-chappy (#871): Chappy-family vehicles preserve the proven proxy
    // hosts (include/teki.h). One finished species at a time: Chappy (2)
    // rides TEKI_Swallow (4, P1 Spotty Bulborb). Later species add their
    // own case when their identity lands (33/43/53 -> 4, 35 -> 32, 67 ->
    // 3, 76 -> 31); a species must never become static before its identity
    // stages, or the proxy visual skips it with nothing to replace it.
    case 2:
        return 4; // TEKI_Swallow: Red Bulborb adult
    // Sarai binds whatever type its anchor carries (pc_p2_sarai_manager.cpp:109,119),
    // so 3 here is the placement vehicle, not a requirement. "Kochappy" is the P2 name
    // for the Dwarf Bulborb; the P1 enum for it is TEKI_Chappy, and there is no
    // TEKI_Kochappy in include/teki.h.
    case 23: return 3; // TEKI_Chappy: Sarai ordinary-host path
    case 54: return 24; // TEKI_Miurin: Mamuta
    case 57: case 78: return 0; // TEKI_Frog: Jellyfloat/Groink sidecar hosts
    default: return original;
    }
}
inline bool hasStaticHost(unsigned source) {
    return hostType(source, -1, false) == hostType(source, -2, false);
}
}
