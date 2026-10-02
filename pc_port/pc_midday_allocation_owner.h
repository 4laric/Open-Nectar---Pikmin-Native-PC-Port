#pragma once
#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace pc_midday {
// Disposable transaction storage. Each entry retains its concrete destructor;
// deleting a non-virtual native base never substitutes for typed ownership.
// Constructors supplied here must either own their internal allocations with
// normal C++ RAII or allocate them separately through this owner. Ordinary
// native constructors with unowned nested new expressions remain unsuitable.
class AllocationOwner {
 struct Entry {
  Entry* previous=nullptr;
  void* object=nullptr;
  std::size_t count=0;
  void (*cleanup)(void*,std::size_t) noexcept=nullptr;
  void (*destroy)(Entry*) noexcept=nullptr;
 };
 template<class T> struct ObjectEntry:Entry {
  alignas(T) unsigned char storage[sizeof(T)];
  T* pointer(){return std::launder(reinterpret_cast<T*>(storage));}
  static void dispose(Entry* entry) noexcept {
   auto* typed=static_cast<ObjectEntry*>(entry);
   typed->pointer()->~T();delete typed;
  }
 };
 template<class T> struct ArrayEntry:Entry {
  T* data=nullptr;
  ~ArrayEntry(){delete[] data;}
  static void dispose(Entry* entry) noexcept {delete static_cast<ArrayEntry*>(entry);}
 };
 template<class T> struct ConstructedArrayEntry:Entry {
  T* data=nullptr;
  std::size_t constructed=0,capacity=0;
  ~ConstructedArrayEntry(){
   while(constructed)data[--constructed].~T();
   if(data)std::allocator<T>{}.deallocate(data,capacity);
  }
  static void dispose(Entry* entry) noexcept {delete static_cast<ConstructedArrayEntry*>(entry);}
 };
 Entry* head_=nullptr;
 std::size_t entries_=0,attempts_=0;
 std::size_t failAt_;
 bool clearing_=false;
 void attempt(){
  if(clearing_)throw std::logic_error("allocation during owner cleanup");
  if(attempts_++==failAt_)throw std::bad_alloc();
 }
 void adopt(Entry* entry) noexcept {entry->previous=head_;head_=entry;++entries_;}
public:
 // Zero-based allocation attempt: object entries have one allocation, arrays
 // have metadata and native new[] allocations. STL allocations inside T are
 // owned by T but are not intercepted by this deterministic injection seam.
 explicit AllocationOwner(std::size_t failAt=std::numeric_limits<std::size_t>::max()) noexcept:failAt_(failAt){}
 AllocationOwner(const AllocationOwner&)=delete;
 AllocationOwner&operator=(const AllocationOwner&)=delete;
 ~AllocationOwner(){reset();}

 template<class T,class Factory> T* construct(Factory&& constructInPlace){
  static_assert(!std::is_array<T>::value,"use array for arrays");
  attempt();std::unique_ptr<ObjectEntry<T>> entry(new ObjectEntry<T>);
  // Placement-construction failure unwinds T's completed members. The metadata
  // guard frees its block; no destructor is called for an unconstructed T.
  std::forward<Factory>(constructInPlace)(static_cast<void*>(entry->storage));
  T* value=entry->pointer();entry->object=value;entry->count=1;
  entry->destroy=&ObjectEntry<T>::dispose;adopt(entry.release());return value;
 }
 template<class T,class... Args> T* make(Args&&... args){
  return construct<T>([&](void* storage){return new(storage)T(std::forward<Args>(args)...);});
 }
 template<class T> T* array(std::size_t count){
  if(!count)throw std::invalid_argument("empty owned array");
  if(count>std::numeric_limits<std::size_t>::max()/sizeof(T))throw std::bad_array_new_length();
  attempt();std::unique_ptr<ArrayEntry<T>> entry(new ArrayEntry<T>);
  attempt();entry->data=new T[count]();entry->object=entry->data;entry->count=count;
  entry->destroy=&ArrayEntry<T>::dispose;auto* value=entry->data;adopt(entry.release());return value;
 }
 template<class T,class Factory> T* arrayConstruct(std::size_t count,Factory&& constructElement){
  if(!count)throw std::invalid_argument("empty owned array");
  if(count>std::numeric_limits<std::size_t>::max()/sizeof(T))throw std::bad_array_new_length();
  attempt();std::unique_ptr<ConstructedArrayEntry<T>> entry(new ConstructedArrayEntry<T>);
  entry->capacity=count;attempt();entry->data=std::allocator<T>{}.allocate(count);
  for(std::size_t i=0;i<count;++i){
   // Callback does exactly one placement construction, with no work after it.
   constructElement(static_cast<void*>(entry->data+i),i);++entry->constructed;
  }
  entry->object=entry->data;entry->count=count;entry->destroy=&ConstructedArrayEntry<T>::dispose;
  T* value=entry->data;adopt(entry.release());return value;
 }
 // Install immediately after creation, before any later fallible allocation.
 // All detach hooks execute while every owned allocation still exists, then
 // concrete destruction executes in reverse allocation order. Hooks must only
 // sever independently owned links, never delete objects or allocate storage.
 void setCleanup(void* object,void(*cleanup)(void*,std::size_t) noexcept){
  if(clearing_)throw std::logic_error("cleanup registration during owner cleanup");
  for(Entry* entry=head_;entry;entry=entry->previous)if(entry->object==object){entry->cleanup=cleanup;return;}
  throw std::invalid_argument("cleanup target is not owned");
 }
 void reset() noexcept {
  if(clearing_)std::terminate();clearing_=true;
  for(Entry* entry=head_;entry;entry=entry->previous)if(entry->cleanup)entry->cleanup(entry->object,entry->count);
  while(head_){Entry* entry=head_;head_=entry->previous;--entries_;entry->destroy(entry);}
  clearing_=false;
 }
 std::size_t ownedEntries()const noexcept{return entries_;}
 std::size_t allocationAttempts()const noexcept{return attempts_;}
};
}
