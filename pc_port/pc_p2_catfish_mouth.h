#pragma once
#include <string>
class BTeki;
bool pc_p2_catfish_mouth_resources(std::string& error);
bool pc_p2_catfish_mouth_birth(BTeki* actor,std::string& error);
bool pc_p2_catfish_mouth_follow(BTeki* actor,const std::string& clip,float sourceFrame);
int pc_p2_catfish_mouth_eat(BTeki* actor);
int pc_p2_catfish_mouth_swallow(BTeki* actor);
void pc_p2_catfish_mouth_release(BTeki* actor);
void pc_p2_catfish_mouth_forget(BTeki* actor);
