#include "classic-raster.h"
#include <cassert>
#include <cstdio>
#include <vector>
int main() {
    for(unsigned width=160;width<=240;width+=80) {
        unsigned height=width*3/4;
        std::vector<uint16_t> memory(320*240+2,0xa55a),source(width*height);
        for(unsigned i=0;i<source.size();++i)memory[i+1]=source[i]=uint16_t(i*113u^(i/7u));
        ol_classic_upscale(memory.data()+1,width);
        assert(memory.front()==0xa55a && memory.back()==0xa55a);
        for(unsigned y=0;y<240;++y)for(unsigned x=0;x<320;++x)
            assert(memory[1+y*320+x]==source[(y*height/240)*width+x*width/320]);
    }
    puts("scale:240x180 and160x120 match independent nearest-neighbor reference; in-place guards intact");
}
