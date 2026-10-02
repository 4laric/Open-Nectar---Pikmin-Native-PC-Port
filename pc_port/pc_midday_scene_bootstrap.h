#pragma once
#include "pc_midday_codec.h"
#include "pc_midday_restore.h"
namespace pc_midday {
// Named replacements for GameCoreSection's campaign-lifetime progression flags.
// These defaults exactly match the former function statics; no reset on scene load.
struct SceneBootstrap {
    bool colorGranted[3]={false,false,false};
    bool initialColorRegistered=false,redsQueued=false,redsReady=false,gameplayReady=false;
    int32_t initialField=20;
};
SceneBootstrap& sceneBootstrap(); // gameplay thread only, same as prior statics
bool encodeSceneBootstrap(const SceneBootstrap&,Bytes&,std::string&);
bool decodeSceneBootstrap(const Bytes&,SceneBootstrap&,std::string&);
// Destination is backend-owned staging, never sceneBootstrap() before publication.
// The scene backend must publish the staged record with the world in its final
// nonthrowing commit, or discard it on abort; this helper cannot publish globals.
bool applySceneBootstrap(SceneBootstrap& staged,const Bytes&,const RestoreGate&,std::string&);
}
