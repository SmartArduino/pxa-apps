#pragma once

#include "game.hpp"
#include <pxa/app.hpp>

namespace tide {

class PilotAtlas {
public:
    struct Row {
        std::array<unsigned char,PilotWidth/8> bits{};
        bool loaded=false;

        bool land(int x) const {
            return x>=0 && x<PilotWidth && (bits[x/8]&(128>>(x&7)));
        }
    };

    PilotWater water(int patch) const {
        return {collision_patch==patch?collision.data():nullptr,true};
    }

    bool failed(int patch) const { return patch==failed_patch; }

    Row row(int patch,int y) const {
        Row result;
        if (y<0 || y>=PilotHeight) return result;
        const auto* tile=find(patch);
        if (!tile || !tile->size) return result;
        if (tile->reduced) {
            constexpr int stride=PilotWidth/16;
            for (int i=0;i<stride;++i) {
                const auto b=tile->pixels[(y/2)*stride+i];
                result.bits[i*2]=expand(b>>4);
                result.bits[i*2+1]=expand(b&15);
            }
        } else {
            const int group=y/16;
            unpack_row(std::span(tile->pixels).first(tile->size),y,result.bits,tile->index[group]);
        }
        result.loaded=true;
        return result;
    }

    void prepare(pxa::Context& ctx,Point ship,Point camera,int priority,bool& dirty) {
        if (priority>=0 && collision_patch!=priority) {
            if (const auto* source=find(priority);source && source->size && !source->reduced) {
                if (decode_pilot_tile(std::span(source->pixels).first(source->size),collision))
                    collision_patch=priority;
            }
        }
        if (busy) return;
        std::array<int,9> wanted{};
        int count=0;
        auto add=[&](int patch) {
            if (patch>=0 && patch<PilotTileCount && count<int(wanted.size()) &&
                std::find(wanted.begin(),wanted.begin()+count,patch)==wanted.begin()+count)
                wanted[count++]=patch;
        };
        auto neighbours=[&](int patch) {
            const int x=patch%PilotTilesX,y=patch/PilotTilesX;
            for (const auto step : {Point{-1,0},Point{1,0},Point{0,-1},Point{0,1},
                                    Point{-1,-1},Point{1,-1},Point{-1,1},Point{1,1}}) {
                const int column=x+step.x,row=y+step.y;
                if (column>=0 && column<PilotTilesX && row>=0 && row<PilotTilesY)
                    add(row*PilotTilesX+column);
            }
        };
        const int ship_patch=pilot_patch_at({ship.x/8,ship.y/8});
        const int camera_patch=pilot_patch_at({camera.x/8,camera.y/8});
        add(priority);
        add(ship_patch);
        add(camera_patch);
        neighbours(camera_patch);
        neighbours(ship_patch);
        for (const int patch : std::span(wanted).first(count)) {
            if (patch==failed_patch) continue;
            auto* slot=find(patch);
            const bool physics=patch==priority && collision_patch!=patch;
            if (slot && !physics) continue;
            if (!slot) {
                auto replace=std::find_if(tiles.begin(),tiles.end(),[&](auto& tile) {
                    return std::find(wanted.begin(),wanted.begin()+count,tile.patch)==wanted.begin()+count;
                });
                if (replace==tiles.end()) break;
                slot=&*replace;
                slot->patch=patch;
                slot->size=0;
            }
            busy=true;
            if (!ctx.tasks().start(read_tile(ctx.assets(),*slot,physics,dirty))) {
                busy=false;
                if (!slot->size) slot->patch=-1;
            }
            break;
        }
    }

private:
    struct Run { std::uint16_t offset=0, produced=0; };
    struct Tile {
        std::array<unsigned char,1024> pixels{};
        std::array<Run,PilotHeight/16> index{};
        int patch=-1;
        std::uint16_t size=0;
        bool reduced=false;
    };

    Tile* find(int patch) {
        for (auto& tile : tiles) if (tile.patch==patch) return &tile;
        return nullptr;
    }
    const Tile* find(int patch) const {
        for (const auto& tile : tiles) if (tile.patch==patch) return &tile;
        return nullptr;
    }

    static unsigned char expand(unsigned v) {
        v=(v|(v<<2))&0x33;
        v=(v|(v<<1))&0x55;
        return static_cast<unsigned char>(v|(v<<1));
    }

