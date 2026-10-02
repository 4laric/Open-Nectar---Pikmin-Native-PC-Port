#include "pc_midday_view_piki.h"
namespace pc_midday {
bool view_piki_schema(const ActorFields& fields, std::vector<FieldSchema>& schema, std::string& e) {
    u32 version = 0;
    if (!actor_u32(fields, "viewPiki.version", version, e)) return false;
    if (version != 1) { e = "unsupported ViewPiki extension version"; return false; }
    schema = {
        FieldSchema::value("viewPiki.version", ScalarKind::U32),
        FieldSchema::ref("viewPiki.pikiShape", RefKind::Shape, false, "PikiShapeObject", ReferenceOwnership::Content),
        FieldSchema::ref("viewPiki.happaModel", RefKind::Shape, true, "Shape", ReferenceOwnership::Content),
        FieldSchema::value("viewPiki.lastEffectPosition.x", ScalarKind::F32),
        FieldSchema::value("viewPiki.lastEffectPosition.y", ScalarKind::F32),
        FieldSchema::value("viewPiki.lastEffectPosition.z", ScalarKind::F32)
    };
    return true;
}
bool validate_view_piki(const ActorBytes& bytes, const LogicalResolver& resolver, std::string& e) {
    ActorFields fields; std::vector<FieldSchema> schema;
    return decode_actor_fields(bytes, fields, e) && view_piki_schema(fields, schema, e) &&
        validate_actor_fields(fields, schema, resolver, e);
}
}
