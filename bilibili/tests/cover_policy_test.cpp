// SPDX-License-Identifier: MIT
#include "../cover_policy.hpp"
#include <cassert>
int main(){
    bili::CoverPolicy p;
    assert(p.admit(0));assert(!p.admit(999));
    for(unsigned i=1;i<20;++i)assert(p.admit(i*1000));
    assert(!p.admit(20000));assert(!p.admit(59999));assert(p.admit(60000));
    bili::CoverPolicy boundary;
    for(unsigned i=0;i<20;++i)assert(boundary.admit(42000+i*1000));
    assert(!boundary.admit(62000));assert(!boundary.admit(101999));assert(boundary.admit(102000));
    p.defer(180000);assert(!p.admit(179999));assert(p.admit(180000));
    assert(p.fail(180001,true)==185001);assert(!p.admit(185000));
    for(unsigned i=0;i<100;++i)assert(p.fail(200000,false)<=500000);
    assert(bili::CoverPolicy::retry_after("3600",60000)==3600000);
    assert(bili::CoverPolicy::retry_after("1",60000)==60000);
    assert(bili::CoverPolicy::retry_after("Wed, 18 Nov 2026 10:00:00 GMT",60000)==300000);
    assert(bili::CoverPolicy::retry_after("999999999999999999999",60000)==300000);
}
