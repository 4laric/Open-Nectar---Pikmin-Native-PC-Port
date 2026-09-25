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
    // hosts (include/teki.h). The lane is complete: Chappy (2),
    // FireChappy (33), YellowChappy (43) and KingChappy (53) ride
    // TEKI_Swallow (4, P1 Spotty Bulborb), KumaChappy (35) rides
    // TEKI_Swallob (32, P1 Spotty Bulbear), LeafChappy (67) rides
    // TEKI_Chappy (3, P1 Dwarf Bulborb), KumaKochappy (76) rides
    // TEKI_Chappb (31, P1 Dwarf Bulbear).
    case 2: case 33: case 43: case 53:
        return 4; // TEKI_Swallow: Red/Fiery/Hairy/Emperor Bulborb adults
    case 35:
        return 32; // TEKI_Swallob: Spotty Bulbear
    case 67:
        return 3; // TEKI_Chappy: Bulbmin
    case 76:
        return 31; // TEKI_Chappb: Dwarf Bulbear
    // Sarai binds whatever type its anchor carries (pc_p2_sarai_manager.cpp:109,119),
    // so 3 here is the placement vehicle, not a requirement. "Kochappy" is the P2 name
    // for the Dwarf Bulborb; the P1 enum for it is TEKI_Chappy, and there is no
    // TEKI_Kochappy in include/teki.h.
    case 23: return 3; // TEKI_Chappy: Sarai ordinary-host path
    case 28: case 68: return 3; // TEKI_Chappy: ElecBug/TamagoMushi ground hosts
    case 94: return 4; // TEKI_Swallow: DangoMushi snagret host
    case 12: return 18; // TEKI_KabekuiA: UjiA Female Sheargrub host
    case 13: return 19; // TEKI_KabekuiB: UjiB Male Sheargrub host
    case 14: return 20; // TEKI_KabekuiC: Tobi Shearwig host
    case 54: return 24; // TEKI_Miurin: Mamuta
    case 57: case 78: return 0; // TEKI_Frog: Jellyfloat/Groink sidecar hosts
    case 17: return 0; // TEKI_Frog: Yellow Wollywog (inst-frogs #871)
    case 18: return 33; // TEKI_Frow: Wollywog (inst-frogs #871)
    case 24: return 15; // TEKI_Tank: Fiery Blowhog (inst2-frogs #871)
    case 25: return 15; // TEKI_Tank: Watery Blowhog (inst2-frogs #871)
    case 15: return 3; // TEKI_Chappy: Cloaking Burrow-nit (inst2-frogs #871)
    case 75: return 17; // TEKI_Beatle: Armored Cannon Beetle Larva (inst2-frogs #871)
    default: return original;
    }
}
inline bool hasStaticHost(unsigned source) {
    return hostType(source, -1, false) == hostType(source, -2, false);
}
}
