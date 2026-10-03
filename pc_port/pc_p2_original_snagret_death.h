#pragma once
namespace p2original { namespace bulblax_snagret {
// SnakeCrowState::StateDead KEYEVENT_3 authored131 delivers at observed132;
// KEYEVENT_END at165 retires the actor. SnakeCrow's deathProcedure hook is empty.
// Only original SnakeCrow uses this policy; inherited AP/P1 paths are unchanged.
struct DeathItems {
 bool emitted=false;
 bool key(bool original,bool delivered){if(!original||emitted||!delivered)return false;emitted=true;return true;}
};
} }
