#pragma once

#include "draw.hpp"
#include <pxa/app.hpp>
#include <memory>

namespace tide {

inline constexpr int VesselFrames=8;
inline constexpr unsigned VesselFrameMs=135;
inline constexpr int SeaPictureWidth=160, SeaPictureHeight=104;
inline constexpr int FisheryPicture=101, MachicoPicture=102, VilaFrancaPicture=103;
inline constexpr int HelmIconSize=22, PilotPictureBase=120;
inline constexpr int HarborPictureBase=160;
inline constexpr int VeniceStreetPicture=HarborPictureBase+8, KilwaStreetPicture=HarborPictureBase+9;
inline constexpr int LisbonStreetPicture=HarborPictureBase+10;
inline constexpr int LisbonGuildPicture=HarborPictureBase+11, CalicutGuildPicture=HarborPictureBase+12;

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
    switch (port) {
    case lisbon: return LisbonStreetPicture;
    case venice: return VeniceStreetPicture;
    case kilwa: return KilwaStreetPicture;
    default: return Calm;
    }
}
inline int guild_picture(int port) {
    return port==lisbon?LisbonGuildPicture:port==calicut?CalicutGuildPicture:Calm;
}
inline constexpr int PilotMarkSize=22, SmallShipSize=20, SmallShipStride=8*SmallShipSize;

inline bool pilot_picture(int kind) {
    return kind>=PilotPictureBase && kind<PilotPictureBase+int(std::size(PilotRegions));
}

inline Text sea_picture_path(int kind) {
    if (pilot_picture(kind)) return PilotRegions[kind-PilotPictureBase].asset;
    constexpr Text ports[]{"assets/ports/atlantic.pxi","assets/ports/mediterranean.pxi",
        "assets/ports/islands.pxi","assets/ports/indian.pxi","assets/ports/lisbon.pxi",
        "assets/ports/venice.pxi","assets/ports/kilwa.pxi","assets/ports/calicut.pxi",
        "assets/ports/venice-streets.pxi","assets/ports/kilwa-streets.pxi","assets/ports/lisbon-streets.pxi",
        "assets/ports/lisbon-guild.pxi","assets/ports/calicut-guild.pxi"};
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
        if (ready_==kind || loading_!=Calm || failed_) return;
        const Text path=sea_picture_path(kind);
        if (path.empty() && !large_) return;
        large_.reset();
        ready_=Calm;
        if (path.empty()) return;
        loading_=kind;
        if (!ctx.tasks().start(load(ctx.assets(),kind,path,dirty))) loading_=Calm;
    }

    bool failed(int kind) const { return wanted_==kind && failed_; }

    const unsigned char* pixels(int kind) const {
        return kind!=Calm && ready_==kind?(large_?large_.get():bytes_.data()):nullptr;
    }

private:
    static std::size_t image_size(std::span<const std::byte,4> header,bool nearshore) {
        if (nearshore) {
            constexpr std::array magic{std::byte{'P'},std::byte{'C'},std::byte{'H'},std::byte{'1'}};
            return std::equal(header.begin(),header.end(),magic.begin())?PilotMapBytes:0;
        }
        const auto width=pxa::wire::get16(header.data()),height=pxa::wire::get16(header.data()+2);
        return width && width<=576 && height && height<=272?516+std::size_t(width)*height:0;
    }

    pxa::Task<void> load(pxa::AssetService assets,int kind,Text path,bool& dirty) {
        std::array<std::byte,4> header{};
        const auto first=co_await assets.read(path,0,header);
        const auto size=first && first->bytes==header.size()?image_size(header,pilot_picture(kind)):0;
        std::span<std::byte> output;
        if (size && size==first->total_bytes && wanted_==kind) {
            if (size<=bytes_.size()) output=std::as_writable_bytes(std::span(bytes_)).first(size);
            else {
                large_.reset(new (std::nothrow) unsigned char[size]);
                if (large_) output=std::as_writable_bytes(std::span(large_.get(),size));
            }
        }
        std::size_t offset=0;
        if (!output.empty()) {
            std::copy(header.begin(),header.end(),output.begin());
            offset=header.size();
        }
        while (offset<output.size() && wanted_==kind) {
            const auto length=std::min<std::size_t>(output.size()-offset,4064);
            auto read=co_await assets.read(path,offset,output.subspan(offset,length));
            if (!read || read->total_bytes!=output.size() || !read->bytes || read->bytes>length) break;
            offset+=read->bytes;
        }
        const bool complete=offset && offset==output.size() && wanted_==kind;
        if (complete) ready_=kind;
        else {
            large_.reset();
            if (wanted_==kind) failed_=true;
        }
        loading_=Calm;
        dirty=true;
        co_return pxa::Result<void>{};
    }

    // A single streamed picture or pilotage grid is resident at a time.
    std::array<unsigned char,516+SeaPictureWidth*SeaPictureHeight> bytes_{};
    std::unique_ptr<unsigned char[]> large_;
    int wanted_=Calm,ready_=Calm,loading_=Calm;
    bool failed_=false;
};

} // namespace tide
