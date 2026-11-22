#include "../world_view.hpp"

#include <cassert>

using namespace wind;

static_assert(walking_elapsed(Walker{.walk_beats=7},0)==0);
static_assert(walking_elapsed(Walker{.walk_beats=6},49)==99);
static_assert(walking_elapsed(Walker{.walk_beats=1},98)==350);
static_assert(walking_phase(Walker{.at={8,8}},174)==0 && walking_phase(Walker{.at={8,8}},175)==1);
constexpr Walker expedition_walk{.at={8,9},.walk_beats=5,.walk_tile_beats=5};
static_assert(walking_elapsed(expedition_walk,49)==49);
static_assert(walking_phase(expedition_walk,0)==2 && walking_phase(expedition_walk,125)==3 &&
              walking_phase(expedition_walk,250)==-1);

int main() {
    constexpr int height=Draw::FolderTabHeight;
    constexpr Color clear=rgb(0x3657ad);
    std::array<std::byte,224*100*2> buffer{};
    auto surface=*pxa::Rgb565Pixels::from_bytes(buffer,224,100,448);
    Draw draw(surface);
    for (const bool selected:{false,true}) for (const bool focused:{false,true}) {
        const int source_x=selected?0:78;
        const int source_y=90+(focused?height:0);
        for (const int width:{66,79,113}) {
            surface.fill(clear);
            draw.folder_tab({3,2,width,height},selected,focused);
            for (int y=0;y<height;++y) for (int x=0;x<width;++x) {
                const int sx=x<12?x:x>=width-12?78-(width-x):12+(x-12)%54;
                const auto index=*atlas_row(art::ui,source_x+sx,source_y+y);
                const auto drawn=Color((*surface.row(y+2))[x+3]);
                assert(drawn==(index==255?clear:indexed_color(art::ui,index)));
                // Even when stretched across the catalog, the closed lower
                // edge must stay opaque right up to both corners.
                if (!selected && y==height-1) assert(drawn!=clear);
            }
        }
        if (selected) {
            const auto paper=*atlas_row(art::ui,39,source_y+height-1);
            assert(indexed_color(art::ui,paper)==Draw::paper_color());
        }
    }
    const Rect left{5,2,104,height},right{111,2,104,height},page{5,2+height-1,210,75};
    for (const bool select_left:{false,true}) for (const bool focused:{false,true}) {
        const Rect active=select_left?left:right;
        const Rect inactive=select_left?right:left;
        surface.fill(clear);
        draw.folder_page(page,active);
        draw.folder_tab(active,true,focused);
        draw.folder_tab(inactive,false,focused);
        const auto* profile=atlas_row(art::ui,0,90+height-1);
        for (int i=0;i<Draw::FolderRim;++i) {
            const auto rim=indexed_color(art::ui,profile[i]);
            const auto border=page.inset(i);
            const auto seam=*surface.row(border.y);
            for (int x=border.x;x<border.right();++x) {
                const bool in_opening=x>active.x+i && x<active.right()-1-i;
                assert((Color(seam[x])!=rim)==in_opening);
            }
            const auto side=*surface.row(page.center().y);
            assert(Color(side[border.x])==rim && Color(side[border.right()-1])==rim);
            assert(Color((*surface.row(border.bottom()-1))[page.center().x])==rim);
        }
        const auto join=*surface.row(page.y);
        assert(Color(join[active.center().x])==Draw::paper_color());
        // The inactive tab and the page use the same lower stroke, at the
        // same y. The row above it is still shaded paper.
        assert(Color((*surface.row(page.y-1))[inactive.center().x])!=indexed_color(art::ui,profile[0]));
    }
    for (const Text name:{"铺面","地形与道路","2星解锁","上一页"}) {
        const Rect label{10,8,58,12};
        surface.fill(clear);
        const auto line=Draw::fit(label,name);
        assert(line.value==name);
        draw.line(label,line,TextInk,true);
        for (int y=0;y<100;++y) for (int x=0;x<224;++x)
            if (!label.contains({x,y})) assert(Color((*surface.row(y))[x])==clear);
    }
}
