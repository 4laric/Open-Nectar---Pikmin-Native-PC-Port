#include "pc_p2_original_selected_shape_stream.h"
#include <cstdio>
#include <stdexcept>
#include <type_traits>
namespace { unsigned checks=0;void require(bool b,const char* text){++checks;if(!b)throw std::runtime_error(text);} template<class F>void refused(F f,const char* text){bool caught=false;try{f();}catch(const std::runtime_error&){caught=true;}require(caught,text);} }
int main(){try{
using p2original::SelectedShapeStream;
static_assert(!std::is_constructible<SelectedShapeStream,std::string&&,const char*>::value,"retained bytes cannot be temporary");
const std::string bytes("\x12\x34\x56\x78\x80\x01\x3f\x80\x00\x00\x7f",11);const auto copy=bytes;
SelectedShapeStream s(bytes,"selected-role");require(s.getPosition()==0&&s.getLength()==11&&s.getAvailable()==11,"initial retained extent");
require(s.readInt()==0x12345678,"actual native big endian int");require(s.readShort()==static_cast<short>(0x8001),"actual signed native short");require(s.readFloat()==1.0f,"actual native IEEE float");require(s.readByte()==0x7f,"actual native byte");require(s.getPosition()==11&&s.getPending()==0,"native primitives consume exact retained buffer");
char sentinel='x';refused([&]{s.read(&sentinel,1);},"EOF refuses");require(sentinel=='x'&&s.getPosition()==11,"EOF keeps output and cursor");
s.setPosition(1);refused([&]{s.read(&sentinel,-1);},"negative read refuses");require(s.getPosition()==1&&sentinel=='x',"negative no change");refused([&]{s.read(nullptr,1);},"null destination refuses");require(s.getPosition()==1,"null no advance");s.read(nullptr,0);require(s.getPosition()==1,"zero null read inert");
refused([&]{s.setPosition(-1);},"negative seek refuses");refused([&]{s.setPosition(12);},"past extent seek refuses");require(s.getPosition()==1,"bad seeks transactional");
refused([&]{s.write(&sentinel,1);},"raw write forbidden");refused([&]{s.writeInt(42);},"actual native int write forbidden");require(bytes==copy&&s.getPosition()==1,"retained bytes immutable");
s.setPosition(0);require(s.readIntFrom(0)==0x12345678&&s.getPosition()==4,"actual native positional primitive advances from requested offset");s.setPosition(9);refused([&]{s.readInt();},"truncated int refuses");require(s.getPosition()==9,"truncated primitive no advance");
const std::string empty;refused([&]{SelectedShapeStream bad(empty,"empty");},"empty retained stream refused");
std::printf("P2_SELECTED_SHAPE_STREAM PASS %u (actual native Stream primitives; no Shape/GL runtime)\n",checks);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}

