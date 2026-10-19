#include "../diagnostics.hpp"
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
std::atomic<unsigned> allocations=0;
void* operator new(std::size_t n){++allocations;if(auto p=std::malloc(n))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
int main(){
    using namespace weread;
    const auto before=allocations.load();
    auto line=diagnostic("WEREAD TEST",{{"signed",INT64_MIN},{"unsigned",UINT64_MAX},{"bool",true}});
    assert(line&&line->view()=="WEREAD TEST signed=-9223372036854775808 unsigned=18446744073709551615 bool=1");
    for(auto i=line->size;i<line->bytes.size();++i)assert(line->bytes[i]==0);
    assert(diagnostic("WEREAD TEST",{})->view()=="WEREAD TEST");
    std::array<char,257> long_text{};long_text.fill('a');
    assert(diagnostic(std::string_view(long_text).substr(0,256),{})->size==256);
    assert(!diagnostic(std::string_view(long_text),{}));
    assert(!diagnostic(std::string_view(long_text).substr(0,254),{{"a",1}}));
    assert(!diagnostic("",{})&&!diagnostic(std::string_view("A\0B",3),{}));
    assert(!diagnostic("P",{{"",1}})&&!diagnostic("P",{{std::string_view("A\0B",3),1}}));
    assert(allocations.load()==before);
    std::cout<<"Bounded numeric diagnostics: exact signed/unsigned values, overflow/NUL rejection, zero allocations\n";
}
