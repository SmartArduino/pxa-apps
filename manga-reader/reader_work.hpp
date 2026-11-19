#pragma once
#include <array>
#include <coroutine>
#include <cstdint>
#include <utility>

namespace manga {
// Cancellation-safe suspension until the reader has submitted its next frame.
// A clock round trip alone can resume many batches before another frame runs.
struct FrameWork {
    struct Entry {std::uint64_t ticket=0;std::coroutine_handle<> handle{};};
    std::array<Entry,4> waiting{};
    std::uint64_t sequence=0;
    void cancel(std::uint64_t ticket){for(auto& e:waiting)if(e.ticket==ticket)e={};}
    struct Awaiter {
        FrameWork& owner;std::uint64_t ticket=0;
        explicit Awaiter(FrameWork& value):owner(value){}
        Awaiter(const Awaiter&)=delete;
        ~Awaiter(){if(ticket)owner.cancel(ticket);}
        bool await_ready()const{return false;}
        bool await_suspend(std::coroutine_handle<> handle){
            for(auto& e:owner.waiting)if(!e.ticket){
                if(!++owner.sequence)++owner.sequence;
                ticket=owner.sequence;e={ticket,handle};return true;
            }return false;
        }
        bool await_resume(){auto value=std::exchange(ticket,0);if(value)owner.cancel(value);return value!=0;}
    };
    Awaiter yield(){return Awaiter{*this};}
    void advance(){
        const auto pending=waiting;
        for(auto scheduled:pending)if(scheduled.ticket){
            for(auto& e:waiting)if(e.ticket==scheduled.ticket){
                e={};scheduled.handle.resume();break;
            }
        }
    }
};
}
