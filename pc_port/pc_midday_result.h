#pragma once
#include "pc_midday_player_core.h"
#include "pc_midday_constructor.h"
#include <memory>
struct ResultFlags;
namespace pc_midday {
struct ResultStageTag;
struct ResultDescriptor{int32_t screen=0,priority=0;uint32_t store=0;bool autoSet=false;};
struct ResultFields{std::array<uint8_t,38>states{};std::array<int16_t,30>days{};std::vector<ResultDescriptor>descriptors;};
bool encodeResult(const ResultFields&,Bytes&,std::string&);
bool decodeResult(const Bytes&,ResultFields&,std::string&);
bool captureResult(const ResultFlags&,const PlayerCoreReadFence&,Bytes&,std::string&);
class IsolatedResult {
 struct Impl;std::unique_ptr<Impl>impl_;
public:
 IsolatedResult();~IsolatedResult();
 bool prepare(const Bytes&,const RestoreGate&,ConstructorFence&,std::string&);
 ResultFlags*resource()const;
};
}
// Compiled topology is defined in resultFlag.cpp, where flagTable is complete.
// Private native allocation fields are read/bound only through this narrow bridge.
struct PcMiddayResultAccess {
 static bool compiled(std::vector<pc_midday::ResultDescriptor>&,std::string&);
 static bool read(const ResultFlags&,pc_midday::ResultFields&,std::string&);
 static void bind(ResultFlags&,uint8_t*,uint32_t*,const pc_midday::ResultFields&);
};
