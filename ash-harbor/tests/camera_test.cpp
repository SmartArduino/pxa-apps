// SPDX-License-Identifier: MIT
#include "../camera.hpp"
#include <cassert>
#include <cstdio>
using namespace ash;
int main(){
    const int screens[][2]={{240,240},{296,240},{320,240},{240,480},{427,240},{640,240},{960,240},{480,480}};
    unsigned cases=0;
    for(auto& screen:screens){
        float scale=fitScale(screen[0],screen[1]);
        float ox=std::floor((screen[0]-320*scale)/2),oy=std::floor((screen[1]-240*scale)/2);
        assert(scale>0&&320*scale<=screen[0]+.001f&&240*scale<=screen[1]+.001f);
        for(float hudTop:{68.f,78.f,82.f})for(int character=0;character<4;++character)for(int pose=0;pose<48;++pose)
        for(int face:{-1,1})for(int x:{19,91,301})for(int jump:{0,35,68,90}){
            Fighter a,b;a.character=character;a.face=face;a.x=x;a.y=jump;
            b.character=(character+1)%4;b.face=-face;b.x=x<160?301:19;
            Bounds actors=unite(framingBounds(a,pose),framingBounds(b,(pose+13)%48));
            Bounds view{std::max(-96.f,-ox/scale)+4,hudTop,std::min(416.f,(screen[0]-ox)/scale)-4,236};
            auto camera=fitCamera(actors,view);
            assert(camera.zoom>0&&camera.zoom<=1);
            float left=camera.x+actors.left*camera.zoom,right=camera.x+actors.right*camera.zoom;
            float top=camera.y+actors.top*camera.zoom,bottom=camera.y+actors.bottom*camera.zoom;
            assert(left>=view.left-.01f&&right<=view.right+.01f&&top>=view.top-.01f&&bottom<=view.bottom+.01f);
            assert(ox+left*scale>=-.01f&&ox+right*scale<=screen[0]+.01f);
            assert(oy+top*scale>=-.01f&&oy+bottom*scale<=screen[1]+.01f);
            ++cases;
        }
    }
    for(int character=0;character<4;++character)for(int face:{-1,1}){
        Fighter a,b;a.character=character;a.face=face;a.x=19;b.x=301;b.character=(character+1)%4;
        Bounds view{4,82,316,236};Camera base=fitCamera(unite(framingBounds(a,0),framingBounds(b,0)),view);
        for(int frame=0;frame<48;++frame)if(frame<4||frame>=40){
            auto c=fitCamera(unite(framingBounds(a,frame),framingBounds(b,frame)),view);
            assert(c.zoom==base.zoom&&c.x==base.x&&c.y==base.y);
        }
    }
    std::printf("%u camera cases passed; breathing and neutral walk poses keep identical camera transforms.\n",cases);
}
