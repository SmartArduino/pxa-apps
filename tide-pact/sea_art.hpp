#pragma once

#include "draw.hpp"
#include <pxa/app.hpp>
#include <cstdio>

namespace tide {

inline constexpr int VesselFrames=8;
inline constexpr unsigned VesselFrameMs=135;
inline constexpr int SeaPictureWidth=576, SeaPictureHeight=272;
inline constexpr int FisheryPicture=101, MachicoPicture=102, VilaFrancaPicture=103;
inline constexpr int HelmIconSize=22, PilotPictureBase=120;
inline constexpr int HarborPictureBase=160, WorldFieldPictureBase=200, WorldGroundPictureBase=240;
inline constexpr int PortStoryPictureBase=260;
inline constexpr int MadeiraStreetPicture=270, CanariesStreetPicture=271, CadizStreetPicture=272;
inline constexpr int VeniceStreetPicture=HarborPictureBase+8, KilwaStreetPicture=HarborPictureBase+9;
inline constexpr int LisbonStreetPicture=HarborPictureBase+10;
inline constexpr int LisbonGuildPicture=HarborPictureBase+11, CalicutGuildPicture=HarborPictureBase+12;
inline constexpr int NewHarborPictureBase=HarborPictureBase+13;
enum ExpeditionPicture {
    EstuaryPicture=280, ForestPicture, WadiPicture, VardoPicture, MalabarPicture,
    SabakiPicture, VardoThawPicture, MalabarDryPicture, ArguinPicture
};

inline int new_harbor_picture(int port) {
    const int index=port_index(port);
    return index>=BasePortCount?NewHarborPictureBase+index-BasePortCount:Calm;
}

inline int harbor_picture(int port) {
    if (const int picture=new_harbor_picture(port); picture!=Calm) return picture;
    switch (port) {
    case lisbon: return HarborPictureBase+4;
    case venice: return HarborPictureBase+5;
    case kilwa: return HarborPictureBase+6;
    case calicut: return HarborPictureBase+7;
    default: return HarborPictureBase+Zone[port_index(port)];
    }
}

inline int harbor_street_picture(int port) {
    switch (port) {
    case lisbon: return LisbonStreetPicture;
    case madeira: return MadeiraStreetPicture;
    case canaries: return CanariesStreetPicture;
    case cadiz: return CadizStreetPicture;
    case venice: return VeniceStreetPicture;
    case kilwa: return KilwaStreetPicture;
    default: return new_harbor_picture(port);
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
    constexpr Text expeditions[]{"assets/expeditions/estuary.pxi","assets/expeditions/forest.pxi",
        "assets/expeditions/wadi.pxi","assets/expeditions/vardo.pxi","assets/expeditions/malabar.pxi",
        "assets/expeditions/sabaki.pxi","assets/expeditions/vardo-thaw.pxi",
        "assets/expeditions/malabar-dry.pxi","assets/expeditions/arguin.pxi"};
    static_assert(std::size(expeditions)==ArguinPicture-EstuaryPicture+1);
    if (kind>=EstuaryPicture && kind<=ArguinPicture) return expeditions[kind-EstuaryPicture];
    constexpr Text grounds[]{"assets/world/ground-boreal.pxi","assets/world/ground-mangrove.pxi",
        "assets/world/ground-ice.pxi","assets/world/ground-tropical.pxi","assets/world/ground-reef.pxi",
        "assets/world/ground-sandstone.pxi","assets/world/ground-lava.pxi",
        "assets/world/ground-beypore.pxi","assets/world/ground-houzhu.pxi",
        "assets/world/ground-tsushima.pxi","assets/world/ground-yeompo.pxi"};
    if (kind>=WorldGroundPictureBase && kind<WorldGroundPictureBase+int(std::size(grounds)))
        return grounds[kind-WorldGroundPictureBase];
    if (kind>=WorldFieldPictureBase && kind<WorldFieldPictureBase+3*FieldStudyCount) {
        const int plate=kind-WorldFieldPictureBase;
        return FieldStudies[plate/3].pictures[plate%3];
    }
    constexpr Text ports[]{"assets/ports/atlantic.pxi","assets/ports/mediterranean.pxi",
        "assets/ports/islands.pxi","assets/ports/indian.pxi","assets/ports/lisbon.pxi",
        "assets/ports/venice.pxi","assets/ports/kilwa.pxi","assets/ports/calicut.pxi",
        "assets/ports/venice-streets.pxi","assets/ports/kilwa-streets.pxi","assets/ports/lisbon-streets.pxi",
        "assets/ports/lisbon-guild.pxi","assets/ports/calicut-guild.pxi",
        "assets/ports/ragusa-streets.pxi","assets/ports/candia-streets.pxi",
        "assets/ports/diu-streets.pxi","assets/ports/gresik-streets.pxi",
        "assets/ports/quanzhou-streets.pxi","assets/ports/hakata-streets.pxi",
        "assets/ports/kochi-streets.pxi","assets/ports/busanpo-streets.pxi",
        "assets/ports/malacca-streets.pxi","assets/ports/naha-streets.pxi",
        "assets/ports/ningbo-streets.pxi","assets/ports/sakai-streets.pxi"};
    static_assert(std::size(ports)==NewHarborPictureBase-HarborPictureBase+PortCount-BasePortCount);
    constexpr Text stories[]{"assets/ports/malacca-story.pxi","assets/ports/naha-story.pxi",
                             "assets/ports/ningbo-story.pxi","assets/ports/sakai-story.pxi"};
    static_assert(std::size(stories)==PortStoryCount-WetPorcelain);
    if (kind>=PortStoryPictureBase && kind<PortStoryPictureBase+int(std::size(stories)))
        return stories[kind-PortStoryPictureBase];
    if (kind>=HarborPictureBase && kind<HarborPictureBase+int(std::size(ports)))
        return ports[kind-HarborPictureBase];
    switch (kind) {
    case MadeiraStreetPicture: return "assets/ports/madeira-streets.pxi";
    case CanariesStreetPicture: return "assets/ports/canaries-streets.pxi";
    case CadizStreetPicture: return "assets/ports/cadiz-streets.pxi";
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
        ready_=Calm;
        if (path.empty()) return;
        loading_=kind;
        if (!ctx.tasks().start(load(ctx,kind,path,dirty))) loading_=Calm;
    }

    bool failed(int kind) const { return wanted_==kind && failed_; }

    const unsigned char* pixels(int kind) const {
        return kind!=Calm && ready_==kind?bytes_.data():nullptr;
    }

private:
    static std::size_t image_size(std::span<const std::byte,4> header,bool nearshore) {
        if (nearshore) {
            constexpr std::array magic{std::byte{'P'},std::byte{'C'},std::byte{'H'},std::byte{'1'}};
            return std::equal(header.begin(),header.end(),magic.begin())?PilotMapBytes:0;
        }
        const auto width=pxa::wire::get16(header.data()),height=pxa::wire::get16(header.data()+2);
        return width && width<=SeaPictureWidth && height && height<=SeaPictureHeight?516+std::size_t(width)*height:0;
    }

    pxa::Task<void> load(pxa::Context& ctx,int kind,Text path,bool& dirty) {
        auto assets=ctx.assets();
        std::array<std::byte,4> header{};
        const auto first=co_await assets.read(path,0,header);
        auto error=first?pxa::Error::protocol_error:first.error();
        const auto size=first && first->bytes==header.size()?image_size(header,pilot_picture(kind)):0;
        std::span<std::byte> output;
        if (size && size==first->total_bytes && size<=bytes_.size() && wanted_==kind)
            output=std::as_writable_bytes(std::span(bytes_)).first(size);
        std::size_t offset=0;
        if (!output.empty()) {
            std::copy(header.begin(),header.end(),output.begin());
            offset=header.size();
        }
        while (offset<output.size() && wanted_==kind) {
            const auto length=std::min<std::size_t>(output.size()-offset,4064);
            auto read=co_await assets.read(path,offset,output.subspan(offset,length));
            if (!read) {
                error=read.error();
                break;
            }
            if (read->total_bytes!=output.size() || !read->bytes || read->bytes>length) break;
            offset+=read->bytes;
        }
        const bool complete=offset && offset==output.size() && wanted_==kind;
        if (complete) ready_=kind;
        else if (wanted_==kind) {
            failed_=true;
            char message[180];
            std::snprintf(message,sizeof(message),"Tide Pact picture unavailable: %.*s (%d)",
                          int(path.size()),path.data(),int(error));
            (void)ctx.log().write(pxa::LogLevel::warning,message);
        }
        loading_=Calm;
        dirty=true;
        co_return pxa::Result<void>{};
    }

    // Stream every picture into one fixed buffer. Switching from a plate to the
    // field map never needs a larger contiguous guest-heap allocation.
    static_assert(PilotMapBytes<=516+SeaPictureWidth*SeaPictureHeight);
    std::array<unsigned char,516+SeaPictureWidth*SeaPictureHeight> bytes_{};
    int wanted_=Calm,ready_=Calm,loading_=Calm;
    bool failed_=false;
};

} // namespace tide
