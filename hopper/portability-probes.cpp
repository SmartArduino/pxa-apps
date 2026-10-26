/* SPDX-License-Identifier: MIT. Native-only original seconds format comparison. */
#include "owner_ascii_format.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <initializer_list>
static unsigned comparisons,guard_checks;
static bool seen[60];
static void compare(uint32_t frames){
 char prior[6],current[6];std::memset(prior,0x5a,6);std::memset(current,0x5a,6);
 const unsigned seconds=frames/60%60;
 // Defined native int varargs promotion for the original attainable 0..59.
 std::sprintf(prior,"'%02d\"",static_cast<int>(seconds));
 obono_format_seconds(current,sizeof(current),frames);
 if(std::memcmp(prior,current,6)){
  std::fprintf(stderr,"seconds mismatch frame=%u\n",frames);std::exit(1);
 }
 seen[seconds]=true;++comparisons;
}
int main(){
 // Full first cycle and full uint16 frame domain; uint32 endpoints and cycles.
 for(uint32_t frame=0;frame<=UINT16_MAX;++frame)compare(frame);
 for(uint32_t delta=0;delta<3600;++delta)compare(UINT32_MAX-delta);
 for(uint32_t remainder=0;remainder<3600;++remainder)compare(UINT32_C(2147482800)+remainder);
 for(uint32_t base: {UINT32_C(65536),UINT32_C(65537),UINT32_C(1073741824),UINT32_C(2147483647),UINT32_C(2147483648),UINT32_C(4294963200)})compare(base);
 for(unsigned i=0;i<60;++i)if(!seen[i])return 2;
 for(size_t capacity=0;capacity<5;++capacity){
  char buffer[6];std::memset(buffer,0x5a,6);
  obono_format_seconds(buffer,capacity,UINT32_MAX);
  for(size_t i=0;i<6;++i){
   if(buffer[i]!=(capacity&&i==0?0:0x5a))return 3;++guard_checks;
  }
 }
 std::printf("{\"passed\":true,\"seconds_comparisons\":%u,\"attainable_remainders\":60,\"capacity_guard_byte_checks\":%u,\"uint32_domain_entirely_enumerated\":false,\"original_output_bytes_compared\":6}\n",comparisons,guard_checks);
}
