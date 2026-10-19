#include "../json.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
std::atomic<unsigned long> allocations=0;
void* operator new(std::size_t n){allocations.fetch_add(1,std::memory_order_relaxed);if(auto p=std::malloc(n))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
int main(){
 const std::string_view raw=R"({"bookInfo":{"bookId":"1","title":"Synthetic"},"curChapterUrlParam":{"param":"opaque"},"chapterInfoCount":300,"readerUrlParams":[{"cUid":1,"param":"opaque"}],"chapterIndexes":[{"chapterUid":1,"title":"Chapter"}],"curChapterSection":0})";
 const std::string_view keys[]={"bookInfo","curChapterUrlParam","chapterInfoCount","readerUrlParams","chapterIndexes","curChapterSection","missing"};
 const weread::Json object(raw);std::size_t sum=0;
 const auto before=allocations.load();const auto start=std::chrono::steady_clock::now();
 for(unsigned i=0;i<70000;++i)sum+=object.get(keys[i%7]).raw().size();
 const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
 std::printf("{\"lookups\":70000,\"nanoseconds\":%lld,\"allocations\":%lu,\"checksum\":%zu}\n",(long long)ns,allocations.load()-before,sum);
}
