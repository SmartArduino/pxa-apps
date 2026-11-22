#pragma once

#include "draw.hpp"
#include <pxa/app.hpp>

namespace tide {

inline constexpr int VesselFrameWidth=40, VesselFrameHeight=30, VesselFrames=8;
inline constexpr unsigned VesselFrameMs=135;
inline constexpr std::array<int,VesselFrames> VesselHeave{0,-1,-1,0,0,1,1,0};
inline constexpr std::array<Point,3> VesselMast{{{19,10},{24,9},{17,2}}};
inline constexpr int SeaPictureWidth=160, SeaPictureHeight=104;
inline constexpr int CaravelFrameWidth=SeaPictureWidth/2, CaravelFrameHeight=SeaPictureHeight/2;
inline constexpr int CaravelDetail=100;

inline constexpr Text sea_picture_path(int kind) {
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
    case CaravelDetail: return "assets/sea/caravel-detail.pxi";
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

    const unsigned char* pixels(int kind) const {
        return kind!=Calm && ready_==kind?bytes_.data():nullptr;
    }

private:
    pxa::Task<void> load(pxa::AssetService assets, int kind, bool& dirty) {
        ready_=Calm;
        auto output=std::as_writable_bytes(std::span(bytes_));
        std::size_t offset=0;
        while (offset<output.size()) {
            const auto length=std::min<std::size_t>(output.size()-offset,4064);
            auto read=co_await assets.read(sea_picture_path(kind),offset,output.subspan(offset,length));
            if (!read || read->total_bytes!=output.size() || !read->bytes) break;
            offset+=read->bytes;
        }
        const Image image{bytes_.data()};
        const bool complete=offset==output.size() && !image.packed() &&
                            image.width()==SeaPictureWidth && image.height()==SeaPictureHeight;
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
