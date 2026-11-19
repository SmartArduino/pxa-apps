#include "../reader_work.hpp"
#include <cassert>
struct Job {
    struct promise_type {
        Job get_return_object(){return {std::coroutine_handle<promise_type>::from_promise(*this)};}
        std::suspend_never initial_suspend(){return {};}
        std::suspend_always final_suspend()noexcept{return {};}
        void return_void(){} void unhandled_exception(){assert(false);}
    };
    std::coroutine_handle<promise_type> handle{};
    Job(std::coroutine_handle<promise_type> value):handle(value){}
    Job(const Job&)=delete;Job(Job&& other):handle(std::exchange(other.handle,{})){}
    ~Job(){if(handle)handle.destroy();}
};
Job count(manga::FrameWork& work,int& result,int n){for(int i=0;i<n;++i){assert(co_await work.yield());++result;}}
int main(){
    manga::FrameWork work;int a=0,b=0;
    auto first=count(work,a,3);
    {auto second=count(work,b,3);work.advance();assert(a==1&&b==1);}
    work.advance();assert(a==2&&b==1);work.advance();assert(a==3&&b==1);
    work.advance();assert(a==3&&b==1);
    // Cancelling a suspended decode must not leave a freed coroutine queued.
    {auto cancelled=count(work,a,1);}
    auto replacement=count(work,b,1);work.advance();assert(a==3&&b==2);
    for(auto e:work.waiting)assert(!e.ticket);
}
