#ifndef PC_P2_ORIGINAL_SELECTED_SHAPE_STREAM_H
#define PC_P2_ORIGINAL_SELECTED_SHAPE_STREAM_H
#include "Stream.h"
#include <string>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace p2original {
// The actual native Shape reader consumes precisely this retained buffer.
// Stream's endian primitives remain the production implementations.
class SelectedShapeStream final:public RandomAccessStream {
public:
    SelectedShapeStream(const std::string& bytes,const char* role):mBytes(bytes){
        if(bytes.empty()||bytes.size()>static_cast<size_t>(std::numeric_limits<int>::max()))
            throw std::runtime_error("invalid selected Shape stream extent");
        mPath=role;
    }
    SelectedShapeStream(std::string&&,const char*) = delete;
    void read(void* out,int size) override {
        if(size<0||(size&&!out)||static_cast<size_t>(size)>mBytes.size()-mPosition)
            throw std::runtime_error("selected Shape read exceeds authenticated bytes");
        if(size)std::memcpy(out,mBytes.data()+mPosition,static_cast<size_t>(size));
        mPosition+=static_cast<size_t>(size);
    }
    void write(immut void*,int) override {throw std::runtime_error("selected Shape stream is immutable");}
    int getPosition() override {return static_cast<int>(mPosition);}
    int getLength() override {return static_cast<int>(mBytes.size());}
    int getAvailable() override {return getLength()-getPosition();}
    void setPosition(int position) override {
        if(position<0||static_cast<size_t>(position)>mBytes.size())
            throw std::runtime_error("selected Shape seek exceeds authenticated bytes");
        mPosition=static_cast<size_t>(position);
    }
private:
    const std::string& mBytes;
    size_t mPosition=0;
};
}
#endif
