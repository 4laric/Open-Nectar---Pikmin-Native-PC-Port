#include "pc_midday_actor_archive.h"
#include "pc_p2_demon_drop_policy.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>
using namespace pc_midday;
namespace {
int checks=0;
void check(bool b,const char* msg) {++checks;if(!b)throw std::runtime_error(msg);}
struct Resolver : LogicalResolver {
    int object=1;
    bool validateTyped(const FieldSchema& d,const LogicalRef& r,std::string& e) const override {return !d.targetType.empty()&&(!r.owner&&!r.resource&&!r.slot?d.nullable:validate(d.key.c_str(),d.reference,r,e));}
    bool identify(const char* key,RefKind,const void* p,LogicalRef& r,std::string&) override {if(p!=&object)return false;r={7,0,2};return true;}
    bool validate(const char* key,RefKind,const LogicalRef& r,std::string&) const override {return (r.owner==7 || (r.owner==8 && std::string(key)!="target")) && r.resource==0 && r.slot==2;}
    bool resolve(const char* key,RefKind k,const LogicalRef& r,void*& p,std::string& e) override {if(!validate(key,k,r,e))return false;p=&object;return true;}
    bool identifyHandle(const char* key,RefKind,u32 v,LogicalRef& r,std::string&) override {if(v!=19)return false;r={7,0,2};return true;}
    bool resolveHandle(const char* key,RefKind k,const LogicalRef& r,u32& v,std::string& e) override {if(!validate(key,k,r,e))return false;v=81;return true;}
    bool identifyToken(const char*,RefKind,u64 v,LogicalRef& r,std::string&) override {if(v!=0x100000019ULL)return false;r={7,0,2};return true;}
    bool resolveToken(const char* key,RefKind k,const LogicalRef& r,u64& v,std::string& e) override {if(!validate(key,k,r,e))return false;v=0x200000051ULL;return true;}
};
void integer(ActorFields& f,const char* key,ScalarKind kind,u64 bits){ActorField a;a.scalar=kind;a.bits=bits;f[key]=a;}
void run() {
    Resolver resolver;std::string error;ActorFields fields;
    {
        ActorFields tokens;u64 source=0x100000019ULL;
        FieldArchive captureToken(Mode::Capture,tokens,resolver,error,0);
        check(captureToken.token64("projectile.owner",RefKind::ProjectileToken,source)&&captureToken.finish(),"64-bit token capture");
        ActorBytes wire;check(encode_actor_fields(tokens,wire,error),"logical token encoding");
        ActorFields restored;check(decode_actor_fields(wire,restored,error),"logical token decoding");
        const auto schema=std::vector<FieldSchema>{FieldSchema::token64("projectile.owner",RefKind::ProjectileToken,false,"P2CannonStone",ReferenceOwnership::ActorSubobject)};
        check(validate_actor_fields(restored,schema,resolver,error),"token closure validation");
        u64 rebound=0;FieldArchive applyToken(Mode::Apply,restored,resolver,error,0);
        check(applyToken.token64("projectile.owner",RefKind::ProjectileToken,rebound)&&applyToken.finish()&&rebound==0x200000051ULL,"64-bit token remaps without truncation");
        auto invalid=restored;invalid["projectile.owner"].target={};std::string failure;
        check(!validate_actor_fields(invalid,schema,resolver,failure),"required token absent refused");
        invalid=restored;invalid["projectile.owner"].category=FieldCategory::Handle;failure.clear();
        check(!validate_actor_fields(invalid,schema,resolver,failure),"32-bit handle cannot replace token");
        invalid=restored;invalid["projectile.owner"].reference=static_cast<RefKind>(-1);failure.clear();
        check(!encode_actor_fields(invalid,wire,failure),"negative reference kind refused");
    }
    {
        ActorFields seed;integer(seed,"navi.runtime.plate.capacity",ScalarKind::S32,1);integer(seed,"navi.runtime.mPcPikiAnimColor",ScalarKind::S32,0xffffffffu);
        std::vector<FieldSchema> descriptors;check(navi_runtime_schema(seed,descriptors,error),"CPlate role schema");
        FieldSchema occupant;for(const auto& d:descriptors)if(d.key=="navi.runtime.plate.slot.0.occupant")occupant=d;
        check(occupant.targetType=="Piki","all nonnull CPlate occupants require Piki");
        struct PlateResolver:Resolver {bool validateTyped(const FieldSchema& d,const LogicalRef& r,std::string& e)const override {return d.targetType=="Piki"&&(r.owner==7||(!r.owner&&!r.resource&&!r.slot&&d.nullable));}} plateResolver;
        ActorField value;value.category=FieldCategory::Reference;value.reference=RefKind::Creature;value.target={7,0,2};ActorFields one={{occupant.key,value}};
        check(validate_actor_fields(one,{occupant},plateResolver,error),"registered Piki slot occupant accepted");
        one[occupant.key].target.owner=8;check(resolver.validate(occupant.key.c_str(),RefKind::Creature,one[occupant.key].target,error),"foreign Navi exists in catalog");
        check(!validate_actor_fields(one,{occupant},plateResolver,error),"valid foreign Navi cannot occupy CPlate");
        one[occupant.key].target={};check(validate_actor_fields(one,{occupant},plateResolver,error),"empty inactive CPlate slot retained");
    }
    s32 signedValue=-53;u64 wide=0xfedcba9876543210ULL;float fraction=0.125f;bool flag=true;int* pointer=&resolver.object;u32 handle=19;
    FieldArchive capture(Mode::Capture,fields,resolver,error,100);
    check(capture.field("signed",signedValue)&&capture.field("wide",wide)&&capture.field("fraction",fraction)&&capture.field("flag",flag),"typed capture");
    check(capture.ref("target",RefKind::Creature,pointer)&&capture.handle("path",RefKind::Path,handle)&&capture.finish(),"logical capture");
    ActorBytes bytes;check(encode_actor_fields(fields,bytes,error),"encode");
    ActorFields decoded;check(decode_actor_fields(bytes,decoded,error),"decode");
    for(size_t n=0;n<bytes.size();++n) {
        ActorBytes truncated(bytes.begin(),bytes.begin()+n);ActorFields unchanged=decoded;std::string e;
        check(!decode_actor_fields(truncated,unchanged,e)&&unchanged.size()==decoded.size(),"truncation atomic refusal");
    }
    auto extra=bytes;extra.push_back(0);std::string e;check(!decode_actor_fields(extra,decoded,e),"trailing refusal");
    auto invalid=fields;invalid["flag"].bits=2;ActorBytes unchanged=bytes;e.clear();check(!encode_actor_fields(invalid,unchanged,e)&&unchanged==bytes,"invalid bool atomic refusal");
    invalid=fields;invalid["fraction"].bits=0x7fc00000;e.clear();check(!encode_actor_fields(invalid,unchanged,e),"NaN refusal");
    std::vector<FieldSchema> schema={FieldSchema::value("signed",ScalarKind::S32),FieldSchema::value("wide",ScalarKind::U64),FieldSchema::value("fraction",ScalarKind::F32),FieldSchema::value("flag",ScalarKind::Bool),FieldSchema::ref("target",RefKind::Creature,false,"Piki"),FieldSchema::handle("path",RefKind::Path,false,"RouteHandle",ReferenceOwnership::Content)};
    e.clear();check(validate_actor_fields(fields,schema,resolver,e),"pure graph validation");
    {
        struct StrongResolver final:Resolver {
            bool validateTyped(const FieldSchema& d,const LogicalRef& r,std::string& e)const override {
                return (d.key!="target"||d.strength==ReferenceStrength::StrongCreature)&&Resolver::validateTyped(d,r,e);
            }
        } strongResolver;
        auto strongSchema=schema;strongSchema[4].strength=ReferenceStrength::StrongCreature;
        e.clear();check(validate_actor_fields(fields,strongSchema,strongResolver,e),"compiled strong storage metadata reaches resolver");
        e.clear();check(!validate_actor_fields(fields,schema,strongResolver,e),"weak metadata cannot substitute for required smart storage");
        strongSchema[5].strength=ReferenceStrength::StrongCreature;e.clear();
        check(!validate_actor_fields(fields,strongSchema,resolver,e),"non-creature handle cannot be strong creature storage");
        strongSchema=schema;strongSchema[4].strength=static_cast<ReferenceStrength>(99);e.clear();
        check(!validate_actor_fields(fields,strongSchema,resolver,e),"unknown reference strength refused");
    }
    struct TypedResolver final : Resolver {
        bool validateTyped(const FieldSchema& d,const LogicalRef& r,std::string& error) const override {
            if(d.key=="target")return d.targetType=="Piki" && d.ownership==ReferenceOwnership::AnyLive && validate(d.key.c_str(),d.reference,r,error);
            return Resolver::validateTyped(d,r,error);
        }
    } typedResolver;
    e.clear();check(validate_actor_fields(fields,schema,typedResolver,e),"concrete reference contract delivered to resolver");
    auto wrongType=schema;wrongType[4].targetType="Navi";e.clear();
    check(!validate_actor_fields(fields,wrongType,typedResolver,e),"same broad kind cannot bypass concrete target type");
    wrongType=schema;wrongType[4].ownership=ReferenceOwnership::Self;e.clear();
    check(!validate_actor_fields(fields,wrongType,typedResolver,e),"ownership contract delivered to resolver");
    wrongType=schema;wrongType[4].targetType.clear();wrongType[4].nullable=true;
    invalid=fields;invalid["target"].target={};e.clear();
    check(!validate_actor_fields(invalid,wrongType,typedResolver,e),"null optional reference still requires compiled target contract");
    {
        ActorFields linked;
        ActorField captain;captain.category=FieldCategory::Reference;captain.reference=RefKind::Creature;captain.target={8,0,2};linked["captain"]=captain;
        ActorField plate=captain;plate.reference=RefKind::CPlate;linked["plate"]=plate;
        std::vector<FieldSchema> linkSchema={FieldSchema::ref("captain",RefKind::Creature,false,"Navi"),FieldSchema::ref("plate",RefKind::CPlate,false,"CPlate",ReferenceOwnership::ActorSubobject,"captain")};
        e.clear();check(validate_actor_fields(linked,linkSchema,resolver,e),"captain-owned subobject cross-actor link validates");
        linked["plate"].target.owner=7;e.clear();check(!validate_actor_fields(linked,linkSchema,resolver,e),"valid subobject belonging to wrong captain refused");
        linked["plate"].target.owner=8;linked["captain"].target={};linkSchema[0].nullable=true;e.clear();
        check(!validate_actor_fields(linked,linkSchema,resolver,e),"present subobject cannot refer through absent owner");
    }
    {
        struct ResourceResolver final:Resolver {
            bool resourceContext=true;
            bool validateTyped(const FieldSchema& d,const LogicalRef& r,std::string&) const override {
                if(!resourceContext||d.targetType!="zen::particleMdl"||d.ownership!=ReferenceOwnership::ResourceSubobject)return false;
                return !r.owner&&!r.resource&&!r.slot?d.nullable:r.resource==91&&r.slot==4;
            }
        } resourceResolver;
        ActorField node;node.category=FieldCategory::Reference;node.reference=RefKind::ParticleNode;node.target={0,91,4};
        ActorFields nodes={{"model",node}};
        std::vector<FieldSchema> nodeSchema={FieldSchema::ref("model",RefKind::ParticleNode,false,"zen::particleMdl",ReferenceOwnership::ResourceSubobject)};
        e.clear();check(validate_actor_fields(nodes,nodeSchema,resourceResolver,e),"resource subject child validates");
        nodes["model"].target.owner=7;e.clear();check(!validate_actor_fields(nodes,nodeSchema,resourceResolver,e),"resource child cannot carry actor owner");
        nodes["model"].target={0,92,4};e.clear();check(!validate_actor_fields(nodes,nodeSchema,resourceResolver,e),"different resource subject child refused");
        nodes["model"].target={0,91,5};e.clear();check(!validate_actor_fields(nodes,nodeSchema,resourceResolver,e),"unregistered resource child slot refused");
        nodes["model"].target={};nodeSchema[0].nullable=true;e.clear();check(validate_actor_fields(nodes,nodeSchema,resourceResolver,e),"nullable resource child with correct context validates");
        resourceResolver.resourceContext=false;e.clear();check(!validate_actor_fields(nodes,nodeSchema,resourceResolver,e),"nullable resource child cannot bypass subject context");
        resourceResolver.resourceContext=true;nodeSchema[0].ownership=static_cast<ReferenceOwnership>(99);e.clear();check(!validate_actor_fields(nodes,nodeSchema,resolver,e),"nullable reference cannot bypass unknown ownership check");
    }
    invalid=fields;invalid["target"].target={};e.clear();check(!validate_actor_fields(invalid,schema,resolver,e),"required null refusal");
    invalid=fields;invalid["target"].target.owner=8;e.clear();check(!validate_actor_fields(invalid,schema,resolver,e),"existing incompatible actor role refusal");
    invalid=fields;invalid["target"].reference=RefKind::Animation;e.clear();check(!validate_actor_fields(invalid,schema,resolver,e),"reference role refusal");
    signedValue=41;wide=3;fraction=8;flag=false;pointer=nullptr;
    FieldArchive validate(Mode::Validate,fields,resolver,error,900);
    check(validate.field("signed",signedValue)&&validate.field("wide",wide)&&validate.field("fraction",fraction)&&validate.field("flag",flag)&&validate.ref("target",RefKind::Creature,pointer),"validate pass");
    check(signedValue==41&&wide==3&&fraction==8&&!flag&&!pointer,"validation never changes actor members");
    FieldArchive apply(Mode::Apply,fields,resolver,error,900);
    check(apply.field("signed",signedValue)&&apply.field("wide",wide)&&apply.field("fraction",fraction)&&apply.field("flag",flag)&&apply.ref("target",RefKind::Creature,pointer)&&apply.handle("path",RefKind::Path,handle)&&apply.finish(),"apply pass");
    check(signedValue==-53&&wide==0xfedcba9876543210ULL&&fraction==.125f&&flag&&pointer==&resolver.object&&handle==81,"typed value and remapped handles");
    for(int id=0;id<38;++id) {
        ActorFields f;
        integer(f,"creature.objectType",ScalarKind::S32,54);integer(f,"creature.search.capacity",ScalarKind::S16,0);integer(f,"creature.search.count",ScalarKind::S16,0);integer(f,"creature.search.last",ScalarKind::S32,0xffffffff);
        integer(f,"navi.runtime.plate.capacity",ScalarKind::S32,1);integer(f,"navi.runtime.mPcPikiAnimColor",ScalarKind::S32,0xffffffff);integer(f,"current",ScalarKind::S32,id);integer(f,"last",ScalarKind::S32,0xffffffff);
        integer(f,"demon.listeners.count",ScalarKind::U32,0);integer(f,"demon.policy.phase",ScalarKind::S32,id==36 ? 1 : 0);
        if(id==25){integer(f,"state.current",ScalarKind::S32,0);integer(f,"state.last",ScalarKind::S32,0xffffffff);}
        std::vector<FieldSchema> description;e.clear();check(navi_schema(f,description,e),"all38 Navi schemas");
        for(const auto& d:description)if(!f.count(d.key)) {ActorField v;v.category=d.category;v.scalar=d.scalar;v.reference=d.reference;if(d.category!=FieldCategory::Scalar&&!d.nullable)v.target={7,0,2};f[d.key]=v;}
        check(validate_actor_fields(f,description,resolver,e),"complete Navi schema validates");
        if(id==0) {
            ActorBytes valid;check(encode_actor_fields(f,valid,e)&&validate_navi(valid,resolver,e),"pure prebegin Navi factory");
            auto bad=f;double age=-.5;std::memcpy(&bad["whistle.pressAge"].bits,&age,8);ActorBytes wire;e.clear();
            check(encode_actor_fields(bad,wire,e)&&!validate_navi(wire,resolver,e),"fractional negative age refusal");
            for(const char* key:{"navi.runtime.plate.mPlatePikiCount","navi.runtime.plate.happa.0"}) {
                bad=f;bad[key].bits=2;e.clear();check(encode_actor_fields(bad,wire,e)&&!validate_navi(wire,resolver,e),"CPlate counter beyond capacity refused");
            }
            bad=f;bad["navi.runtime.plate.happa.0"].bits=0xffffffff;e.clear();check(encode_actor_fields(bad,wire,e)&&!validate_navi(wire,resolver,e),"negative CPlate happa refused");
            bad=f;bad["navi.runtime.plate.happa.0"].bits=1;bad["navi.runtime.plate.happa.1"].bits=1;e.clear();check(encode_actor_fields(bad,wire,e)&&!validate_navi(wire,resolver,e),"CPlate combined happa overflow refused");
            bad=f;bad["navi.runtime.plate.happa.0"].bits=1;e.clear();check(encode_actor_fields(bad,wire,e)&&validate_navi(wire,resolver,e),"bounded transitional CPlate counter differences retained");
            bad=f;bad["current"].bits=38;e.clear();check(encode_actor_fields(bad,wire,e)&&!validate_navi(wire,resolver,e),"unknown state refusal before begin");
        }
        if(id==3) {
            auto bad=f;bad["state.mWhistleAnimPhase"].bits=3;ActorBytes wire;e.clear();
            check(encode_actor_fields(bad,wire,e)&&!validate_navi(wire,resolver,e),"invalid inner phase before begin");
        }
        f["unexpected"]=ActorField{};e.clear();check(!validate_actor_fields(f,description,resolver,e),"unknown Navi fields refused");
    }
    P2DemonDropPolicy before;check(before.begin(5,13,40).accepted,"drop begin");check(before.bounce(5).accepted,"drop bounce");
    auto state=before.captureState();P2DemonDropPolicy restored;check(restored.restoreState(state),"drop typed restore");
    auto command=restored.animationEnd(5,P2DemonDropPhase::Knockdown);check(command.deliverDamage&&command.damage==13,"pending damage survives");
    check(!restored.animationEnd(5,P2DemonDropPhase::Knockdown).deliverDamage,"damage once only");
    state.phase=static_cast<P2DemonDropPhase>(99);check(!restored.restoreState(state),"bad phase refused");
}
}
int main(){try{run();std::printf("midday actor component: %d checks passed\n",checks);return 0;}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
