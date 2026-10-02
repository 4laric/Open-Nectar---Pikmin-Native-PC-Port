#include "pc_midday_player_root.h"
namespace pc_midday {
bool validatePlayerRootCore(const PlayerCoreFields& v,const PlayerCoreTopology& t,std::string& e){
 if(!validatePlayerCoreTopology(v,t,e))return false;
 if(v.totalParts!=30||v.totalRegisteredParts<0||v.totalRegisteredParts>30){e="PlayerState native ship registration topology invalid";return false;}
 if(v.unused186>1){e="PlayerState native bool186 byte is noncanonical; refusing normalization";return false;}
 e.clear();return true;
}
}
