#pragma once
#include "pc_p2_original_group.h"
class GeneratorList;
// Source installer calls prepare BEFORE any original GenObject stream read.
// Metadata includes every active/cache enemy group; no surrogate/AP bindings.
bool pc_p2_original_course_prepare(const std::string& fingerprint,
 const std::vector<p2original::CatalogRow>&,
 const std::vector<p2original::GeneratorState>& literal,
 std::function<bool(unsigned)> metColor,std::string&);
// Collect actual original objects in the native global list, validate the
// complete inventory, then install physical provider ownership before init.
bool pc_p2_original_course_start(GeneratorList*,std::string&);
bool pc_p2_original_course_finish(std::string&);
bool pc_p2_original_course_prepared();
// Optional explicit private manifest directory; no implicit fallback if set.
bool pc_p2_original_course_load(const char* directory,const char* course,
 std::function<bool(unsigned)> metColor,std::string&);
bool pc_p2_original_course_boot(const char* directory,const char* course,std::string&);
bool pc_p2_original_course_use_models(std::string&);
void pc_p2_original_course_retired(Creature*);
bool pc_p2_original_course_shadow(const Generator*);
