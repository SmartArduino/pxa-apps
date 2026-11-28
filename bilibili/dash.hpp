// SPDX-License-Identifier: MIT
#pragma once
#include "mp4.hpp"
#include "../common/media/json.hpp"
#include <charconv>
#include <string>
#include <string_view>

namespace bili {
constexpr size_t dash_max_range = 768 * 1024;
struct Segment {
    uint64_t offset, pts, end;
    uint32_t bytes;
    bool random_access;
};
struct FragmentDefaults {
    uint32_t track = 0, duration = 0, size = 0, flags = 0;
};
struct DashStream {
    std::string url;
    std::string backup_url;
    size_t init_bytes = 0;
    std::vector<Segment> segments;
    FragmentDefaults defaults;
    uint64_t presentation_skip = 0;
    size_t next = 0;
};
struct DashRange {
    uint64_t offset;
    size_t bytes, next;
};

inline DashRange dash_range(const DashStream& stream,size_t limit) {
    const auto& first=stream.segments[stream.next];
    DashRange range{first.offset,first.bytes,stream.next+1};
    while (range.next<stream.segments.size() &&
           range.bytes+stream.segments[range.next].bytes<=limit)
        range.bytes+=stream.segments[range.next++].bytes;
    return range;
}

inline bool https_url(media::json::Value value, std::string& url) {
    char text[2049];
    bool truncated = false;
    if (!value.string(text, sizeof(text), &truncated) || truncated || std::strncmp(text, "https://", 8))
        return false;
    url = text;
    return true;
}

inline bool dash_cdn(std::string_view url) {
    auto host = url.substr(8);
    host = host.substr(0, host.find('/'));
    return host.ends_with(".bilivideo.com");
}

inline bool dash_stream(media::json::Value entry, DashStream& stream) {
    std::string url;
    if (!https_url(entry.get("baseUrl"), url) && !https_url(entry.get("base_url"), url))
        return false;
    // The API also supplies standard HTTPS mirrors for its edge nodes.
    std::string backup_url;
    media::json::Array backups(entry.get(entry.get("backupUrl") ? "backupUrl" : "backup_url"));
    media::json::Value backup;
    while (backups.next(backup)) {
        std::string mirror;
        if (!https_url(backup, mirror) || !dash_cdn(mirror) || mirror == url) continue;
        if (!dash_cdn(url)) {
            backup_url = std::move(url);
            url = std::move(mirror);
        } else {
            backup_url = std::move(mirror);
            break;
        }
    }
    auto base = entry.get("SegmentBase");
    if (!base) base = entry.get("segment_base");
    char index[48];
    const auto range = base.get(base.get("indexRange") ? "indexRange" : "index_range");
    bool truncated = false;
    if (!range.string(index, sizeof(index), &truncated) || truncated) return false;
    const char* last = std::strchr(index, '-');
    if (!last || !last[1]) return false;
    ++last;
    size_t end = 0;
    const char* finish = index + std::strlen(index);
    auto result = std::from_chars(last, finish, end);
    if (result.ec != std::errc{} || result.ptr != finish || end >= 64 * 1024) return false;
    stream.url = std::move(url);
    stream.backup_url = std::move(backup_url);
    stream.init_bytes = end + 1;
    return true;
}

inline bool dash_tracks(media::json::Value dash, unsigned quality, DashStream& video, DashStream& audio) {
    media::json::Value entry;
    media::json::Array videos(dash.get("video"));
    while (videos.next(entry)) {
        if (entry.get("id").number() == quality && entry.get("codecid").number() == 7 && dash_stream(entry, video))
            break;
    }
    if (video.url.empty()) return false;
    media::json::Array audios(dash.get("audio"));
    uint64_t bandwidth = UINT64_MAX;
    while (audios.next(entry)) {
        char codec[32]{};
        entry.get("codecs").string(codec, sizeof(codec));
        auto bitrate = entry.get("bandwidth").number(UINT64_MAX);
        if (!std::strcmp(codec, "mp4a.40.2") && bitrate < bandwidth && dash_stream(entry, audio))
            bandwidth = bitrate;
    }
    return true;
}

inline uint64_t media_us(uint64_t ticks, uint32_t scale) {
    return ticks / scale * 1000000 + ticks % scale * 1000000 / scale;
}

inline bool parse_segment_index(Bytes body, uint64_t pos, DashStream& stream) {
    if (body.size() < 24 || body[0] > 1 || be32(body.data() + 4) != stream.defaults.track)
        return false;
    const auto scale = be32(body.data() + 8);
    const bool large = body[0] == 1;
    const size_t header = large ? 32 : 24;
    if (!scale || body.size() < header) return false;
    uint64_t ticks = large ? be64(body.data() + 12) : be32(body.data() + 12);
    auto start = large ? be64(body.data() + 20) : be32(body.data() + 16);
    if (start > UINT64_MAX - pos) return false;
    pos += start;
    const size_t count = unsigned(body[header - 2]) * 256 + body[header - 1];
    if (!count || count > 4096 || count > (body.size() - header) / 12) return false;
    for (size_t i = 0; i < count; ++i) {
        const auto* item = body.data() + header + i * 12;
        uint32_t size = be32(item), length = be32(item + 4);
        if (!size || (size & 0x80000000u) || !length || pos > UINT64_MAX - size ||
            ticks > uint64_t(scale) * 86400 || length > uint64_t(scale) * 86400 - ticks)
            return false;
        const auto sap = be32(item + 8);
        stream.segments.push_back({pos, media_us(ticks, scale), media_us(ticks + length, scale), size,
                                   (sap & 0xf0000000u) == 0x90000000u});
        pos += size;
        ticks += length;
    }
    return true;
}

inline bool parse_dash_init(Bytes bytes, DashStream& stream, Track& track, bool video) {
    auto moov = child(bytes, "moov");
    auto trak = child(moov, "trak");
    auto trex = child(child(moov, "mvex"), "trex");
    Bytes stbl;
    if (!parse_track_description(trak, track, stbl) ||
        (video ? !track.video : !track.audio) || trex.size() < 24 || be32(trex.data() + 8) != 1)
        return false;
    stream.defaults = {be32(trex.data() + 4), be32(trex.data() + 12),
                       be32(trex.data() + 16), be32(trex.data() + 20)};
    auto edit = child(child(trak, "edts"), "elst");
    if (edit.size() >= 20 && !edit[0] && be32(edit.data() + 4) == 1) {
        const auto skip = int32_t(be32(edit.data() + 12));
        if (skip > 0) stream.presentation_skip = media_us(skip, track.timescale);
    }
    stream.segments.clear();
    return boxes(bytes, [&](Box box) {
        return box.type != tag("sidx") ||
            (stream.segments.empty() && parse_segment_index(box.data, box.data.data() -
                bytes.data() + box.data.size(), stream));
    }) && !stream.segments.empty() && (video || std::none_of(stream.segments.begin(),stream.segments.end(),
        [](const Segment& segment) { return segment.bytes > dash_max_range; }));
}

inline bool fragment_defaults(Bytes tfhd, FragmentDefaults& defaults) {
    if (tfhd.size() < 8 || be32(tfhd.data() + 4) != defaults.track) return false;
    const auto flags = be32(tfhd.data());
    // The supported layout addresses a single track relative to its moof.
    if ((flags & ~0x2003au) || !(flags & 0x20000)) return false;
    size_t at = 8;
    auto field = [&](uint32_t bit, uint32_t& value) {
        if (!(flags & bit)) return true;
        if (tfhd.size() - at < 4) return false;
        value = be32(tfhd.data() + at);
        at += 4;
        return true;
    };
    uint32_t description = 1;
    return field(2, description) && description == 1 && field(8, defaults.duration) &&
        field(16, defaults.size) && field(32, defaults.flags);
}

inline bool parse_run(Bytes moof, uint64_t moof_offset, uint64_t data, uint64_t end, uint64_t start,
                      const DashStream& stream, Track& track) {
    auto traf = child(moof, "traf");
    auto tfhd = child(traf, "tfhd"), tfdt = child(traf, "tfdt"), trun = child(traf, "trun");
    auto defaults = stream.defaults;
    if (!fragment_defaults(tfhd, defaults) || tfdt.size() < 8 || tfdt[0] > 1 ||
        (tfdt[0] == 1 && tfdt.size() < 12) || trun.size() < 12 || trun[0] > 1)
        return false;
    uint64_t dts = tfdt[0] ? be64(tfdt.data() + 4) : be32(tfdt.data() + 4);
    const auto flags = be32(trun.data()) & 0x00ffffff;
    const auto count = be32(trun.data() + 4);
    const auto relative = int32_t(be32(trun.data() + 8));
    if (!(flags & 1) || (flags & ~0x0f05u) || relative < 0 || !count || !track.timescale ||
        count > 4096 || track.samples.size() + count > 8192)
        return false;
    uint64_t offset = moof_offset + unsigned(relative);
    if (offset < data || offset >= end) return false;
    size_t at = 12;
    auto field = [&](uint32_t bit, uint32_t& value) {
        if (!(flags & bit)) return true;
        if (trun.size() - at < 4) return false;
        value = be32(trun.data() + at);
        at += 4;
        return true;
    };
    auto first_flags = defaults.flags;
    if (!field(4, first_flags)) return false;
    const auto max_ticks = uint64_t(track.timescale) * 86400;
    for (uint32_t i = 0; i < count; ++i) {
        auto length = defaults.duration, size = defaults.size;
        auto sample_flags = i ? defaults.flags : first_flags;
        uint32_t composition = 0;
        if (!field(0x100, length) || !field(0x200, size) || !field(0x400, sample_flags) ||
            !field(0x800, composition) || !length || !size || size > 512 * 1024 ||
            size > end - offset || dts > max_ticks || length > max_ticks - dts)
            return false;
        auto delta = trun[0] ? int64_t(int32_t(composition)) : int64_t(composition);
        auto pts = int64_t(dts) + delta;
        uint64_t time = pts < 0 ? 0 : media_us(uint64_t(pts), track.timescale);
        time -= std::min(time, stream.presentation_skip);
        if (track.video && !(sample_flags & 0x10000))
            track.sync_samples.push_back(uint32_t(track.samples.size()));
        track.samples.push_back({start + offset, time, size, uint32_t(media_us(length, track.timescale))});
        offset += size;
        dts += length;
    }
    return true;
}

// A fully received moof can index samples as soon as the mdat header arrives.
// The declared mdat extent must fit the requested range, and the caller still
// checks that each sample's bytes have arrived before feeding it to the codec.
struct DashRangeParser {
    size_t next_box = 0;
    size_t moof_offset = 0, moof_header = 0, moof_size = 0;

