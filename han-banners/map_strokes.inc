// Antialiased map strokes use sparse coverage tiles in one GameRender texture.
class MapStrokeMask {
public:
    struct Layer {int from=0,to=0;};
    static constexpr int edge=256;

    bool begin(Rect area) {
        bounds=area;
        columns=(area.w+tile-1)/tile;
        rows=(area.h+tile-1)/tile;
        if (area.w<=0 || area.h<=0 || columns*rows>int(cells.size())) return false;
        count=0;
        layerTag=0;
        overflow=false;
        std::fill_n(cells.begin(),columns*rows,uint16_t(0));
        return true;
    }

    void beginLayer() {layerTag+=indexMask+1;}
    int size() const {return count;}
    bool overflowed() const {return overflow;}

    void stroke(float x,float y,float u,float v,float width,float opacity) {
        if (overflow) return;
        if (std::abs(u-x)>=std::abs(v-y)) capsule<false>(x,y,u,v,width*.5f,opacity);
        else capsule<true>(y,x,v,u,width*.5f,opacity);
    }

    pxa::Result<void> upload(pxa::game::Renderer& renderer,int slot) {
        const int height=std::max(1,(count+perRow-1)/perRow)*tile;
        return pxa::game::Upload(renderer,data).texture({uint8_t(slot)},edge,uint16_t(height),
                                                       texels().first(edge*height));
    }

    void draw(pxa::game::Frame& frame,int slot,Layer layer,Rect clip,int dx,int dy,uint16_t color) const {
        std::array<pxa::game::Sprite,64> sprites;
        size_t used=0;
        const auto flush=[&] {
            frame.sprites({uint8_t(slot)},std::span(sprites).first(used),
                          {.solid_color=true,.texel_alpha=true,.color={color}});
            used=0;
        };
        for (int i=layer.from;i<layer.to;++i) {
            const int x=origins[i].first+dx,y=origins[i].second+dy;
            const int left=std::max(x,clip.x),right=std::min(x+tile,clip.x+clip.w);
            const int top=std::max(y,clip.y),bottom=std::min(y+tile,clip.y+clip.h);
            if (left>=right || top>=bottom) continue;
            sprites[used++]={int16_t(left),int16_t(top),uint16_t(right-left),uint16_t(bottom-top),
                uint16_t((i%perRow)*tile+left-x),uint16_t((i/perRow)*tile+top-y),
                uint16_t(right-left),uint16_t(bottom-top)};
            if (used==sprites.size()) flush();
        }
        flush();
    }

private:
    static constexpr int tile=8,perRow=edge/tile,maxTiles=992;
    static constexpr uint16_t indexMask=1023;
    static_assert(maxTiles<=indexMask && maxTiles%perRow==0);
    std::array<std::byte,edge*tile*(maxTiles/perRow)+pxa::game::Upload::header_bytes> data{};
    std::array<std::pair<int16_t,int16_t>,maxTiles> origins{};
    std::array<uint16_t,9000> cells{};
    Rect bounds{};
    int columns=0,rows=0,count=0;
    uint16_t layerTag=0;
    bool overflow=false;

    std::span<std::byte> texels() {
        return std::span(data).subspan<pxa::game::Upload::header_bytes>();
    }
    void sample(int x,int y,uint8_t opacity) {
        const int localX=x-bounds.x,localY=y-bounds.y;
        auto& cell=cells[(localY/tile)*columns+localX/tile];
        if ((cell&~indexMask)!=layerTag) {
            if (count==maxTiles) {overflow=true;return;}
            origins[count]={int16_t(x-localX%tile),int16_t(y-localY%tile)};
            auto* first=texels().data()+(count/perRow)*tile*edge+(count%perRow)*tile;
            for (int row=0;row<tile;++row) std::fill_n(first+row*edge,tile,std::byte{});
            ++count;
            cell=layerTag+count;
        }
        const int index=(cell&indexMask)-1;
        auto& pixel=texels()[((index/perRow)*tile+localY%tile)*edge+(index%perRow)*tile+localX%tile];
        pixel=std::max(pixel,std::byte(opacity));
    }

    template<bool Vertical>
    void capsule(float a,float b,float c,float d,float radius,float opacity) {
        if (a>c) {std::swap(a,c);std::swap(b,d);}
        const float run=c-a;
        const float slope=run>0?(d-b)/run:0;
        const float axis=1+slope*slope,inverse=1.f/axis;
        const float outer=radius+.5f,outerSquared=outer*outer;
        const float inner=std::max(0.f,radius-.5f),innerSquared=inner*inner;
        const float band=outer*std::sqrt(axis);
        const int majorMin=Vertical?bounds.y:bounds.x,minorMin=Vertical?bounds.x:bounds.y;
        const int majorEnd=majorMin+(Vertical?bounds.h:bounds.w);
        const int minorEnd=minorMin+(Vertical?bounds.w:bounds.h);
        const int first=std::max(majorMin,int(std::floor(a-outer)));
        const int end=std::min(majorEnd,int(std::ceil(c+outer)));
        for (int i=first;i<end && !overflow;++i) {
            const float along=i+.5f-a;
            const float middle=b+std::clamp(along,0.f,run)*slope;
            const int low=std::max(minorMin,int(std::floor(middle-band)));
            const int high=std::min(minorEnd,int(std::ceil(middle+band)));
            for (int j=low;j<high && !overflow;++j) {
                const float across=j+.5f-b;
                const float point=std::clamp((along+across*slope)*inverse,0.f,run);
                const float du=along-point,dv=across-point*slope;
                const float square=du*du+dv*dv;
                if (square>=outerSquared) continue;
                const float alpha=square<=innerSquared && radius>=.5f?1.f:outer-std::sqrt(square);
                const auto level=uint8_t(alpha*opacity+.5f);
                if (level) sample(Vertical?j:i,Vertical?i:j,level);
            }
        }
    }
};
