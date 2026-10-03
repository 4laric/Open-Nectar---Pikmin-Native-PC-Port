#pragma once
#include "pc_p2_authored_cave_session.h"
bool pc_p2_authored_piki_catalog_validate(const P2AuthoredCaveRoute&,const std::string& runRoot,std::string& error);
bool pc_p2_authored_piki_catalog_bind(const P2AuthoredCaveRoute&,int stage,const char* file,int offset,unsigned sourceUid,unsigned& stableUid);
bool pc_p2_authored_piki_catalog_saved(const P2AuthoredCaveRoute&,unsigned stableUid,unsigned sourceUid);
bool pc_p2_authored_piki_catalog_contains(const P2AuthoredCaveRoute&,unsigned stableUid);
unsigned pc_p2_authored_piki_catalog_uid(std::uint64_t seed,const std::string& token,const std::string& generatorSha,int stage,unsigned offset,unsigned sourceUid);
