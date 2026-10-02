#pragma once
#include "pc_midday_actor_archive.h"
class PikiShapeObject;
class Shape;
namespace pc_midday {
// Same typed field walk used by the exact-subtype wrapper. Kept inline so
// actual-header controls need no fabricated actor or engine constructor stubs.
// Validate the complete payload schema before Apply. The resolver must return
// the exact non-null typed resource for every non-absent logical reference.
inline bool view_piki_subtype_fields(PikiShapeObject*& shape, Shape*& happa, Vector3f& position, ActorArchive& a) {
    if (a.mode() == Mode::Capture && !shape) return a.fail("ViewPiki capture requires completed initialization");
    const bool capture = a.mode() == Mode::Capture;
    PikiShapeObject* stagedShape = capture ? shape : nullptr;
    Shape* stagedHappa = capture ? happa : nullptr;
    Vector3f stagedPosition;
    if (capture) stagedPosition = position;
    u32 version = capture ? 1 : 0;
    if (!(a.scalar("viewPiki.version", ScalarKind::U32, &version) &&
        (version == 1 || a.fail("unsupported ViewPiki extension version")) &&
        a.ref("viewPiki.pikiShape", RefKind::Shape, stagedShape) &&
        a.ref("viewPiki.happaModel", RefKind::Shape, stagedHappa) &&
        a.field("viewPiki.lastEffectPosition", stagedPosition))) return false;
    if (a.mode() == Mode::Apply) {
        shape = stagedShape; happa = stagedHappa; position = stagedPosition;
    }
    return true;
}
// Separate derived payload; the existing base Piki wire format is unchanged.
// Compose with the base record for the same actor identity. Validate both before
// scene allocation, then bind both into a disposable stage before publication.
bool view_piki_fields(Piki&, ActorArchive&);
bool view_piki_schema(const ActorFields&, std::vector<FieldSchema>&, std::string&);
bool capture_view_piki(Piki&, LogicalResolver&, double, ActorBytes&, std::string&);
bool validate_view_piki(const ActorBytes&, const LogicalResolver&, std::string&);
bool bind_view_piki(Piki&, const ActorBytes&, LogicalResolver&, double, std::string&);
}