    static void unpack_row(std::span<const unsigned char> source,int y,std::span<unsigned char> output,Run run) {
        constexpr int stride=PilotWidth/8;
        const int begin=y*stride,end=begin+stride;
        if (source.size()==1) { std::fill(output.begin(),output.end(),source.front()); return; }
        if (source.size()==PilotTileBytes) { std::copy_n(source.begin()+begin,stride,output.begin()); return; }
        int produced=run.produced;
        for (int i=run.offset;i+1<int(source.size()) && produced<end;i+=2) {
            const int finish=produced+source[i];
            const int left=std::max(produced,begin),right=std::min(finish,end);
            if (left<right) std::fill(output.begin()+left-begin,output.begin()+right-begin,source[i+1]);
            produced=finish;
        }
    }

    void store(Tile& tile,std::span<const unsigned char> packed) {
        tile.reduced=packed.size()>tile.pixels.size();
        if (!tile.reduced) {
            std::copy(packed.begin(),packed.end(),tile.pixels.begin());
            tile.size=packed.size();
            tile.index={};
            if (packed.size()==1) return;
            unsigned offset=0, produced=0;
            for (unsigned group=0;group<tile.index.size();++group) {
                const auto first=group*16*(PilotWidth/8);
                while (offset+1<packed.size() && produced+packed[offset]<=first) {
                    produced+=packed[offset];
                    offset+=2;
                }
                tile.index[group]={static_cast<std::uint16_t>(offset),static_cast<std::uint16_t>(produced)};
            }
            return;
        }
        tile.size=tile.pixels.size();
        for (int y=0;y<PilotHeight;y+=2) {
            std::array<unsigned char,PilotWidth/8> row{};
            unpack_row(packed,y,row,{});
            for (int i=0;i<PilotWidth/16;++i) {
                const unsigned pair=unsigned(row[2*i])<<8|row[2*i+1];
                unsigned down=0;
                for (int x=0;x<8;++x) down|=((pair>>(15-x*2))&1)<<(7-x);
                tile.pixels[y/2*(PilotWidth/16)+i]=down;
            }
        }
    }

    pxa::Task<unsigned> read_packed(pxa::AssetService assets,int patch) {
        constexpr Text path="assets/sea/pilot-world.bin";
        std::span<unsigned char> bytes=scratch;
        auto header=co_await assets.read(path,patch*4,std::as_writable_bytes(bytes.first(8)));
        if (!header || header->bytes!=8) co_return 0U;
        const auto read=[&](int pos) {
            return unsigned(bytes[pos])|unsigned(bytes[pos+1])<<8|
                   unsigned(bytes[pos+2])<<16|unsigned(bytes[pos+3])<<24;
        };
        const auto from=read(0),to=read(4);
        if (from<(PilotTileCount+1)*4 || to<=from || to>header->total_bytes || to-from>PilotTileBytes) co_return 0U;
        bytes=bytes.first(to-from);
        for (std::size_t offset=0;offset<bytes.size();) {
            const auto size=std::min<std::size_t>(bytes.size()-offset,4064);
            auto block=co_await assets.read(path,from+offset,std::as_writable_bytes(bytes.subspan(offset,size)));
            if (!block || !block->bytes || block->bytes>size) co_return 0U;
            offset+=block->bytes;
        }
        co_return to-from;
    }

    pxa::Task<void> read_tile(pxa::AssetService assets,Tile& tile,bool physics,bool& dirty) {
        const auto length=co_await read_packed(assets,tile.patch);
        if (length && *length) {
            const auto packed=std::span(scratch).first(*length);
            if (!tile.size) store(tile,packed);
            if (physics) {
                collision_patch=decode_pilot_tile(packed,collision)?tile.patch:-1;
                if (collision_patch<0) failed_patch=tile.patch;
            }
        } else failed_patch=tile.patch;
        busy=false;
        dirty=true;
        co_return pxa::Result<void>{};
    }

    std::array<Tile,9> tiles{};
    std::array<unsigned char,PilotTileBytes> scratch{},collision{};
    int collision_patch=-1,failed_patch=-1;
    bool busy=false;
};

} // namespace tide
