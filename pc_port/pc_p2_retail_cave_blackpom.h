#pragma once
#include "pc_p2_retail_cave_native.h"
#include "pc_p2_original_blackpom_native.h"
namespace p2retail {
// Actual SAVE/party owner must query current physical and cached Purple counts
// against this independently selected floor. No inferred/default population.
using BlackPomPopulation=std::function<bool(const Snapshot&,p2original::blackpom::BirthContext&,std::string&)>;
using BlackPomConsumer=std::function<bool(Pom*,const p2original::InstanceIdentity&,unsigned,std::string&)>;
// Consumer installs actual donor snapshot/head provenance before source start.
bool bindBlackPom(NativeFloor&,p2original::blackpom::Native&,BlackPomPopulation,BlackPomConsumer,std::string&);
}
