#include "gl/pc_gfx.h"
#include <cassert>
#include <iostream>
#include <cstring>
extern "C" bool pc_gfx_halo_scope_cpu_control(const char**);
int main(){
 const char* error=nullptr;assert(pc_gfx_halo_scope_cpu_control(&error));
 // Real public API refuses absent context; it does not fabricate a scope.
 PcGfxHaloScope* scope=nullptr;assert(!pc_gfx_begin_halo_scope(scope,&error));assert(!scope&&error);
 pc_gfx_note_display_list_recording(true);pc_gfx_note_display_list_recording(true);
 assert(!pc_gfx_begin_halo_scope(scope,&error)&&!scope&&std::strstr(error,"display-list"));
 pc_gfx_note_display_list_recording(false);
 assert(!pc_gfx_begin_halo_scope(scope,&error)&&!scope&&std::strstr(error,"display-list"));
 pc_gfx_note_display_list_recording(false);
 assert(!pc_gfx_begin_halo_scope(scope,&error)&&!scope&&std::strstr(error,"GL context"));
 assert(!pc_gfx_end_halo_scope(scope,&error));assert(!scope&&error);
 auto* sentinel=reinterpret_cast<PcGfxHaloScope*>(1);scope=sentinel;
 assert(!pc_gfx_begin_halo_scope(scope,&error)&&scope==sentinel);
 assert(!pc_gfx_end_halo_scope(scope,&error)&&scope==sentinel);
 std::cout<<"Actual halo transport CPU roundtrip and absent-context refusal PASS (engineering only)\n";
}
