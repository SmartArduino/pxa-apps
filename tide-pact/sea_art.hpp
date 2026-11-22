#pragma once

#include "draw.hpp"
#include <pxa/app.hpp>

namespace tide {

inline constexpr int VesselFrames=8;
inline constexpr unsigned VesselFrameMs=135;
inline constexpr int SeaPictureWidth=160, SeaPictureHeight=104;
inline constexpr int FisheryPicture=101, MachicoPicture=102, VilaFrancaPicture=103;
inline constexpr int HelmIconSize=22, PilotPictureBase=120;
inline constexpr int PilotWorldPictureBase=256;
inline constexpr int PilotMarkSize=22, SmallShipSize=16, SmallShipStride=8*SmallShipSize;

inline bool pilot_picture(int kind) {
    return kind>=PilotPictureBase && kind<PilotPictureBase+int(std::size(PilotRegions));
}

inline bool pilot_world_picture(int kind) {
    return kind>=PilotWorldPictureBase && kind<PilotWorldPictureBase+PilotTileCount;
}

inline Text sea_picture_path(int kind) {
    if (pilot_picture(kind)) return PilotRegions[kind-PilotPictureBase].asset;
    if (pilot_world_picture(kind)) return "assets/sea/pilot-world.bin";
    switch (kind) {
    case Castaways: return "assets/sea/castaways.pxi";
    case FalseBeacon: return "assets/sea/false-beacon.pxi";
    case Rigging: return "assets/sea/rigging.pxi";
    case ReliefShip: return "assets/sea/water-boat.pxi";
    case Fog: return "assets/sea/fog-bank.pxi";
    case ReversedFlag: return "assets/sea/reversed-flag.pxi";
    case StrandedPost: return "assets/sea/grounded.pxi";
    case ShoalLantern: return "assets/sea/shoal-lantern.pxi";
    case Doldrums: return "assets/sea/doldrums.pxi";
    case AdenMerchant: return "assets/sea/aden-ship.pxi";
    case MachicoPicture: return "assets/sea/machico.pxi";
    case VilaFrancaPicture: return "assets/sea/vila-franca.pxi";
    case FisheryPicture: return "assets/sea/fishery.pxi";
    default: return {};
    }
}

class SeaPicture {
public:
    void show(pxa::Context& ctx, int kind, bool& dirty) {
        if (wanted_!=kind) {
            wanted_=kind;
            failed_=false;
        }
        if (ready_==kind || busy_ || failed_ || sea_picture_path(kind).empty()) return;
        busy_=true;
        if (!ctx.tasks().start(load(ctx.assets(),kind,dirty))) busy_=false;
    }

    bool failed(int kind) const { return wanted_==kind && failed_; }

    const unsigned char* pixels(int kind) const {
        return kind!=Calm && ready_==kind?bytes_.data():nullptr;
    }

private:
    pxa::Task<bool> load_world(pxa::AssetService assets,int patch) {
        auto output=std::span(bytes_);
        const auto path=sea_picture_path(PilotWorldPictureBase+patch);
        auto header=co_await assets.read(path,patch*4,std::as_writable_bytes(output.first(8)));
        if (!header || header->bytes!=8) co_return false;
        const auto read=[&](int pos) {
            return unsigned(bytes_[pos])|unsigned(bytes_[pos+1])<<8|
                   unsigned(bytes_[pos+2])<<16|unsigned(bytes_[pos+3])<<24;
        };
        const auto start=read(0), end=read(4);
        if (start<(PilotTileCount+1)*4 || end<=start || end>header->total_bytes ||
            end-start>PilotTileBytes) co_return false;
        auto packed=output.subspan(PilotTileBytes,end-start);
        std::size_t offset=0;
        while (offset<packed.size()) {
            const auto count=std::min<std::size_t>(packed.size()-offset,4064);
            auto block=co_await assets.read(path,start+offset,std::as_writable_bytes(packed.subspan(offset,count)));
            if (!block || !block->bytes) co_return false;
            offset+=block->bytes;
        }
        co_return decode_pilot_tile(packed,output.first(PilotTileBytes));
    }

    pxa::Task<void> load(pxa::AssetService assets, int kind, bool& dirty) {
        ready_=Calm;
        if (pilot_world_picture(kind)) {
            const auto loaded=co_await load_world(assets,kind-PilotWorldPictureBase);
            if (loaded && *loaded) ready_=kind;
            else if (wanted_==kind) failed_=true;
            busy_=false;
            dirty=true;
            co_return pxa::Result<void>{};
        }
        const bool nearshore=pilot_picture(kind);
        const auto count=nearshore?PilotMapBytes:int(bytes_.size());
        auto output=std::as_writable_bytes(std::span(bytes_)).first(count);
        std::size_t offset=0;
        while (offset<output.size()) {
            const auto length=std::min<std::size_t>(output.size()-offset,4064);
            auto read=co_await assets.read(sea_picture_path(kind),offset,output.subspan(offset,length));
            if (!read || read->total_bytes!=output.size() || !read->bytes) break;
            offset+=read->bytes;
        }
        const Image image{bytes_.data()};
        const auto valid=nearshore?
            bytes_[0]=='P' && bytes_[1]=='C' && bytes_[2]=='H' && bytes_[3]=='1':
            !image.packed() && image.width()==SeaPictureWidth && image.height()==SeaPictureHeight;
        const bool complete=offset==output.size() && valid;
        if (complete) ready_=kind;
        else if (wanted_==kind) failed_=true;
        busy_=false;
        dirty=true;
        co_return pxa::Result<void>{};
    }

    // Only the active illustration or close-up sprite sheet occupies linear memory.
    std::array<unsigned char,516+SeaPictureWidth*SeaPictureHeight> bytes_{};
    int wanted_=Calm, ready_=Calm;
    bool busy_=false, failed_=false;
};

} // namespace tide
