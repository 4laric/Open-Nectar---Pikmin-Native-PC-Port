#include "pc_midday_view_piki.h"
#include "ViewPiki.h"
#include <typeinfo>
namespace pc_midday {
bool view_piki_fields(Piki& base, ActorArchive& a) {
    if (typeid(base) != typeid(ViewPiki)) return a.fail("ViewPiki exact factory type required");
    auto& actor = static_cast<ViewPiki&>(base);
    // The constructor leaves mHappaModel uninitialized. Capture is for actors
    // whose ordinary init/initBirth completed at a tick boundary, not raw births.
    if (a.mode() == Mode::Capture && !actor.mPikiShape) return a.fail("ViewPiki capture requires completed initialization");
    return view_piki_subtype_fields(actor.mPikiShape, actor.mHappaModel, actor.mLastEffectPosition, a);
}
bool capture_view_piki(Piki& actor, LogicalResolver& resolver, double now, ActorBytes& bytes, std::string& e) {
    ActorFields fields; FieldArchive a(Mode::Capture, fields, resolver, e, now);
    std::vector<FieldSchema> schema;
    return view_piki_fields(actor, a) && a.finish() && view_piki_schema(fields, schema, e) &&
        validate_actor_fields(fields, schema, resolver, e) && encode_actor_fields(fields, bytes, e);
}
bool bind_view_piki(Piki& actor, const ActorBytes& bytes, LogicalResolver& resolver, double now, std::string& e) {
    if (!validate_view_piki(bytes, resolver, e)) return false;
    ActorFields fields; if (!decode_actor_fields(bytes, fields, e)) return false;
    FieldArchive check(Mode::Validate, fields, resolver, e, now);
    if (!view_piki_fields(actor, check) || !check.finish()) return false;
    FieldArchive apply(Mode::Apply, fields, resolver, e, now);
    return view_piki_fields(actor, apply) && apply.finish();
}
}
