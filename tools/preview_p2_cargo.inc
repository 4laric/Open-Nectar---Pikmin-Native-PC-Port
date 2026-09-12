// Include after preview fixture's require/capture helpers. Integration calls
// verifyCargoReceipts() at a safe floor boundary; no transport is simulated here.
#include "pc_p2_cargo.h"
#include "Generator.h"
#include <fstream>
#include <set>
static void verifyCargoReceipts() {
    std::ifstream in("p2-cargo.txt");auto specs=p2ReadCargo(in);
    require(pc_p2_preview_cargo_count()==int(specs.size()),"cargo registry count");
    std::set<PelletConfig*> configs;std::set<Shape*> shapes;
    const int before=pc_p2_preview_pokos(),repairs=playerState->getCurrParts();int value=0;
    for(size_t i=0;i<specs.size();++i) {
        Pellet* actor=pc_p2_preview_cargo_at(int(i));require(actor && actor->mGenerator,"cargo actor identity");
        require(actor->mGenerator->_70==specs[i].generator,"cargo generator binding");
        require(actor->mConfig->mCarryMinPikis()==specs[i].weight && actor->mConfig->mCarryMaxPikis()==specs[i].slots,"cargo per-actor weights");
        require(configs.insert(actor->mConfig).second,"cargo config shared between actors");
        require(shapes.insert(pc_p2_preview_cargo_shape(actor)).second && pc_p2_preview_cargo_shape(actor),"cargo model shared/missing");
        PelletConfig* global=pelletMgr->getConfig(actor->mConfig->mPelletId.mId);
        require(global && global!=actor->mConfig,"cargo mutates shared global config");
        // Constructor-owned intrusive parameter links must all remain in this clone.
        int nodes=0;for(BaseParm* node=actor->mConfig->mFirstParm;node;node=node->mNext) {
            uintptr_t address=reinterpret_cast<uintptr_t>(node),base=reinterpret_cast<uintptr_t>(actor->mConfig);
            require(address>=base && address<base+sizeof(PelletConfig),"cargo parameter link points outside clone");
            require(++nodes<32,"cargo parameter chain cycle");
        }
        require(nodes>10,"cargo incomplete parameter chain");
        require(pc_p2_preview_deliver(actor),"cargo delivery not handled");value+=specs[i].value;
    }
    require(pc_p2_preview_pokos()==before+value,"cargo receipt total");
    for(int i=0;i<pc_p2_preview_cargo_count();++i)require(pc_p2_preview_deliver(pc_p2_preview_cargo_at(i)),"repeat cargo not handled");
    require(pc_p2_preview_pokos()==before+value && playerState->getCurrParts()==repairs,"duplicate cargo credit or P1 repairs");
    std::printf("P2_CARGO_FIXTURE_PASS count=%zu added=%d duplicate_credit=0 repairs_unchanged=1 private_configs=1\n",specs.size(),value);
}
// Optional independent real transport check for the second floor-1 treasure.
// Assigns real Pikmin transport AI; no actor positions or receipt hooks changed.
static bool cargoCarryFixture(Navi* n) {
    static bool requested=[](){FILE* f=std::fopen("p2-cargo-carry.txt","r");if(!f)return false;std::fclose(f);return true;}();
    if(!requested)return false;
    static int frames=0,before=0,value=0,repairs=0;static Pellet* target=nullptr;
    ++frames;if(frames<90)return true;
    if(frames==90) {
        require(pc_p2_preview_cargo_count()>=2,"second cargo missing");target=pc_p2_preview_cargo_at(1);
        std::ifstream in("p2-cargo.txt");auto specs=p2ReadCargo(in);value=specs[1].value;
        before=pc_p2_preview_pokos();repairs=playerState->getCurrParts();int count=0;
        Iterator p(pikiMgr);CI_LOOP(p){Piki* v=static_cast<Piki*>(*p);if(!v->isAlive())continue;
            v->mActiveAction->abandon(nullptr);v->mActiveAction->mCurrActionIdx=PikiAction::Transport;
            v->mActiveAction->mChildActions[PikiAction::Transport].initialise(target);v->mMode=PikiMode::TransportMode;++count;
        }
        require(count>=5,"too few carriers for second cargo");
        std::printf("P2_CARGO_CARRY_START generator=%u weight=%d carriers=%d\n",target->mGenerator->_70,target->mConfig->mCarryMinPikis(),count);
    }
    if(!target->isAlive()) {
        require(pc_p2_preview_pokos()==before+value && playerState->getCurrParts()==repairs,"second cargo actual delivery accounting");
        capture("p2-cargo-carry.ppm");std::puts("PASS P2 second cargo actual native transport and Pod delivery");std::fflush(stdout);std::_Exit(0);
    }
    require(frames<6000,"second cargo transport timeout");return true;
}
