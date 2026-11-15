#pragma once
#include "vendor/quickjs/quickjs.h"
#include <algorithm>
#include <cstddef>
#include <cstdlib>

namespace browser::script::detail {
// Charges live allocator blocks, including our aligned header. The ceiling
// is a quota; it never reserves a buffer of that size.
struct ScriptHeap {
    struct alignas(std::max_align_t) Header { std::size_t bytes; };
    std::size_t used=0, peak=0, limit=0;
    bool failed=false;
    static void* allocate(JSMallocState* state,std::size_t size) {
        auto& heap=*static_cast<ScriptHeap*>(state->opaque);
        if(!size || size>heap.limit || sizeof(Header)>heap.limit-size ||
           heap.used>heap.limit-size-sizeof(Header)) {heap.failed=true;return nullptr;}
        auto* h=static_cast<Header*>(std::malloc(sizeof(Header)+size));
        if(!h){heap.failed=true;return nullptr;}
        h->bytes=size;
        heap.used+=size+sizeof(Header);heap.peak=std::max(heap.peak,heap.used);
        state->malloc_size=heap.used;++state->malloc_count;
        return h+1;
    }
    static void release(JSMallocState* state,void* ptr) {
        if(!ptr)return;
        auto& heap=*static_cast<ScriptHeap*>(state->opaque);
        auto* h=static_cast<Header*>(ptr)-1;
        heap.used-=h->bytes+sizeof(Header);state->malloc_size=heap.used;
        --state->malloc_count;std::free(h);
    }
    static void* resize(JSMallocState* state,void* ptr,std::size_t size) {
        if(!ptr)return allocate(state,size);
        if(!size){release(state,ptr);return nullptr;}
        auto& heap=*static_cast<ScriptHeap*>(state->opaque);
        auto* old=static_cast<Header*>(ptr)-1;
        const auto retained=heap.used-old->bytes-sizeof(Header);
        if(size>heap.limit || sizeof(Header)>heap.limit-size ||
           retained>heap.limit-size-sizeof(Header)){heap.failed=true;return nullptr;}
        if(size==old->bytes)return ptr;
        // Let libc grow in place or reuse a smaller block. The old manual
        // allocate/copy/free path always copied and required both block sizes
        // to fit the JS quota, rejecting arrays whose final size would fit.
        // realloc failure leaves the original block and accounting intact.
        auto* next=static_cast<Header*>(std::realloc(old,sizeof(Header)+size));
        if(!next){heap.failed=true;return nullptr;}
        next->bytes=size;heap.used=retained+sizeof(Header)+size;
        heap.peak=std::max(heap.peak,heap.used);state->malloc_size=heap.used;
        return next+1;
    }
    static std::size_t usable(const void* ptr) {
        return ptr?(static_cast<const Header*>(ptr)-1)->bytes:0;
    }
};
}