    size_t required_offset() const { return moof_size ? moof_offset : next_box; }

    bool append(Bytes bytes, size_t total, uint64_t start, const DashStream& stream, Track& track,
                size_t window_offset = 0) {
        if (window_offset > required_offset() || window_offset > total ||
            bytes.size() > total - window_offset || total > UINT64_MAX - start) return false;
        const size_t received = window_offset + bytes.size();
        const bool complete = received == total;
        while (next_box < received) {
            const auto tail = bytes.subspan(next_box - window_offset);
            if (tail.size() < 8) return !complete;
            auto size = uint64_t(be32(tail.data()));
            size_t header = 8;
            if (size == 1) {
                if (tail.size() < 16) return !complete;
                size = be64(tail.data() + 8);
                header = 16;
            }
            if (!size) size = total - next_box;
            if (size < header || size > total - next_box) return false;
            const auto type = be32(tail.data() + 4);
            if (type == tag("mdat")) {
                if (!moof_size ||
                    !parse_run(bytes.subspan(moof_offset - window_offset + moof_header, moof_size - moof_header),
                        moof_offset, next_box + header, next_box + size, start, stream, track))
                    return false;
                moof_size = 0;
            } else if (type == tag("moof")) {
                if (moof_size || size == header) return false;
                if (size > tail.size()) return !complete;
                moof_offset = next_box;
                moof_header = header;
                moof_size = size_t(size);
            }
            next_box += size_t(size);
        }
        return !complete || (next_box == total && !moof_size && !track.samples.empty());
    }
};

// Keep a partial moof and a frame crossing the HTTP boundary readable.
struct VideoPage { uint64_t offset; size_t bytes; };

// A parsed moof may describe samples beyond the bytes already downloaded.
inline bool buffered_through(const Track& track,size_t next,uint64_t start,size_t used,uint64_t until) {
    for (size_t i=next;i<track.samples.size();++i) {
        const auto& sample=track.samples[i];
        if (sample.offset<start || sample.offset-start>used || sample.size>used-(sample.offset-start))
            return false;
        if (sample.pts>=until) return true;
    }
    return false;
}
inline VideoPage next_video_page(const DashRange& segment,const DashRangeParser& parser,
                                const Sample *sample,size_t target,uint64_t previous,size_t received) {
    size_t at=parser.required_offset();
    if (sample) at=std::min(at,size_t(sample->offset-segment.offset));
    if (at>=segment.bytes) return {segment.offset+segment.bytes,0};
    const auto offset=segment.offset+at;
    auto bytes=std::min(segment.bytes-at,std::max<size_t>(target,sample ? sample->size : 0));
    if (offset==previous && bytes<=received)
        bytes=std::min(segment.bytes-at,dash_max_range);
    return {offset,bytes};
}

inline VideoPage prefetch_video_page(const DashRange& segment,const DashRangeParser& parser,
                                     const Track& track,size_t first,size_t target,
                                     uint64_t start,size_t used) {
    if (!segment.bytes || start+used>=segment.offset+segment.bytes) return {};
    const Sample *next=nullptr;
    for (size_t i=first;i<track.samples.size();++i) {
        const auto& sample=track.samples[i];
        if (sample.offset<start || sample.offset-start>used || sample.size>used-(sample.offset-start)) {
            next=&sample;
            break;
        }
    }
    auto page=next_video_page(segment,parser,next,target,start,used);
    return page.offset>start ? page : VideoPage{};
}

inline bool parse_dash_range(Bytes bytes, uint64_t start, const DashStream& stream, Track& track) {
    track.samples.clear();
    track.sync_samples.clear();
    return DashRangeParser{}.append(bytes, bytes.size(), start, stream, track);
}

inline size_t dash_segment_at(const DashStream& stream, uint64_t time) {
    auto it = std::upper_bound(stream.segments.begin(), stream.segments.end(), time,
        [](uint64_t value, const Segment& segment) { return value < segment.pts; });
    return it == stream.segments.begin() ? 0 : size_t(it - stream.segments.begin() - 1);
}

inline bool dash_contains_segment(const Segment& segment, uint64_t start, size_t bytes) {
    return segment.offset >= start && segment.offset - start <= bytes &&
           segment.bytes <= bytes - (segment.offset - start);
}

// Reuse only a complete segment. A consumed prefix may already have been
// released, and video samples are stored in decode order, not PTS order.
inline size_t dash_cached_seek_sample(const Track& track, const Segment& segment, uint64_t pts,
                                      uint64_t cache_start, size_t cache_bytes) {
    const auto& samples = track.samples;
    if (!dash_contains_segment(segment,cache_start,cache_bytes))
        return samples.size();
    auto sample = track.video
        ? std::lower_bound(samples.begin(), samples.end(), segment.offset,
              [](const Sample& s, uint64_t offset) { return s.offset < offset; })
        : std::lower_bound(samples.begin(), samples.end(), pts,
              [](const Sample& s, uint64_t time) { return s.pts < time; });
    if (sample == samples.end() || sample->offset < segment.offset ||
        sample->offset - segment.offset >= segment.bytes ||
        sample->size > segment.bytes - (sample->offset - segment.offset))
        return samples.size();
    const size_t index = size_t(sample - samples.begin());
    if (track.video && !std::binary_search(track.sync_samples.begin(), track.sync_samples.end(), index))
        return samples.size();
    return index;
}
}
