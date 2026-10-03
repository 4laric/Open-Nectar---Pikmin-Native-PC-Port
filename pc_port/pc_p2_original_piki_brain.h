#pragma once
#include "pc_p2_original_piki_runtime.h"
namespace p2original { namespace piki {
bool brainFree(Handle,RuntimeState&,Services&,std::string&);
bool brainFormation(Handle,RuntimeState&,Services&,Navi*,std::string&);
bool brainExec(Handle,RuntimeState&,Services&,const Parameters&,float,std::string&);
bool brainCleanup(Handle,RuntimeState&,Services&,std::string&);
bool brainCleanupAll(Handle,RuntimeState&,Services&,std::string&);
} }
