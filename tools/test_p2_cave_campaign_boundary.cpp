#include "pc_p2_cave_campaign_boundary.h"
#include <cstdlib>
#include <iostream>
#include <limits>
int main(){
    unsigned checks=0;
    auto check=[&](bool passed){++checks;if(!passed)std::exit(1);};
    P2CaveBoundarySnapshot live;live.ready=true;live.seed=17;live.sceneGeneration=3;
    live.checkpointGeneration=2;live.cave="forest_1";live.token="seed-owned";
    check(p2CaveBoundaryMatches(live,live));
    auto refuse=[&](P2CaveBoundarySnapshot changed){check(!p2CaveBoundaryMatches(changed,live));};
    auto changed=live;changed.ready=false;refuse(changed);
    changed=live;++changed.seed;refuse(changed);
    changed=live;++changed.sceneGeneration;refuse(changed);
    changed=live;++changed.checkpointGeneration;refuse(changed);
    changed=live;changed.action=P2CaveBoundaryAction::Return;refuse(changed);
    changed=live;changed.floor=1;refuse(changed);
    changed=live;changed.cave="foreign";refuse(changed);
    changed=live;changed.token="foreign";refuse(changed);
    changed=live;changed.x=1;refuse(changed);
    changed=live;changed.z=1;refuse(changed);
    changed=live;changed.radius=81;refuse(changed);
    changed=live;changed.x=std::numeric_limits<float>::quiet_NaN();refuse(changed);
    changed=live;changed.ready=false;check(!p2CaveBoundaryMatches(live,changed));
    changed=live;changed.sceneGeneration=0;check(!p2CaveBoundaryMatches(changed,changed));
    live.action=P2CaveBoundaryAction::Return;live.floor=1;
    check(p2CaveBoundaryMatches(live,live));
    changed=live;changed.floor=0;check(!p2CaveBoundaryMatches(changed,changed));
    std::cout<<checks<<" boundary identity controls PASS\n";
}
