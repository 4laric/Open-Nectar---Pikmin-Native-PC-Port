#pragma once
#include "pc_p2_cave_campaign_party.h"

// An optional living surface checkpoint inside the native campaign envelope.
// Existing day-boundary saves have no extension. The card's hash authenticates
// this descriptor and the shared typed party together with its cache/stock.
struct P2SurfaceSession {
    bool present=false;
    int stage=-1, index=-1, day=-1;
    std::string file;
    P2CaveCampaignParty party;
    bool valid() const {
        if (!present) return stage==-1 && index==-1 && day==-1 && file.empty() && !party.present;
        if (stage<0 || stage>=5 || index<0 || index>=5 || day<0 || day>=30
            || file.size()>255 || file.rfind("stages/",0)!=0 || file.find("..")!=std::string::npos
            || file.find('\\')!=std::string::npos || !party.present || party.inside
            || !party.resumeLiving || party.landing || !party.valid()) return false;
        for (unsigned char c:file) if (c<=32 || c>=127) return false;
        return true;
    }
    bool read(std::istream& in) {
        P2SurfaceSession next; std::string marker,version;
        if (!(in>>marker>>version>>next.stage>>next.index>>next.day>>next.file)
            || marker!="SURFACE_SESSION" || version!="1" || !next.party.read(in)) return false;
        next.present=true;
        if (!next.valid()) return false;
        *this=std::move(next); return true;
    }
    void write(std::ostream& out) const {
        if (!present) return;
        out<<" SURFACE_SESSION 1 "<<stage<<' '<<index<<' '<<day<<' '<<file;
        party.write(out);
    }
};
