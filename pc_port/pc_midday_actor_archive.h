#pragma once
#include "pc_midday_actor_states.h"
#include <vector>
#include <map>
#include <set>
namespace pc_midday {
using ActorBytes = std::vector<u8>;
enum class FieldCategory : u8 { Scalar, Reference, Handle };
struct FieldSchema {
    std::string key;
    FieldCategory category;
    ScalarKind scalar;
    RefKind reference;
    bool nullable;
    static FieldSchema value(const char* k, ScalarKind s) { return {k,FieldCategory::Scalar,s,RefKind::Creature,false}; }
    static FieldSchema ref(const char* k, RefKind r, bool n=false) { return {k,FieldCategory::Reference,ScalarKind::U8,r,n}; }
    static FieldSchema handle(const char* k, RefKind r, bool n=false) { return {k,FieldCategory::Handle,ScalarKind::U32,r,n}; }
};
// owner is a checkpoint actor incarnation, resource is a content-bound resource
// identity, slot identifies a named/indexed subobject of that owner/resource.
// All-zero is absent; no process address or recycled runtime handle is permitted.
struct LogicalRef { u64 owner=0, resource=0; u32 slot=0; };
struct ActorField {
    FieldCategory category=FieldCategory::Scalar;
    ScalarKind scalar=ScalarKind::U8;
    RefKind reference=RefKind::Creature;
    u64 bits=0;
    LogicalRef target;
};
using ActorFields = std::map<std::string,ActorField>;
// Each callback receives the schema key. Implementations MUST enforce the
// concrete destination type and ownership role for that key (not merely the
// broad RefKind), before begin. resolve returns that adjusted concrete pointer.
class LogicalResolver {
public:
    virtual ~LogicalResolver()=default;
    virtual bool identify(const char*,RefKind,const void*,LogicalRef&,std::string&)=0;
    virtual bool validate(const char*,RefKind,const LogicalRef&,std::string&) const=0;
    virtual bool resolve(const char*,RefKind,const LogicalRef&,void*&,std::string&)=0;
    virtual bool identifyHandle(const char*,RefKind,u32,LogicalRef&,std::string&)=0;
    virtual bool resolveHandle(const char*,RefKind,const LogicalRef&,u32&,std::string&)=0;
};
bool encode_actor_fields(const ActorFields&,ActorBytes&,std::string&);
bool decode_actor_fields(const ActorBytes&,ActorFields&,std::string&);
bool validate_actor_fields(const ActorFields&,const std::vector<FieldSchema>&,const LogicalResolver&,std::string&);
bool actor_i32(const ActorFields&,const char*,int&,std::string&);
bool actor_u32(const ActorFields&,const char*,u32&,std::string&);
class FieldArchive final : public ActorArchive {
    Mode operation;
    double clockInstant;
    ActorFields& fields;
    LogicalResolver& resolver;
    std::string& error;
    std::set<std::string> visited;
    bool entry(const char*,FieldCategory,ScalarKind,RefKind,ActorField*&);
public:
    FieldArchive(Mode m,ActorFields& f,LogicalResolver& r,std::string& e,double now) : operation(m),clockInstant(now),fields(f),resolver(r),error(e) {}
    Mode mode() const override { return operation; }
    double clock_now() const override { return clockInstant; }
    bool scalar(const char*,ScalarKind,void*) override;
    bool reference(const char*,RefKind,void*&) override;
    bool handle(const char*,RefKind,u32&) override;
    bool fail(const char* reason) override { if(error.empty()) error=reason; return false; }
    bool finish();
};
// Schema functions are pure and may run before RestoreBackend::begin.
bool capture_navi(Navi&,LogicalResolver&,double now,ActorBytes&,std::string&);
bool validate_navi(const ActorBytes&,const LogicalResolver&,std::string&);
bool allocate_navi_subobjects(Navi&,const ActorBytes&,LogicalResolver&,double now,std::string&);
bool bind_navi(Navi&,const ActorBytes&,LogicalResolver&,double now,std::string&);
bool navi_runtime_schema(const ActorFields&,std::vector<FieldSchema>&,std::string&);
void animation_schema(const std::string&,std::vector<FieldSchema>&);
bool navi_schema(const ActorFields&,std::vector<FieldSchema>&,std::string&);
bool piki_state_schema(int,std::vector<FieldSchema>&);
bool piki_action_schema(int,std::vector<FieldSchema>&);
void piki_runtime_schema(std::vector<FieldSchema>&);
// Piki adapter: pure validation is required before scene allocation/publication.
bool piki_schema(const ActorFields&,std::vector<FieldSchema>&,std::string&);
bool capture_piki(Piki&,LogicalResolver&,double now,ActorBytes&,std::string&);
bool validate_piki(const ActorBytes&,const LogicalResolver&,std::string&);
bool bind_piki(Piki&,const ActorBytes&,LogicalResolver&,double now,std::string&);
}
