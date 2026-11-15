#include "../browser_script_heap.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
using Heap=browser::script::detail::ScriptHeap;
int main() {
    Heap heap;JSMallocState state{};state.opaque=&heap;
    heap.limit=sizeof(Heap::Header)+1536;
    auto* p=Heap::allocate(&state,1024);assert(p && heap.used==1024+sizeof(Heap::Header));
    assert(reinterpret_cast<std::uintptr_t>(p)%alignof(std::max_align_t)==0);
    std::memset(p,0xa5,1024);
    // The final block fits exactly, even though old+new blocks do not.
    p=Heap::resize(&state,p,1536);assert(p && !heap.failed && heap.used==heap.limit);
    for(unsigned i=0;i<1024;++i)assert(static_cast<unsigned char*>(p)[i]==0xa5);
    assert(state.malloc_count==1 && state.malloc_size==heap.used && heap.peak==heap.limit);
    const auto used=heap.used;
    assert(!Heap::resize(&state,p,1537) && heap.failed && heap.used==used);
    assert(Heap::usable(p)==1536 && static_cast<unsigned char*>(p)[0]==0xa5);
    heap.failed=false;
    p=Heap::resize(&state,p,256);assert(p && Heap::usable(p)==256 && state.malloc_count==1);
    assert(heap.used==256+sizeof(Heap::Header) && state.malloc_size==heap.used);
    assert(Heap::resize(&state,p,256)==p);
    const auto other_used=heap.used;
    auto* q=Heap::resize(&state,nullptr,64);assert(q && state.malloc_count==2);
    assert(!Heap::resize(&state,p,heap.limit- sizeof(Heap::Header)));
    assert(Heap::usable(p)==256 && heap.used==other_used+64+sizeof(Heap::Header));
    assert(!Heap::resize(&state,q,std::numeric_limits<std::size_t>::max()));
    assert(Heap::usable(q)==64 && state.malloc_count==2);
    assert(!Heap::resize(&state,q,0));Heap::release(&state,p);Heap::release(&state,nullptr);
    assert(!heap.used && !state.malloc_size && !state.malloc_count);
    std::cout<<"script heap PASS: exact-fit resize, data/alignment, shrink, failed-resize preservation, overflow and release\n";
}
