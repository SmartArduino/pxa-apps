#include "ui.hpp"
#include "effects.hpp"

#include <cstdio>
#include <string_view>

namespace sky {

struct Glyph { char32_t cp; int advance; std::array<std::uint16_t,12> rows; };
#include "glyphs.inc"
#include "title_logo.inc"
#include "cockpit_art.inc"

// Pixel lettering for the cockpit selector.
constexpr Glyph cockpit_glyph{U'内',12,{0x020,0x020,0x7fe,0x422,0x422,0x462,
                                        0x452,0x48a,0x502,0x402,0x402,0x602}};
constexpr Color hud_dark=rgb(0x122a38), hud_line=rgb(0x568187), hud_cream=rgb(0xffefca);
constexpr Color mint=rgb(0xa5e1c5), ember=rgb(0xf88d5d), quiet=rgb(0xb6d2cb);

inline const Glyph* next_glyph(std::string_view& s) {
    const auto first=std::uint8_t(s.front());
    char32_t key=first;
    unsigned length=1;
    if (first>=0xe0 && s.size()>=3) {
        key=((first&0x0f)<<12) | ((std::uint8_t(s[1])&0x3f)<<6) | (std::uint8_t(s[2])&0x3f);
        length=3;
    }
    s.remove_prefix(length);
    if (key==cockpit_glyph.cp) return &cockpit_glyph;
    const auto* it=std::lower_bound(std::begin(glyphs),std::end(glyphs),key,
                                    [](const Glyph& a,char32_t b){return a.cp<b;});
    return it!=std::end(glyphs) && it->cp==key ? it : nullptr;
}
inline int text_width(std::string_view s,int scale=1) {
    int w=0;
    while (!s.empty()) {
        const auto* glyph=next_glyph(s);
        w+=(glyph ? glyph->advance : 6)*scale;
    }
    return w;
}
inline void text(Canvas& d,int x,int y,std::string_view s,Color c=hud_cream,int scale=1) {
    while (!s.empty()) {
        const auto* glyph=next_glyph(s);
        if (!glyph) {
            d.frame({x+1,y+scale,4*scale,8*scale},c);
            x+=6*scale;
            continue;
        }
        for (int j=0;j<12;++j) {
            unsigned bits=glyph->rows[j];
            while (bits) {
                const int k=__builtin_ctz(bits);
                d.fill({x+k*scale,y+j*scale,scale,scale},c);
                bits&=bits-1;
            }
        }
        x+=glyph->advance*scale;
    }
}
inline void centered(Canvas& d,int x,int y,std::string_view s,Color c=hud_cream,int scale=1) {
    text(d,x-text_width(s,scale)/2,y,s,c,scale);
}

void title_logo(Canvas& d,Rect area) {
    const int width=std::min({title_width,area.w-12,area.h*3/4});
    if (width<=0) return;
    const int height=std::max(1,width*title_height/title_width);
    const int x0=area.x+(area.w-width)/2,y0=area.y+std::max(8,area.h/12);
    for (int y=0;y<height;++y) {
        const auto* source=title_texels.data()+(y*title_height/height)*title_width;
        for (int x=0;x<width;++x) {
            const auto texel=source[x*title_width/width];
            if (texel) d.pixel(x0+x,y0+y,texel);
        }
    }
}

inline void panel(Canvas& d,Rect r,Color edge,int alpha=27) {
    d.tint(r,hud_dark,alpha);
    d.frame(r,hud_line);
    d.fill({r.x,r.y,std::min(11,r.w),1},edge);
    d.fill({r.x,r.y,1,std::min(7,r.h)},edge);
}
inline void button(Canvas& d,Rect r,std::string_view label,Color color=hud_cream,bool active=false) {
    panel(d,r,color,active ? 32 : 24);
    if (active) d.frame(r,color);
    centered(d,r.x+r.w/2,r.y+(r.h-12)/2,label,color);
}
inline void stick(Canvas& d,const Layout& l,Input input) {
    d.disk(l.stick,l.radius,hud_dark,23);
    const int inner=l.radius-4;
    constexpr Point dots[]{{100,0},{98,20},{92,38},{83,56},{71,71},{56,83},{38,92},{20,98}};
    for (const auto p:dots) {
        const int a=p.x*inner/100,b=p.y*inner/100;
        d.pixel(l.stick.x+a,l.stick.y+b,hud_line);
        d.pixel(l.stick.x-b,l.stick.y+a,hud_line);
        d.pixel(l.stick.x-a,l.stick.y-b,hud_line);
        d.pixel(l.stick.x+b,l.stick.y-a,hud_line);
    }
    for (int sign:{-1,1}) {
        d.line({l.stick.x+sign*(inner-7),l.stick.y},{l.stick.x+sign*inner,l.stick.y},mint);
        d.line({l.stick.x,l.stick.y+sign*(inner-7)},{l.stick.x,l.stick.y+sign*inner},mint);
    }
    const Point center{l.stick.x+int(input.turn*(inner-8)),l.stick.y-int(input.climb*(inner-8))};
    d.disk(center,7,hud_line);
    d.disk(center,5,mint,25);
    d.pixel(center.x,center.y,hud_dark);
}
inline void diamond(Canvas& d,Point c,int size,Color color) {
    d.line({c.x-size,c.y},{c.x,c.y-size},color);
    d.line({c.x,c.y-size},{c.x+size,c.y},color);
    d.line({c.x+size,c.y},{c.x,c.y+size},color);
    d.line({c.x,c.y+size},{c.x-size,c.y},color);
}
void friendly_marker(Canvas& d,Point p,int size) {
    constexpr Color teal=rgb(0x77ebd1);
    d.line({p.x-size,p.y+size},{p.x,p.y-size},teal);
    d.line({p.x,p.y-size},{p.x+size,p.y+size},teal);
    d.line({p.x+size,p.y+size},{p.x-size,p.y+size},teal);
}
const char* surface_name(const SurfaceUnit& unit) {
    switch (unit.role) {
    case SurfaceRole::ally_carrier: return "友舰";
    case SurfaceRole::enemy_carrier: return "敌航母";
    case SurfaceRole::gunboat: return "炮艇";
    case SurfaceRole::battery: return "岸炮";
    }
    return "";
}
inline void radar(Canvas& d,const Layout& l,const Game& g) {
    const int y=l.safe.y+35;
    const int x=l.safe.x+l.safe.w-20;
    d.disk({x,y+21},19,hud_dark,26);
    for (int i=-1;i<=1;++i) {
        d.pixel(x+i,y+4,mint); d.pixel(x+i,y+38,hud_line);
        d.pixel(x-17,y+21+i,hud_line); d.pixel(x+17,y+21+i,hud_line);
    }
    d.line({x-10,y+21},{x+10,y+21},hud_line);
    d.line({x,y+11},{x,y+31},hud_line);
    const auto axes=attitude(g.player.yaw);
    for (const auto& e:g.enemies) if (e.health) {
        Vec p=e.craft.pos-g.player.pos;
        const float sx=dot(p,axes.right),sy=dot(p,axes.front);
        const float factor=17/std::max(530.f,std::hypot(sx,sy));
        d.fill({x+int(sx*factor)-1,y+21-int(sy*factor)-1,3,3},ember);
    }
    if (g.wingman.hull) {
        const Vec p=g.wingman.craft.pos-g.player.pos;
        const float sx=dot(p,axes.right),sy=dot(p,axes.front);
        const float factor=16/std::max(250.f,std::hypot(sx,sy));
        friendly_marker(d,{x+int(sx*factor)-2,y+21-int(sy*factor)},2);
    }
    if (g.sortie==Sortie::escort) {
        const Vec p=g.convoy.pos-g.player.pos;
        const float sx=dot(p,axes.right),sy=dot(p,axes.front);
        const float factor=16/std::max(530.f,std::hypot(sx,sy));
        diamond(d,{x+int(sx*factor),y+21-int(sy*factor)},2,mint);
    }
    if (g.sortie==Sortie::fleet) for (const auto& unit:g.fleet.units) if (unit.hull) {
        const Vec p=unit.pos-g.player.pos;
        const float sx=dot(p,axes.right),sy=dot(p,axes.front);
        const float scale=16/std::max(680.f,std::hypot(sx,sy));
        diamond(d,{x+int(sx*scale),y+21-int(sy*scale)},unit.carrier() ? 3 : 2,
                unit.friendly() ? mint : ember);
    }
    d.fill({x-1,y+20,3,3},hud_cream);
}
inline void bracket(Canvas& d,Point p,int span,Color c) {
    for (int x:{-1,1}) for (int y:{-1,1}) {
        const Point corner{p.x+x*span,p.y+y*span};
        d.line(corner,{corner.x-x*4,corner.y},c);
        d.line(corner,{corner.x,corner.y-y*4},c);
    }
}
inline void sights(Canvas& d,const Game& g,const Layout& l,View view,int locked,int surface_lock) {
    const bool compact=l.compact();
    const auto cam=flight_camera(g,d.width,d.height,0,compact,view);
    const auto pose=attitude(g.player.yaw,g.player.pitch);
    const auto aim=cam.project(cam.local(g.player.pos+pose.front*(cam.overhead ? 31.f : 730.f)));
    const bool cockpit=view==View::cockpit;
    const auto bank=cockpit ? CanopyBank::project(g.player.bank,d.width,d.height) : CanopyBank{};
    const float eye_y=d.height*CanopyBank::eye_line;
    const Point a=cockpit ? Point{d.width/2,int(eye_y)} : Point{int(aim.x),int(aim.y)};
    const int hud_height=g.sortie!=Sortie::patrol ? 47 : compact ? 30 : 35;
    const int top=l.safe.y+hud_height+6;
    const int bottom=cockpit ? cockpit_deck_y(l,d.height)-7 : d.height-13;
    if (cockpit) {
        const auto track=attitude(g.player.yaw+g.side_slip,g.flight_path).front;
        const Vec direction=cam.local(g.player.pos+track*730.f);
        const Vec path=bank.to_screen(cam.project(direction),cam.cx,eye_y);
        const int x=int(path.x),y=int(path.y);
        if (x>=l.safe.x+13 && x<l.safe.x+l.safe.w-13 && y>=top+12 && y<bottom-7 &&
            (std::abs(x-a.x)>8 || std::abs(y-a.y)>9)) {
            constexpr Point rim[]{{-3,-1},{-1,-3},{1,-3},{3,-1},{3,1},{1,3},{-1,3},{-3,1},{-3,-1}};
            for (unsigned i=1;i<std::size(rim);++i)
                d.line({x+rim[i-1].x,y+rim[i-1].y},{x+rim[i].x,y+rim[i].y},mint);
            d.line({x-8,y},{x-4,y},mint);
            d.line({x+4,y},{x+8,y},mint);
            d.line({x,y-6},{x,y-4},mint);
        }
    }
    if (a.y>=top) {
        d.line({a.x-12,a.y},{a.x-5,a.y},hud_cream);
        d.line({a.x+5,a.y},{a.x+12,a.y},hud_cream);
        d.line({a.x,a.y+5},{a.x,a.y+10},hud_cream);
        d.pixel(a.x,a.y,mint);
        if (g.player.bank!=0 && !cam.overhead && !cockpit) {
            const float cx=std::cos(g.player.bank),sy=-std::sin(g.player.bank);
            for (int side:{-1,1}) {
                const float offset=side*18.f;
                const Point mark{a.x+int(offset*cx),a.y+int(offset*sy)};
                d.line(mark,{mark.x+int(5*side*cx),mark.y+int(5*side*sy)},quiet);
            }
        }
        if (g.hit_feedback>0) {
            const int inset=5+int(g.hit_feedback*9);
            const Color flash=g.hit_feedback>.23f ? mint : hud_cream;
            for (int sx:{-1,1}) for (int sy:{-1,1})
                d.line({a.x+sx*inset,a.y+sy*inset},{a.x+sx*(inset+3),a.y+sy*(inset+3)},flash);
        }
    }
    for (unsigned i=0;i<g.enemies.size();++i) {
        const auto& e=g.enemies[i];
        if (!e.health) continue;
        const Vec p=cam.local(e.craft.pos);
        if (p.z<25 || p.z>1800) continue;
        const auto projected=cam.project(p);
        const auto screen=cockpit ? bank.to_screen(projected,cam.cx,eye_y) : projected;
        if (screen.x<13 || screen.x>d.width-13 || screen.y<top || screen.y>bottom) continue;
        const bool is_lock=int(i)==locked;
        const Point mark{int(screen.x),int(screen.y)};
        const int size=std::clamp(int(cam.scale_at(p.z)*bank.extent*17),5,23)+(is_lock ? 3 : 0);
        if (compact && mark.y-size<top) continue;
        const auto& profile=enemy_profile(e.craft.type);
        bracket(d,mark,size,is_lock ? profile.accent : hud_cream);
        if (is_lock && !compact) {
            char dist[12];
            std::snprintf(dist,sizeof dist,"%s %d M",profile.code,int(std::sqrt(norm2(e.craft.pos-g.player.pos))));
            centered(d,mark.x,mark.y-size-13,dist,profile.accent);
        }
    }
    if (g.wingman.hull) {
        const Vec p=cam.local(g.wingman.craft.pos);
        if (p.z>Scene::near+5 && p.z<490) {
            const auto position=cam.project(p);
            const auto marker=cockpit ? bank.to_screen(position,cam.cx,eye_y) : position;
            const int x=int(marker.x),y=int(marker.y);
            const int lift=std::clamp(int(cam.scale_at(p.z)*bank.extent*7.5f),7,17);
            if (x>=l.safe.x+15 && x<l.safe.x+l.safe.w-15 && y-lift>top+15 && y<bottom-9) {
                friendly_marker(d,{x,y-lift},4);
                if (!compact) centered(d,x,y-lift-17,"友机",mint);
            }
        }
    }
    if (g.sortie==Sortie::fleet) {
        const SurfaceUnit* closest=nullptr;
        float closest_range=1e12f;
        bool closest_visible=false;
        for (unsigned i=0;i<g.fleet.units.size();++i) {
            const auto& unit=g.fleet.units[i];
            if (!unit.hull) continue;
            const auto p=cam.local(unit.aim());
            const auto projected=p.z>Scene::near ? cam.project(p) : Vec{-100,-100,0};
            const auto screen=cockpit ? bank.to_screen(projected,cam.cx,eye_y) : projected;
            const bool visible=screen.x>=l.safe.x+12 && screen.x<l.safe.x+l.safe.w-12 &&
                               screen.y>=top+10 && screen.y<std::min(bottom,l.boost.y-8);
            const float distance=norm2(unit.pos-g.player.pos);
            if (!unit.friendly() && distance<closest_range) {
                closest=&unit;
                closest_range=distance;
                closest_visible=visible;
            }
            if (!visible) continue;
            const Point marker{int(screen.x),int(screen.y)};
            const Color tint=unit.friendly() ? mint : ember;
            if (int(i)==surface_lock) bracket(d,marker,9,tint);
            else diamond(d,{marker.x,marker.y-5},4,tint);
            if (!compact || int(i)==surface_lock) {
                const int width=text_width(surface_name(unit));
                const int label_x=std::clamp(marker.x,l.safe.x+width/2,l.safe.x+l.safe.w-width/2);
                centered(d,label_x,marker.y-24,surface_name(unit),tint);
            }
        }
        if (surface_lock<0 && closest && !closest_visible) {
            const bool right=dot(closest->pos-g.player.pos,pose.right)>=0;
            char label[26];
            std::snprintf(label,sizeof label,right ? "%s %dM >" : "< %s %dM",surface_name(*closest),
                          int(std::sqrt(closest_range)));
            const int width=text_width(label)+8;
            button(d,{right ? l.safe.x+l.safe.w-width : l.safe.x,l.safe.y+l.safe.h*44/100,width,17},label,ember);
        }
    }
    if (g.sortie==Sortie::escort) {
        const auto p=cam.local(g.convoy.pos+Vec{0,11,0});
        const auto tint=g.convoy.damage_flash>0 ? ember : mint;
        if (p.z>Scene::near) {
            const auto projected=cam.project(p);
            const auto screen=cockpit ? bank.to_screen(projected,cam.cx,eye_y) : projected;
            const int floor=cockpit ? bottom : std::min(bottom,l.boost.y-9);
            if (screen.x>=l.safe.x+11 && screen.x<l.safe.x+l.safe.w-11 &&
                screen.y>=l.safe.y+62 && screen.y<floor) {
                const Point pos{int(screen.x),int(screen.y)-8};
                diamond(d,pos,4,tint);
                return;
            }
        }
        const auto offset=g.convoy.pos-g.player.pos;
        const bool right=dot(offset,pose.right)>=0;
        char bearing[24];
        const int meters=int(std::hypot(offset.x,offset.z));
        std::snprintf(bearing,sizeof bearing,right ? "船 %dM >" : "< 船 %dM",meters);
        const int width=text_width(bearing)+10;
        const int x=right ? l.safe.x+l.safe.w-width : l.safe.x;
        const int y=l.safe.y+l.safe.h*46/100;
        button(d,{x,y,width,17},bearing,tint);
    }
}

int cockpit_deck_y(const Layout& l,int height) {
    return std::max(height/2+14,std::min(height*67/100,l.eject.y-32));
}

void attitude_instrument(Canvas& d,Rect box,const Craft& craft) {
    const int radius=std::max(5,std::min(box.w,box.h)/2-4);
    const Point c{box.x+box.w/2,box.y+box.h/2};
    const float sine=std::sin(craft.bank),cosine=std::cos(craft.bank);
    const float pitch=radius*std::clamp(craft.pitch*1.3f,-.83f,.83f);
    constexpr Color blue=rgb(0x326a85),earth=rgb(0x9a6d49);
    for (int y=-radius;y<=radius;++y) {
        for (int x=-radius;x<=radius;++x) {
            const int r=x*x+y*y;
            if (r>radius*radius) continue;
            const float relative=y*cosine-x*sine-pitch;
            const Color color=r>(radius-1)*(radius-1) ? quiet :
                              std::abs(relative)<.8f ? hud_cream : relative<0 ? blue : earth;
            d.pixel(c.x+x,c.y+y,color);
        }
    }
    d.line({c.x-4,c.y},{c.x-1,c.y},ember);
    d.line({c.x+1,c.y},{c.x+4,c.y},ember);
    d.pixel(c.x,c.y+1,ember);
}

void cockpit_trim(Canvas& d,int top) {
    const int width=std::max(9,d.width/10);
    const int step=(cockpit_pillar_width<<14)/width;
    for (int y=0;y<top;++y) {
        const auto* source=cockpit_pillar_texels.data()+(y*cockpit_pillar_height/top)*cockpit_pillar_width;
        auto* destination=d.pixels+y*d.stride;
        bool trim=false;
        for (int x=0,u=0;x<width;++x,u+=step) {
            const Color pixel=source[u>>14];
            trim|=pixel!=0;
            if (!pixel && trim) continue;
            const auto color=pixel ? pixel : rgb(0x101d2b);
            destination[x]=color;
            destination[d.width-1-x]=color;
        }
    }
}

void cockpit_panel_skin(Canvas& d,int top) {
    const int step=(cockpit_panel_width<<14)/d.width;
    int previous=-1;
    for (int y=top;y<d.height;++y) {
        const int source_y=(y-top)*cockpit_panel_height/(d.height-top);
        auto* destination=d.pixels+y*d.stride;
        if (source_y==previous) {
            std::copy_n(destination-d.stride,d.width,destination);
            continue;
        }
        previous=source_y;
        const auto* source=cockpit_panel_texels.data()+source_y*cockpit_panel_width;
        for (int x=0,u=0;x<d.width;++x,u+=step) destination[x]=source[u>>14];
    }
}

void cockpit_instruments(Canvas& d,const Layout& l,const Game& g) {
    const int deck=cockpit_deck_y(l,d.height);
    constexpr Color bevel=rgb(0x425362),shadow=rgb(0x1b2b3a);
    cockpit_trim(d,deck+6);
    cockpit_panel_skin(d,deck+6);
    const int shoulder=d.width/7;
    d.fill({shoulder,deck,d.width-2*shoulder,2},bevel);
    d.fill({shoulder/2,deck+2,d.width-shoulder,4},shadow);

    const int left=l.roll.x+l.roll.w+6,right=l.safe.x+l.safe.w-2;
    const int y=std::min(deck+12,l.eject.y-29),height=std::clamp(l.eject.y-y-3,25,39);
    const int spacing=(right-left)/4;
    char number[12];
    for (int i=0;i<4;++i) {
        const Rect meter{left+spacing*i,y,spacing-3,height};
        panel(d,meter,mint,32);
        if (i==1) {
            attitude_instrument(d,meter,g.player);
            continue;
        }
        const int cx=meter.x+meter.w/2;
        const char* label=i==0 ? "SPD" : i==2 ? "ALT" : "HDG";
        const int reading=i==0 ? int(g.speed*3.6f) :
            i==2 ? int(std::max(0.f,g.player.pos.y-g.world.ground(g.player.pos))) :
                   (int(g.player.yaw*180/pi+360.f)%360);
        std::snprintf(number,sizeof number,"%03d",reading);
        centered(d,cx,meter.y+2,label,quiet);
        centered(d,cx,meter.y+14,number,mint);
    }
}

inline void hud(Canvas& d,const Layout& l,const Game& g,Input input,bool lock_failed,View view) {
    const int x=l.safe.x,y=l.safe.y,bottom=y+l.safe.h;
    const bool compact=l.compact();
    button(d,l.pause,"暂停",hud_cream);
    const View next=next_view(g.phase,view);
    button(d,l.camera,next==View::overhead ? "俯视" : next==View::cockpit ? "机内" : "追尾",mint);
    char info[36];
    if (g.phase==Phase::flight) {
        const bool escort=g.sortie==Sortie::escort;
        const bool fleet=g.sortie==Sortie::fleet;
        const auto deck=g.fleet.deck.phase;
        const bool in_deck_operation=deck!=DeckPhase::free;
        panel(d,{x,y,std::min(106,l.safe.w-42),escort || fleet ? 47 : compact ? 30 : 35},mint);
        if (escort)
            std::snprintf(info,sizeof info,"护航 %02d%%",int(g.convoy.progress*100));
        else if (fleet) std::snprintf(info,sizeof info,"目标 %d/3",3-g.fleet.remaining());
        else std::snprintf(info,sizeof info,"波次%u/3 击落%02u",g.wave,g.score);
        text(d,x+6,y+4,info);
        if (escort) {
            const Color tint=g.convoy.hull<=3 || g.convoy.damage_flash>0 ? ember : mint;
            text(d,x+6,y+17,"船体",tint);
            for (unsigned i=0;i<Convoy::max_hull;++i)
                d.fill({x+34+int(i)*7,y+20,5,5},i<g.convoy.hull ? tint : hud_line);
        } else if (fleet) {
            const auto& ship=g.fleet.units[0];
            std::snprintf(info,sizeof info,"友舰 %02d/10",ship.hull);
            text(d,x+6,y+17,info,ship.hull<=4 || ship.flash>0 ? ember : mint);
        }
        const int hull_y=y+(escort || fleet ? 30 : 17);
        text(d,x+6,hull_y,"机体",quiet);
        for (unsigned i=0;i<5;++i)
            d.fill({x+34+int(i)*11,hull_y+3,8,5},i<g.hull ? (g.hull<3 ? ember : mint) : hud_line);
        if (!compact) radar(d,l,g);
        const int surface=fleet && !in_deck_operation ? g.surface_lock() : -1;
        const int locked=surface<0 && !in_deck_operation ? g.target(.74f,930) : -1;
        if (!in_deck_operation) sights(d,g,l,view,locked,surface);
        button(d,l.fire,"机炮",in_deck_operation ? quiet : hud_cream,input.fire && !in_deck_operation);
        std::snprintf(info,sizeof info,"导弹 %u",g.rockets);
        button(d,l.missile,info,!in_deck_operation && (locked>=0 || surface>=0 || g.surface_aim()) && g.rockets ? mint : quiet);
        button(d,l.boost,"加速",mint,input.boost);
        const auto b=l.boost;
        d.fill({b.x+3,b.y+b.h-4,int((b.w-6)*g.energy),2},mint);
        button(d,l.eject,"弹射",ember);
        button(d,l.roll,"ROLL",input.roll ? ember : quiet,input.roll);
        const bool ready_to_land=fleet && g.can_approach_deck();
        if (fleet) {
            const char* action=deck==DeckPhase::parked ? "起飞" : deck==DeckPhase::approach ? "取消" : "着舰";
            button(d,l.deck,action,ready_to_land || deck==DeckPhase::parked ? mint : quiet,
                   ready_to_land || deck==DeckPhase::parked);
        }
        const bool low_altitude=g.player.pos.y-g.world.ground(g.player.pos)<27;
        if (in_deck_operation) {
            const int cx=x+l.safe.w/2;
            const char* status=deck==DeckPhase::approach ? "着舰进场" : deck==DeckPhase::arrest ? "甲板制动" :
                               deck==DeckPhase::parked ? "已停稳" : "甲板起飞";
            panel(d,{cx-43,y+52,86,20},mint);
            centered(d,cx,y+56,status,mint);
        } else if (low_altitude || lock_failed) {
            const int cx=x+l.safe.w/2;
            panel(d,{cx-49,y+45,98,21},ember);
            centered(d,cx,y+49,low_altitude ? "低空 拉升" : "未锁定目标",ember);
        } else if (g.hull<=1) {
            const int cx=x+l.safe.w/2;
            panel(d,{cx-41,y+46,82,20},ember);
            centered(d,cx,y+50,"机体重损",ember);
        } else if ((escort && g.convoy.resupply_flash>0) || (fleet && g.fleet.supply_flash>0)) {
            const int cx=x+l.safe.w/2;
            panel(d,{cx-46,y+52,92,21},mint);
            centered(d,cx,y+56,"补给完成",mint);
        } else if (g.wave_delay>0) {
            const int cx=x+l.safe.w/2;
            panel(d,{cx-54,y+48,108,21},mint);
            centered(d,cx,y+52,g.wave==3 ? "空域清除" : "下一波接近",mint);
        } else if ((g.rockets<4 || g.hull<5) &&
                   ((escort && !g.convoy.resupplied && norm2(g.convoy.pos-g.player.pos)<208.f*208.f) ||
                    (fleet && !g.fleet.supplied && norm2(g.fleet.units[0].pos-g.player.pos)<220.f*220.f))) {
            const int cx=x+l.safe.w/2;
            panel(d,{cx-45,y+52,90,20},mint);
            centered(d,cx,y+56,"低空接近补给",mint);
        }
    } else {
        panel(d,{x,y,std::min(103,l.safe.w-42),25},ember);
        const char* message=g.phase==Phase::pickup ? "救援机接收" :
            g.phase==Phase::parachute ? (g.rescue_visible ? "救援机接近" : "降落伞已展开") : "飞行员已弹射";
        text(d,x+6,y+6,message,g.phase==Phase::ejection ? ember : mint);
        if (g.phase==Phase::ejection) button(d,l.fire,"开伞",ember);
    }
    if (g.phase==Phase::flight || g.phase==Phase::parachute) stick(d,l,input);
    if (view==View::cockpit) return;
    const bool flight=g.phase==Phase::flight;
    const Vec p=flight ? g.player.pos : g.pilot;
    const int speed=flight ? int(g.speed*3.6f) : int(std::hypot(g.drift.x,g.drift.z)*3.6f);
    const int tape_w=compact ? 42 : 53;
    const int free=l.boost.x-(l.stick.x+l.radius);
    const int tape_x=compact ? x+l.safe.w-tape_w : free>=tape_w+8 ? l.boost.x-tape_w-5 : x;
    const int tape_y=compact ? y+54 : free>=tape_w+8 ? bottom-34 : l.roll.y-39;
    panel(d,{tape_x,tape_y,tape_w,34},mint);
    std::snprintf(info,sizeof info,"%03d M",std::max(0,int(p.y-g.world.ground(p))));
    centered(d,tape_x+tape_w/2,tape_y+2,info,mint);
    std::snprintf(info,sizeof info,compact ? "%03dKMH" : "%03d KM/H",speed);
    centered(d,tape_x+tape_w/2,tape_y+16,info);
}

void overlays(Canvas& d,const Layout& l,const Game& g,Menu menu,Input input,bool failed,View view) {
    view=active_view(g.phase,view);
    if (view==View::cockpit) {
        cockpit_instruments(d,l,g);
        if (g.player.damage.body || g.hull<=2) {
            const int direction=g.player.damage.missing_tip>0 ? -1 : 1;
            const int x=direction>0 ? l.safe.x+16 : l.safe.x+l.safe.w-16;
            const int y=d.height*31/100;
            const auto point=[&](int a,int b){return Point{x+a*direction,y+b};};
            const auto glass=rgb(0xa8c6ca);
            d.line(point(0,6),point(8,0),glass);
            d.line(point(8,0),point(16,3),glass);
            d.line(point(16,3),point(24,-4),glass);
            d.line(point(8,0),point(9,-6),glass);
            if (g.hull<=2) {
                d.line(point(16,3),point(21,11),glass);
                d.line(point(21,11),point(30,13),glass);
            }
        }
    }
    if (menu==Menu::none && g.in_air()) {
        if (g.phase==Phase::flight) {
            render_speed_streaks(d,g,view);
            if (g.damage_feedback>0) {
                const int alpha=int(g.damage_feedback*49);
                for (int i=0;i<4;++i) {
                    const int height=d.height*(5-i)/18,y=(d.height-height)/2;
                    d.tint({i,y,1,height},ember,alpha-i*alpha/5);
                    d.tint({d.width-1-i,y,1,height},ember,alpha-i*alpha/5);
                }
            }
        }
        hud(d,l,g,input,failed,view);
        return;
    }
    const auto b=l.safe;
    const int cx=b.x+b.w/2;
    const int panel_width=std::min(198,b.w+10);
    const Rect card{cx-panel_width/2,b.y+std::max(12,b.h/9),panel_width,66};
    if (menu==Menu::help) {
        const bool roomy=b.h>=182;
        Rect help{cx-panel_width/2,b.y+3,panel_width,roomy ? 137 : 123};
        panel(d,help,mint,31);
        centered(d,cx,help.y+6,"飞行操作",hud_cream);
        text(d,help.x+9,help.y+21,"摇杆或方向键转向与升降",mint);
        text(d,help.x+9,help.y+35,"机炮 A   导弹 B");
        text(d,help.x+9,help.y+49,"加速 X   弹射 Y");
        text(d,help.x+9,help.y+63,"暂停 START  视角 R");
        text(d,help.x+9,help.y+77,"开伞 A 或自动");
        text(d,help.x+9,help.y+91,"ROLL / L   转向键",mint);
        text(d,help.x+9,help.y+105,roomy ? "L/R 机型   Y 护航" : "SEL 舰队  Y 护航");
        if (roomy) text(d,help.x+9,help.y+119,"SELECT 舰队 / 着舰");
        button(d,l.primary,"返回",mint);
        return;
    }
    if (g.phase==Phase::title && menu==Menu::none) {
        title_logo(d,b);
        const auto& model=airframe(g.player.type);
        char selection[24];
        std::snprintf(selection,sizeof selection,"%s %s >",model.code,model.role);
        button(d,l.aircraft,selection,mint);
        button(d,l.escort,"海峡护航",mint);
        button(d,l.fleet,"舰队突袭",hud_cream);
        button(d,l.primary,"开始巡航",mint,true);
        button(d,l.secondary,"操作");
        return;
    }
    panel(d,card,mint);
    if (menu==Menu::paused) {
        centered(d,cx,card.y+15,"飞行暂停",hud_cream,2);
        button(d,l.primary,"继续飞行",mint,true);
        button(d,l.secondary,"操作");
        button(d,l.home,"返回标题");
    } else {
        const bool escort=g.sortie==Sortie::escort;
        const bool fleet=g.sortie==Sortie::fleet;
        const char* title=g.phase==Phase::rescued ? (g.airlift ? "空中获救" : g.wet ? "水上获救" : "安全着陆")
                          : g.phase==Phase::victory ? (fleet ? "突袭完成" : escort ? "护航完成" : "空域清除")
                          : g.phase==Phase::escort_lost ? (fleet ? "友舰失守" : "运输船失守") : "飞行失事";
        centered(d,cx,card.y+10,title,hud_cream,2);
        char summary[36];
        if (escort && g.phase==Phase::victory)
            std::snprintf(summary,sizeof summary,"击落 %02u   船体 %u/%u",g.score,g.convoy.hull,Convoy::max_hull);
        else if (escort) std::snprintf(summary,sizeof summary,"击落 %02u   护航 %02d%%",g.score,int(g.convoy.progress*100));
        else if (fleet) std::snprintf(summary,sizeof summary,"目标 %d/3  击落 %02u",3-g.fleet.remaining(),g.score);
        else if (g.wingman_score)
            std::snprintf(summary,sizeof summary,"击落 %02u   友机 %02u",g.score,g.wingman_score);
        else std::snprintf(summary,sizeof summary,"击落 %02u   波次 %u/3",g.score,g.wave);
        centered(d,cx,card.y+44,summary,mint);
        button(d,l.primary,"再次出击",mint,true);
        button(d,l.secondary,"返回");
    }
}

} // namespace sky
