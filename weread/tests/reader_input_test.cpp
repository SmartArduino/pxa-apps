#include "../reader_input.hpp"
#include <cassert>
#include <iostream>
#include <limits>
#include <string>
int main(){
    using weread::ReaderInput;
    ReaderInput input;assert(input.size()==0&&input.capacity()==0&&input.view().empty());
    const std::string expected(950096,'x');
    for(std::size_t at=0;at<expected.size();at+=257)
        assert(input.append(std::string_view(expected).substr(at,257),1310720));
    assert(input.view()==expected&&input.capacity()<=1310720);
    const auto before=std::string(input.view());
    assert(!input.append(expected,1310720)&&input.view()==before);
    assert(!input.reserve(1310721,1310720)&&input.view()==before);
    ReaderInput moved(std::move(input));assert(input.view().empty()&&input.capacity()==0&&moved.view()==before);
    input=std::move(moved);assert(moved.view().empty()&&input.view()==before);
    // Self-move must preserve bytes and ownership.
    input=std::move(input);assert(input.view()==before);
    input={};assert(input.view().empty()&&input.capacity()==0);
    assert(input.append("retry",5)&&input.view()=="retry");
    assert(!input.append("!",5)&&input.view()=="retry");
    std::cout<<"Owned reader input: fragmented growth, bounds, moves, cancellation release and retry passed\n";
}
