#include "pc_p2_species_schema.h"
#include <cassert>
#include <cstdio>

// Lane 11 policy test: versioned species/compartment schema must reject unknown
// versions and new species in old readers, and must conserve totals.
int main() {
    assert(p2_schema_valid(P2SpeciesSchemaLegacy));
    assert(p2_schema_valid(P2SpeciesSchemaP2));
    assert(p2_schema_valid(P2SpeciesSchemaBulbmin));
    assert(!p2_schema_valid(0) && !p2_schema_valid(4) && !p2_schema_valid(-1));

    assert(p2_schema_max_species(P2SpeciesSchemaLegacy) == P2SpeciesYellow);
    assert(p2_schema_max_species(P2SpeciesSchemaP2) == P2SpeciesWhite);
    assert(p2_schema_max_species(P2SpeciesSchemaBulbmin) == P2SpeciesBulbmin);
    assert(p2_schema_max_species(99) == -1);

    // Each version admits only its own species and older ones.
    assert(p2_schema_supports(P2SpeciesSchemaLegacy, P2SpeciesBlue));
    assert(p2_schema_supports(P2SpeciesSchemaLegacy, P2SpeciesYellow));
    assert(!p2_schema_supports(P2SpeciesSchemaLegacy, P2SpeciesPurple));
    assert(!p2_schema_supports(P2SpeciesSchemaLegacy, P2SpeciesBulbmin));
    assert(p2_schema_supports(P2SpeciesSchemaP2, P2SpeciesPurple));
    assert(p2_schema_supports(P2SpeciesSchemaP2, P2SpeciesWhite));
    assert(!p2_schema_supports(P2SpeciesSchemaP2, P2SpeciesBulbmin));
    assert(p2_schema_supports(P2SpeciesSchemaBulbmin, P2SpeciesBulbmin));
    assert(!p2_schema_supports(99, P2SpeciesBlue));

    // Old readers keep working and reject a newer species explicitly.
    P2SpeciesCounts legacy;
    legacy.count[P2SpeciesBlue] = 7;
    legacy.count[P2SpeciesYellow] = 3;
    assert(p2_schema_validate(P2SpeciesSchemaLegacy, legacy));
    assert(p2_schema_total(legacy) == 10);

    P2SpeciesCounts withPurple = legacy;
    withPurple.count[P2SpeciesPurple] = 1;
    assert(!p2_schema_validate(P2SpeciesSchemaLegacy, withPurple)); // v1 cannot carry Purple
    assert(p2_schema_validate(P2SpeciesSchemaP2, withPurple));

    P2SpeciesCounts withBulbmin = withPurple;
    withBulbmin.count[P2SpeciesBulbmin] = 2;
    assert(!p2_schema_validate(P2SpeciesSchemaP2, withBulbmin)); // v2 cannot carry Bulbmin
    assert(p2_schema_validate(P2SpeciesSchemaBulbmin, withBulbmin));
    assert(p2_schema_total(withBulbmin) == 13);

    // Negative counts and unknown versions are always rejected.
    P2SpeciesCounts bad = withBulbmin;
    bad.count[P2SpeciesWhite] = -1;
    assert(!p2_schema_validate(P2SpeciesSchemaBulbmin, bad));
    assert(!p2_schema_validate(0, withBulbmin));

    std::puts("PASS P2_SPECIES_SCHEMA");
    return 0;
}
