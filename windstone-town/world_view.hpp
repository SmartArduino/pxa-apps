#pragma once
#include "combat_animation.hpp"
#include "draw.hpp"

namespace wind {

inline constexpr Point East{18,10}, South{-18,10};
inline constexpr int Lift=16;
inline constexpr Point HomeCamera{31,153};
inline constexpr Point RidgeCamera{141,169}, SouthernCamera{-180,280};

inline Point grid_anchor(Point p) {
    return {East.x*p.x+South.x*p.y,East.y*p.x+South.y*p.y};
}
inline Point tile_center(const Game& game,Point p) {
    auto center=grid_anchor(p);
    center.y-=int(game.terrain(p).height)*Lift;
    return center;
}
inline Point tile_peak(const Game& game,Point p) {return tile_center(game,p)+Point{0,-East.y};}

inline bool on_face(Point at,Point origin,Point diagonal,int height) {
    if (height<=0) return false;
    const int x=(at.x-origin.x)*(diagonal.x>0?1:-1);
    if (x<0 || x>=std::abs(diagonal.x)) return false;
    const int top=origin.y+x*diagonal.y/std::abs(diagonal.x);
    return at.y>=top && at.y<top+height;
}

inline Point hit_tile(const Game& game,Point at) {
    for (int depth=Width+Height-2;depth>=0;--depth) {
        for (int x=std::max(0,depth-Height+1);x<=std::min(depth,Width-1);++x) {
            const Point p{x,depth-x};
            const auto center=tile_center(game,p);
            const auto diff=at-center;
            if (std::abs(diff.x)*East.y+std::abs(diff.y)*East.x<=East.x*East.y) return p;
            const auto elevation=[&](Point neighbor){return inside(neighbor)?game.terrain(neighbor).height:0;};
            const auto h=game.terrain(p).height;
            const Point peak=center+Point{0,-East.y};
            if (on_face(at,peak+East,South,(h-elevation(p+Point{1,0}))*Lift) ||
                on_face(at,peak+South,East,(h-elevation(p+Point{0,1}))*Lift)) return p;
        }
    }
    return {-1,-1};
}

enum Texture { Lawn, LeafLitter, Meadow, Frost, Path, Cliff, Shallows, Pool, Planks, Steps, Gravel, Autumn,
               CliffLower, CliffShadow, Cobbles, Bank, ShallowsMirror, PoolMirror };
enum Plant { Oak, AutumnOak, Pine, WinterPine, Shrub, Boulder, Tuft, Flowers, Signpost, Fountain, Footbridge, StoneGate, Ruins, Beacon, SurveyTent, FlowerBed,
             RailRising, RailFalling, ParapetRising, ParapetFalling, Braziers, CampSign, RiverFlowers, SmoothRocks,
             GardenPlanter, LampSign, StoneWell, MarketCart, BankPlants, Daisies, OldOak, CampTent, TallPine,
             NoticeBoard, SupplyCache, CavePost, OreCluster };
enum Effect { Flag=1, Smoke, Torch };
inline constexpr Rect tile_source(Texture texture,bool flip=false) {
    return {flip?TileWidth:0,int(texture)*TileHeight,TileWidth,TileHeight};
}
inline void ground_image(Draw& d,Texture texture,Point peak,bool flip=false) {
    d.blit(art::terrain,tile_source(texture,flip),peak+Point{-East.x,0});
}
inline Rect plant_source(Plant asset) {
    if (asset>=NoticeBoard) return {int(asset-NoticeBoard)*42,0,42,36};
    return {int(asset)%4*42,int(asset)/4*36,42,36};
}
inline const unsigned char* plant_atlas(Plant asset) {
    return asset>=NoticeBoard?art::expedition:art::plants;
}
inline void prop(Draw& d,Plant asset,Point base,bool flip=false) {
    const auto src=plant_source(asset);
    d.blit(plant_atlas(asset),src,base+Point{-src.w/2,-src.h},flip);
}
inline void effect(Draw& d,Effect type,int frame,Point base) {
    d.blit(art::effects,{(frame%4)*24,int(type)*26,24,26},base+Point{-12,-26});
}
inline void waterfall(Draw& d,const Game& game,Point pool,Point peak,unsigned time) {
    const auto land=game.terrain(pool);
    if (!pool.y || land.ground!=River) return;
    const auto head=game.terrain(pool+Point{0,-1});
    if (head.ground!=River || head.height!=land.height+1) return;
    constexpr int width=30,height=34;
    const int frame=time/125%4;
    // The lip is one story directly above the shared edge of the lower tile.
    const Point lip=peak-Point{0,Lift};
    d.blit(art::waterfall,{frame*width,0,width,height},lip+Point{-6,-4});
}
inline void relic(Draw& d,int id,Rect dest) {
    d.crop(art::relics,{id%3*28,id/3*28,28,28},dest);
}

enum class BuildingArt { Map, List, Preview };
inline void building_sprite(Draw& d,Kind kind,Point base,BuildingArt use=BuildingArt::Map) {
    if (kind>=Road) {
        d.blit(art::tools,{(int(kind)-int(Road))*22,0,22,17},base+Point{-11,-16});
        return;
    }
    const int y=use==BuildingArt::Map?0:use==BuildingArt::List?48:70;
    const int h=use==BuildingArt::Map?48:use==BuildingArt::List?22:34;
    d.blit(art::sprites,{48*kind,y,48,h},base+Point{-24,-h});
}
inline bool building_covers(Kind kind,Point base,Point p) {
    const auto offset=p-base+Point{24,48};
    return Rect{0,0,48,48}.contains(offset) &&
           *atlas_row(art::sprites,48*kind+offset.x,offset.y)!=255;
}
inline void adventurer_sprite(Draw& d,const unsigned char* atlas,Rect src,Point feet,bool large,bool flip=false) {
    if (large) d.blit_double(atlas,src,feet+Point{-src.w,-2*src.h},flip);
    else d.blit(atlas,src,feet+Point{-src.w/2,-src.h},flip);
}
inline void outfit_piece(Draw& d,const unsigned char* atlas,Rect src,Point at,bool large,bool flip=false) {
    if (large) d.blit_double(atlas,src,at,flip);
    else d.blit(atlas,src,at,flip);
}
inline void outfit(Draw& d,const Hero& hero,Point torso,bool large,bool back=false,bool flip=false) {
    const int scale=large?2:1;
    const auto armor=hero.equipment[int(ItemGroup::Armor)];
    if (armor!=Item::None) {
        const int x=(int(armor)-int(Item::ClothVest))*8;
        outfit_piece(d,art::worn_gear,{x,back?7:0,8,7},
                     torso+Point{-4*scale,-3*scale},large,flip);
    }
    const auto charm=hero.equipment[int(ItemGroup::Charm)];
    if (charm!=Item::None) {
        const int x=(int(charm)-int(Item::RiverPendant))*8+2;
        outfit_piece(d,art::worn_gear,{x,16,4,4},
                     torso+Point{(flip?-3:-1)*scale,-scale},large,flip);
    }
}
inline void travel_outfit(Draw& d,const Hero& hero,Point torso,bool large,bool back=false,bool flip=false) {
    const auto weapon=hero.equipment[int(ItemGroup::Weapon)];
    if (weapon!=Item::None) {
        constexpr Point grips[]{{3,7},{5,4},{3,6},{3,6}};
        const int index=int(weapon)-int(Item::IronSword);
        const auto grip=grips[index];
        const int scale=large?2:1;
        const int side=flip?-1:1;
        const auto hand=torso+Point{side*(back?-4:5)*scale,2*scale};
        outfit_piece(d,art::worn_gear,{index*8,21,8,10},
                     hand-Point{(flip?7-grip.x:grip.x)*scale,grip.y*scale},large,flip);
    }
    outfit(d,hero,torso,large,back,flip);
}
// Anchors are measured in the native pixels of the generated body and weapon cells.
inline constexpr Point CombatHand[HeroCount][4]{
    {{16,18},{17,15},{21,16},{16,19}},
    {{16,18},{18,14},{21,15},{15,16}},
    {{16,18},{18,14},{20,15},{16,19}},
    {{17,17},{18,13},{21,14},{17,18}},
    {{18,17},{18,14},{21,13},{16,19}},
    {{17,16},{18,12},{21,14},{18,18}},
    {{20,17},{20,11},{23,12},{19,18}},
    {{16,16},{18,12},{20,13},{16,18}},
};
inline constexpr Point WeaponGrip[4][4]{
    {{3,11},{7,11},{3,7},{3,4}},
    {{9,7},{9,7},{7,7},{10,7}},
    {{4,10},{7,11},{4,7},{3,4}},
    {{4,10},{7,11},{4,7},{4,4}},
};
inline void combat_hero(Draw& d,const Hero& hero,int id,int pose,Point feet,bool large) {
    const int scale=large?2:1;
    const Rect source{pose*26,id*24,26,24};
    const auto weapon=hero.equipment[int(ItemGroup::Weapon)];
    const auto* body=weapon==Item::None?art::combat:art::combat_body;
    const auto at=feet-Point{13*scale,24*scale};
    outfit_piece(d,body,source,at,large);
    outfit(d,hero,at+Point{13*scale,15*scale},large);
    if (weapon==Item::None) return;
    const int index=int(weapon)-int(Item::IronSword);
    const Point hand=CombatHand[id][pose], grip=WeaponGrip[index][pose];
    outfit_piece(d,art::held_weapons,{pose*16,index*16,16,16},
                 at+Point{(hand.x-grip.x)*scale,(hand.y-grip.y)*scale},large);
    // The front edge of the fist wraps over the handle.
    outfit_piece(d,body,{source.x+hand.x,source.y+hand.y,1,1},
                 at+Point{hand.x*scale,hand.y*scale},large);
}
inline Rect walk_frame(int hero,int frame,bool back=false) {
    return {frame*20,(hero+(back?HeroCount:0))*22,20,22};
}
inline void hero_sprite(Draw& d,int hero,Point feet,bool large=false) {
    adventurer_sprite(d,art::actors,walk_frame(hero,0),feet,large);
}
constexpr unsigned walking_duration(const Walker& walker) {
    return walker.walk_tile_beats*SimulationBeatMs;
}
constexpr unsigned walking_elapsed(const Walker& walker,unsigned interpolation) {
    return std::min(walking_duration(walker),(walker.walk_tile_beats-walker.walk_beats)*SimulationBeatMs+interpolation);
}
constexpr int walking_phase(const Walker& walker,unsigned elapsed) {
    const auto duration=walking_duration(walker);
    return elapsed<duration?((walker.at.x+walker.at.y)%2)*2+int(elapsed>=duration/2):-1;
}
inline bool walker_visible(const Game& game,int id) {
    const auto& t=game.s.trip;
    const bool underground=t.route!=Route::Frontier && (t.phase==Fighting || t.phase==Exploring);
    return game.s.heroes[id].level && !game.walkers[id].at_home && !(underground && (t.team&(1u<<id)));
}
inline bool fighting_outdoors(const Expedition& trip,int id) {
    return trip.route==Route::Frontier && trip.phase==Fighting && (trip.team&(1u<<id));
}
inline Point walker_feet(const Game& game,int id,unsigned interpolation) {
    const auto& w=game.walkers[id];
    const int age=int(walking_elapsed(w,interpolation)), duration=int(walking_duration(w));
    const auto from=tile_center(game,w.from), to=tile_center(game,w.at);
    return {from.x+(to.x-from.x)*age/duration+(id%3-1)*4,
            from.y+(to.y-from.y)*age/duration+(id%2)*3};
}

inline Texture grass_art(Tile tile,unsigned season) {
    if (season==3) return Frost;
    if (season==2) return Autumn;
    if (tile.ground==Forest) return LeafLitter;
    return tile.height?Meadow:Lawn;
}
inline int elevation(const Game& game,Point p) {
    return inside(p)?game.terrain(p).height:0;
}
inline void cliff_face(Draw& d,Point edge,Point run,int tiers,bool lit,bool flip,bool turf) {
    for (int tier=0;tier<tiers;++tier) {
        const int row=tier==0 && turf?(lit?0:3):(lit?1:2);
        const int column=(run.x>0?2:0)+int((tier%2)^flip);
        d.blit(art::walls,{column*18,row*26,18,26},
               edge+Point{std::min(0,run.x),tier*Lift});
    }
}
inline void cliff_faces(Draw& d,Point peak,int east_tiers,int south_tiers,bool flip=false,bool turf=true) {
    // The screen-left face catches the same upper-left light as the buildings.
    cliff_face(d,peak+East,South,east_tiers,false,flip,turf);
    cliff_face(d,peak+South,East,south_tiers,true,!flip,turf);
}
inline Texture road_art(Point tile,Tile land) {
    if (land.ground==Grass || land.ground==Plot) return Gravel;
    if ((tile.x>=6 && tile.x<=9 && tile.y==9) || (land.height>0 && land.ground==Street))
        return Cobbles;
    return Path;
}
inline void land_tile(Draw& d,const Game& game,Point tile,Point peak,unsigned season,unsigned time) {
    const Tile land=game.terrain(tile);
    const unsigned seed=texture(tile);
    const bool flip=seed&1;
    const bool river=land.ground==River || land.ground==Bridge;
    const int east_tiers=land.height-elevation(game,tile+Point{1,0});
    const int south_tiers=land.height-elevation(game,tile+Point{0,1});
    cliff_faces(d,peak,east_tiers,south_tiers,flip,!river && season<2);
    if (river) {
        unsigned shore=0;
        for (unsigned i=0;i<Directions.size();++i) {
            const auto neighbor=tile+Directions[i];
            if (!inside(neighbor)) continue;
            const auto bank=game.terrain(neighbor);
            if (bank.height==land.height && bank.ground!=River && bank.ground!=Bridge)
                shore|=1u<<i;
        }
        const bool mirror=tile.y%2;
        const auto water=land.ground==Bridge?(mirror?PoolMirror:Pool):(mirror?ShallowsMirror:Shallows);
        d.river_tile(tile_source(water,tile.x%2),tile_source(Bank,flip),peak+Point{-East.x,0},shore);
        if (land.ground==River && seed%3==0) {
            const int frame=(time/155+unsigned(4-tile.y%4))%4;
            d.blit(art::glints,{frame*22,0,22,10},peak+Point{-11,5},true);
        }
        return;
    }
    const auto surface=grass_art(land,season);
    if (!game.road(tile)) {
        ground_image(d,surface,peak,flip);
        return;
    }
    unsigned links=0;
    for (unsigned i=0;i<Directions.size();++i) {
        const auto next=tile+Directions[i];
        if (game.usable(tile,next) && land.height==elevation(game,next)) links|=1u<<i;
    }
    const bool square=tile.y==9 && tile.x>=6 && tile.x<=9;
    d.road_tile(tile_source(surface,flip),tile_source(road_art(tile,land),flip),
                peak+Point{-East.x,0},links,square);
}
inline void stone_steps(Draw& d,const Game& game,Point a,Point b,Point offset) {
    if (!game.usable(a,b) || elevation(game,a)==elevation(game,b)) return;
    const Point lower=elevation(game,a)<elevation(game,b)?a:b;
    const Point upper=lower==a?b:a;
    const auto q=tile_center(game,upper)+offset;
    const auto slope=tile_center(game,lower)+offset-q;
    const bool east_west=upper.x!=lower.x;
    const int column=east_west?(slope.x<0?0:1):(slope.x>0?2:3);
    const int across_y=east_west?-5:5;
    const Point at=q+Point{-5+std::min(0,slope.x),
                            -across_y/2+std::min(0,across_y)+slope.y+5-31};
    d.blit(art::stairs,{column*28,0,28,31},at);
}
inline void terrace_edges(Draw& d,const Game& game,Point p,Point peak) {
    const auto land=game.terrain(p);
    if (!land.height || !game.road(p)) return;
    const auto east=p+Point{1,0},south=p+Point{0,1};
    if (land.height>elevation(game,east) && !game.usable(p,east))
        prop(d,ParapetRising,peak+East+Point{South.x/2,South.y/2+2});
    if (land.height>elevation(game,south) && !game.usable(p,south))
        prop(d,ParapetFalling,peak+South+Point{East.x/2,East.y/2+2});
}

inline Plant forest_tree(unsigned season,unsigned seed) {
    if (season==3) return WinterPine;
    if (seed%3==0) return season==2?AutumnOak:(seed%9==0?OldOak:Oak);
    return seed%2==0?TallPine:Pine;
}

inline void draw_world(Draw& d,const Game& game,Rect viewport,Point camera,unsigned time,
                       unsigned interpolation,Point selection,bool placing,Kind kind,bool scenery_only=false,
                       const CombatAnimation* combat=nullptr) {
    const auto old=d.clip;
    d.clip=viewport;
    const unsigned season=(game.s.day-1)/12%4;
    const int backdrop=season==3?1:season==2?2:0;
    const Point offset=viewport.center()-camera;
    // Cover the finite village's edges with the surrounding forest floor.
    const auto corner=viewport.center()-Point{viewport.w/2+30+camera.x%36,viewport.h/2+30+camera.y%36};
    for (int y=corner.y;y<viewport.bottom();y+=36)
        for (int x=corner.x;x<viewport.right();x+=36)
            d.blit(art::backdrop,{backdrop*36,0,36,36},{x,y},bool((x/36+y/36)%2));
    const auto visible=[&](Point p,int padding) {
        return p.x>=viewport.x-padding && p.x<=viewport.right()+padding &&
               p.y>=viewport.y-padding && p.y<=viewport.bottom()+padding;
    };
    const auto pixel=[&](Point p){return tile_center(game,p)+offset;};

    struct Object {
        Point p{};
        int id=0, frame=0, depth=0;
        bool flip=false, back=false;
    };
    constexpr int WildObject=120+HeroCount;
    constexpr int FrontierObject=WildObject+int(WildSites.size());
    constexpr int max_props=Cells+HeroCount+int(WildSites.size())+1;
    std::array<Object,max_props> objects{};
    int count=0;
    std::array<int8_t,Cells> buildings;
    buildings.fill(-1);
    for (int i=0;i<game.s.count;++i) buildings[cell(game.s.buildings[i].at())]=int8_t(i);
    for (int y=0;y<Height;++y) for (int x=0;x<Width;++x) {
        const Point location{x,y}, p=pixel(location);
        if (!visible(p,48)) continue;
        const auto ground=game.terrain(location).ground;
        const auto seed=texture(location);
        const int built=buildings[cell(location)];
        const int depth=x+y;
        const bool cave=location==Cave || location==DepthsCave;
        if (built>=0 || cave) {
            const auto type=cave?CaveMouth:Kind(game.s.buildings[built].kind);
            objects[count++]={p,100+type,built,depth};
        } else if (location==Point{19,7}) objects[count++]={p,Beacon,0,depth};
        else if (location==Point{5,19}) objects[count++]={p,OldOak,0,depth};
        else if (location==Point{17,20}) objects[count++]={p,SurveyTent,0,depth};
        else if (location==GuildBoard) objects[count++]={p+Point{5,0},NoticeBoard,0,depth};
        else if (location==Point{14,8}) objects[count++]={p+Point{-4,2},SupplyCache,0,depth};
        else if (location==Point{16,7}) objects[count++]={p+Point{6,0},CavePost,0,depth};
        else if (location==Point{14,4}) objects[count++]={p+Point{-5,3},OreCluster,0,depth};
        else if (location==Point{17,7}) objects[count++]={p,SurveyTent,0,depth};
        else if (location==Point{7,1}) objects[count++]={p,Ruins,0,depth};
        else if (ground==Forest)
            objects[count++]={p+Point{int(seed%7)-3,2},forest_tree(season,seed),0,depth};
        else if (ground==Stone) objects[count++]={p,Boulder,0,depth};
        else if (ground==Bridge) objects[count++]={p-Point{0,2},Footbridge,0,depth};
        else if (location==Square) objects[count++]={p,StoneWell,0,depth};
        else if (location==Gate) objects[count++]={p,StoneGate,0,depth};
        else if (location==Point{11,9}) objects[count++]={p+Point{10,1},LampSign,0,depth};
        else if (location==Point{6,9}) objects[count++]={p+Point{-9,-1},MarketCart,0,depth};
        else if (location==Point{12,10} || location==Point{13,12})
            objects[count++]={p+Point{4,4},BankPlants,0,depth};
        else if (location==Point{7,7} || location==Point{6,8})
            objects[count++]={p,location==Point{7,7}?GardenPlanter:Daisies,0,depth};
        else if (!game.road(location) && ground!=River && seed%6<=1)
            objects[count++]={p+Point{int(seed%13)-6,int((seed>>6)%6)-1},
                              seed%7==0?Shrub:seed%2?Flowers:Tuft,0,depth};
    }
    for (int id=0;!scenery_only && id<HeroCount;++id) {
        if (!walker_visible(game,id)) continue;
        const auto& w=game.walkers[id];
        const auto p=walker_feet(game,id,interpolation)+offset;
        const auto a=tile_center(game,w.from), b=tile_center(game,w.at);
        const int elapsed=int(walking_elapsed(w,interpolation));
        if (visible(p,24))
            objects[count++]={p,120+id,walking_phase(w,unsigned(elapsed)),
                              std::max(w.from.x+w.from.y,w.at.x+w.at.y),b.x<a.x,b.y<a.y};
    }
    for (int i=0;!scenery_only && i<int(game.wild.size());++i) {
        if (!game.wild[i].hp) continue;
        const auto [x,y]=WildSites[i].at;
        const auto p=pixel({x,y})+Point{22,4};
        if (visible(p,54)) objects[count++]={p,WildObject+i,0,x+y};
    }
    if (!scenery_only && game.frontier_visible()) {
        const auto p=pixel(FrontierGround);
        if (visible(p,60)) objects[count++]={p,FrontierObject,0,FrontierGround.x+FrontierGround.y};
    }
    std::sort(objects.begin(),objects.begin()+count,[](const Object& a,const Object& b) {
        if (a.depth!=b.depth) return a.depth<b.depth;
        return a.p.y!=b.p.y?a.p.y<b.p.y:a.id<b.id;
    });
    int next=0;
    for (int depth=0;depth<Width+Height-1;++depth) {
        const int left=std::max(0,depth-Height+1),right=std::min(depth,Width-1);
        for (int x=left;x<=right;++x) {
            const Point tile{x,depth-x},peak=tile_peak(game,tile)+offset;
            if (visible(peak,62)) land_tile(d,game,tile,peak,season,time);
        }
        for (int x=left;x<=right;++x) {
            const Point tile{x,depth-x},peak=tile_peak(game,tile)+offset;
            if (!visible(peak,62)) continue;
            waterfall(d,game,tile,peak,time);
            if (!game.road(tile)) continue;
            stone_steps(d,game,tile,tile+Point{-1,0},offset);
            stone_steps(d,game,tile,tile+Point{0,-1},offset);
            terrace_edges(d,game,tile,peak);
        }
        while (next<count && objects[next].depth==depth) {
            const auto [p,id,frame,order,flip,back]=objects[next++];
            (void)order;
            if (id==FrontierObject) {
                const bool fighting=game.s.trip.route==Route::Frontier && game.s.trip.phase==Fighting;
                const int pose=fighting && combat?combat->guardian_pose(combat->age+interpolation):0;
                d.blit(art::monsters,{pose*54,FrontierSpecies*54,54,54},p-Point{27,54});
                icon(d,Compass,p.x-6,p.y-59-int(time/360%2),12);
            } else if (id>=WildObject) {
                const int site=id-WildObject;
                const auto& wild=game.wild[site];
                const int pose=wild.age>=WildImpact && wild.age<=WildImpact+1?1:
                               wild.age>=WildImpact+2 && wild.age<=WildImpact+4?3:0;
                d.blit(art::monsters,{pose*54,WildSites[site].species*54,54,54},p-Point{27,54});
                if (wild.age) {
                    d.fill({p.x-13,p.y-53,26,4},DeepShade);
                    d.fill({p.x-12,p.y-52,24*wild.hp/WildSites[site].health,2},rgb(0xe5ad57));
                }
            } else if (id>=120) {
                const int hero=id-120;
                if (combat && fighting_outdoors(game.s.trip,hero)) {
                    const unsigned animation=combat->age+interpolation;
                    const auto& member=game.s.heroes[hero];
                    const int pose=combat->ally_hp(member,hero,animation)>0?combat->ally_pose(animation):0;
                    combat_hero(d,member,hero,pose,p+Point{pose==2?3:0,2},false);
                    continue;
                }
                const auto& walker=game.walkers[hero];
                if (game.s.event==TownEvent::Drill && walker.festival_guest && walker.dwell) {
                    const int pose=int(time/130+hero)%4;
                    combat_hero(d,game.s.heroes[hero],hero,pose,p+Point{(hero%3-1)*3,2},false);
                    continue;
                }
                const int site=game.hunt_target(hero);
                const int age=site>=0?game.wild[site].age:0;
                if (age) {
                    const int pose=age<3?0:age<WildImpact?1:age<=WildImpact+1?2:age<=WildImpact+3?3:0;
                    combat_hero(d,game.s.heroes[id-120],id-120,pose,p+Point{0,4},false);
                    continue;
                }
                const auto* atlas=frame<0?art::actors:art::walking;
                const auto src=walk_frame(id-120,std::max(0,frame),back);
                d.shadow(atlas,{src.x,src.bottom()-3,src.w,3},p+Point{-src.w/2+2,0},DeepShade,6);
                adventurer_sprite(d,atlas,src,p,false,flip);
                const int bob=frame==1 || frame==3?1:0;
                travel_outfit(d,game.s.heroes[id-120],p+Point{flip?-1:1,-9-bob},false,back,flip);
            }
            else if (id>=100) {
                const auto type=Kind(id-100);
                d.shadow(art::sprites,{48*type,40,48,8},p+Point{-22,2},DeepShade,7);
                building_sprite(d,type,p+Point{0,7});
                if (frame>=0 && game.s.buildings[frame].level>1)
                    effect(d,Flag,time/125,p+Point{15,5});
                if (type==Inn || type==Forge)
                    effect(d,Smoke,time/150,p+Point{type==Inn?11:4,-24});
                if (type==CaveMouth) {
                    if (depth==DepthsCave.x+DepthsCave.y) icon(d,Gem,p.x-5,p.y-40);
                    const auto entrance=expedition_entrance(game.s.trip.route);
                    if ((game.s.trip.phase==Fighting || game.s.trip.phase==Exploring) &&
                        depth==entrance.x+entrance.y)
                        icon(d,Compass,p.x-5,p.y-50);
                }
            } else if (id==Footbridge) prop(d,Footbridge,p+Point{0,13});
            else {
                if (id!=Flowers && id!=Tuft && id!=Daisies) {
                    const auto sprite=plant_source(Plant(id));
                    d.shadow(plant_atlas(Plant(id)),{sprite.x,sprite.bottom()-5,sprite.w,5},
                             p+Point{-sprite.w/2+2,1},DeepShade,5);
                }
                prop(d,Plant(id),p+Point{0,5});
                if (id==NoticeBoard && game.s.commission.complete())
                    icon(d,Star,p.x-6,p.y-35,12);
            }
        }
    }
    if (game.s.festival) {
        for (const auto location:{Point{7,9},Point{9,9}}) {
            const auto p=pixel(location);
            effect(d,Flag,time/110,p+Point{1,3});
        }
        if (game.s.event==TownEvent::Feast)
            icon(d,Medicine,pixel(Square).x-5,pixel(Square).y-24);
    }
    if (inside(selection)) {
        const auto p=pixel(selection);
        const bool valid=!placing || game.placement(selection,kind)==Placement::Good;
        const Rect source{valid?0:TileWidth,0,TileWidth,TileHeight};
        d.blit(art::markers,source,p+Point{-East.x,-East.y});
        if (placing && earthwork(kind) && valid) {
            const Point peak=p+Point{0,-East.y+(kind==RaiseGround?-Lift:Lift)};
            if (kind==RaiseGround) cliff_faces(d,peak,1,1);
            ground_image(d,Meadow,peak);
            d.blit(art::markers,source,peak+Point{-East.x,0});
        } else if (placing && !earthwork(kind)) building_sprite(d,kind,p+Point{0,7},BuildingArt::Preview);
    }
    d.clip=old;
}
}
