#pragma once

#include "draw.hpp"
#include <pxa/app.hpp>

namespace tide {

inline constexpr int VesselFrames=8;
inline constexpr unsigned VesselFrameMs=135;
inline constexpr int SeaPictureWidth=160, SeaPictureHeight=104;
inline constexpr int FisheryPicture=101, MachicoPicture=102, VilaFrancaPicture=103;
inline constexpr int HelmIconSize=22, PilotPictureBase=120;
inline constexpr int HarborPictureBase=160;
inline constexpr int VeniceStreetPicture=HarborPictureBase+8, KilwaStreetPicture=HarborPictureBase+9;

inline int harbor_picture(int port) {
    switch (port) {
    case lisbon: return HarborPictureBase+4;
    case venice: return HarborPictureBase+5;
    case kilwa: return HarborPictureBase+6;
    case calicut: return HarborPictureBase+7;
    default: return HarborPictureBase+Zone[port];
    }
}

inline int harbor_street_picture(int port) {
    return port==venice?VeniceStreetPicture:port==kilwa?KilwaStreetPicture:Calm;
}
inline constexpr int PilotMarkSize=22, SmallShipSize=16, SmallShipStride=8*SmallShipSize;

inline bool pilot_picture(int kind) {
    return kind>=PilotPictureBase && kind<PilotPictureBase+int(std::size(PilotRegions));
}

inline Text sea_picture_path(int kind) {
    if (pilot_picture(kind)) return PilotRegions[kind-PilotPictureBase].asset;
    constexpr Text ports[]{"assets/ports/atlantic.pxi","assets/ports/mediterranean.pxi",
        "assets/ports/islands.pxi","assets/ports/indian.pxi","assets/ports/lisbon.pxi",
        "assets/ports/venice.pxi","assets/ports/kilwa.pxi","assets/ports/calicut.pxi",
        "assets/ports/venice-streets.pxi","assets/ports/kilwa-streets.pxi"};
    if (kind>=HarborPictureBase && kind<HarborPictureBase+int(std::size(ports)))
        return ports[kind-HarborPictureBase];
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
    pxa::Task<void> load(pxa::AssetService assets, int kind, bool& dirty) {
        ready_=Calm;
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
