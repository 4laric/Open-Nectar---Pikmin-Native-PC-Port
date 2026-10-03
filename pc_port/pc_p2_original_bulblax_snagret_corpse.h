#pragma once
#include <string>
namespace p2original { namespace bulblax_snagret {
using CorpseResources=bool(*)(unsigned,std::string&);
inline bool requireCorpse(CorpseResources resources,unsigned source,std::string& error){
 if(!resources){error="original Bulblax/Snagret typed corpse resource callback missing";return false;}
 if(source!=33&&source!=34){error="original Bulblax/Snagret typed corpse source invalid";return false;}
 return resources(source,error);
}
}}
