// SPDX-License-Identifier: MIT
#include "../dash.hpp"
#include "../playback.hpp"
#include <cassert>

using Buffer = std::vector<uint8_t>;

static void u32(Buffer& out, uint32_t n) {
    for (unsigned shift : {24, 16, 8, 0}) out.push_back(uint8_t(n >> shift));
}
static Buffer words(std::initializer_list<uint32_t> values) {
    Buffer result;
    for (uint32_t n : values) u32(result, n);
    return result;
}
static void box(Buffer& out, const char* type, const Buffer& content) {
    u32(out, uint32_t(content.size() + 8));
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), content.begin(), content.end());
}
static Buffer fragment(int32_t relative) {
    Buffer traf, moof, result;
    box(traf, "tfhd", words({0x20038, 1, 640, 3, 0x10000}));
    box(traf, "tfdt", words({0, 16000}));
    box(traf, "trun", words({0x01000805, 2, uint32_t(relative), 0, uint32_t(-320), 0}));
    box(moof, "traf", traf);
    box(result, "moof", moof);
    box(result, "mdat", {1, 2, 3, 4, 5, 6});
    return result;
}
int main() {
    bili::DashStream stream;
    const char manifest[]=R"({"baseUrl":"https://p2p.example.com/media", "backupUrl":[
        "https://edge.bilivideo.com/media", "https://backup.bilivideo.com/media"],
        "SegmentBase":{"indexRange":"10-99"}})";
    assert(bili::dash_stream({reinterpret_cast<const uint8_t*>(manifest),sizeof(manifest)-1},stream));
    assert(stream.url=="https://edge.bilivideo.com/media" && stream.init_bytes==100);
    assert(stream.backup_url=="https://backup.bilivideo.com/media");
    stream.defaults.track = 1;
    auto index = words({0, 1, 16000, 0, 12, 2, 1024, 80000, 0x90000000, 2048, 80000, 0x10000000});
    assert(bili::parse_segment_index(index, 900, stream));
    assert(stream.segments.size() == 2 && stream.segments[0].offset == 912);
    assert(stream.segments[1].offset == 1936 && stream.segments[1].end == 10000000);
    assert(stream.segments[0].random_access && !stream.segments[1].random_access);
    const auto first_window=bili::dash_range(stream,2048);
    const auto combined=bili::dash_range(stream,3072);
    assert(first_window.offset==912 && first_window.bytes==1024 && first_window.next==1);
    assert(combined.offset==912 && combined.bytes==3072 && combined.next==2);
    assert(bili::dash_segment_at(stream, 4800000) == 0);
    assert(bili::dash_segment_at(stream, 5000000) == 1);

    bili::Track video_cache;
    video_cache.video = true;
    video_cache.samples = {{1000, 0, 10, 1}, {2100, 5020000, 10, 1}, {2200, 5000000, 10, 1}};
    video_cache.sync_samples = {0, 1};
    const auto& second = stream.segments[1];
    assert(bili::dash_contains_segment(second,second.offset,second.bytes));
    assert(!bili::dash_contains_segment(second,second.offset+1,second.bytes));
    assert(!bili::dash_contains_segment(second,second.offset,second.bytes-1));
    assert(!bili::dash_contains_segment({UINT64_MAX-1,0,0,8,false},0,4096));
    assert(bili::dash_cached_seek_sample(video_cache, second, 5000000, 912, 3072) == 1);
    assert(bili::dash_cached_seek_sample(video_cache, second, 5000000, 912, 3071) == 3);
    assert(bili::dash_cached_seek_sample(video_cache, second, 5000000, 2000, 2048) == 3);
    video_cache.sync_samples = {0, 2};
    assert(bili::dash_cached_seek_sample(video_cache, second, 5000000, 912, 3072) == 3);
    bili::Track audio_cache;
    audio_cache.samples = {{1000, 0, 10, 1}, {2100, 5000000, 10, 1}, {2200, 5020000, 10, 1}};
    assert(bili::dash_cached_seek_sample(audio_cache, second, 5000100, 1936, 2048) == 2);
    assert(bili::dash_cached_seek_sample(audio_cache, second, 5040000, 1936, 2048) == 3);

    bili::Track track;
    track.timescale = 16000;
    track.video = true;
    const size_t offset = fragment(0).size() - 6;
    auto media = fragment(int32_t(offset));
    assert(bili::parse_dash_range(media, 912, stream, track));
    assert(track.samples.size() == 2 && track.samples[0].offset == 912 + offset);
    assert(track.samples[0].pts == 980000 && track.samples[1].pts == 1040000);
    assert(track.sync_samples.size() == 1 && track.sync_samples[0] == 0);
    auto two_fragments=media;
    two_fragments.insert(two_fragments.end(),media.begin(),media.end());
    bili::DashRangeParser incremental;
    track.samples.clear();
    track.sync_samples.clear();
    auto prefix=bili::Bytes(two_fragments);
    assert(incremental.append(prefix.first(offset-1),prefix.size(),912,stream,track));
    assert(track.samples.empty());
    assert(incremental.append(prefix.first(offset),prefix.size(),912,stream,track));
    assert(track.samples.size()==2 && track.samples.front().offset==912+offset);
    assert(incremental.append(prefix.first(offset+4),prefix.size(),912,stream,track));
    assert(track.samples.size()==2);
    assert(incremental.append(prefix.first(media.size()+offset),prefix.size(),912,stream,track));
    assert(track.samples.size()==4 && track.samples[2].offset==912+media.size()+offset);
    assert(incremental.append(prefix,prefix.size(),912,stream,track));
    assert(track.samples.size()==4 && track.sync_samples[1]==2);
    assert(!bili::parse_dash_range(fragment(int32_t(offset - 1)), 912, stream, track));
    // The next moof starts across a window boundary. Index both mdats once.
    bili::DashRangeParser paged;
    track.samples.clear();
    track.sync_samples.clear();
    const bili::DashRange range{912,prefix.size(),1};
    const size_t cut=media.size()+5;
    assert(paged.append(prefix.first(cut),range.bytes,range.offset,stream,track));
    assert(track.samples.size()==2);
    auto page=bili::next_video_page(range,paged,nullptr,128,range.offset,cut);
    assert(page.offset==range.offset+media.size());
    auto ahead=bili::prefetch_video_page(range,paged,track,0,128,range.offset,cut);
    assert(ahead.offset==page.offset && ahead.bytes==page.bytes);
    assert(paged.append(prefix.subspan(media.size()),range.bytes,range.offset,stream,track,media.size()));
    assert(track.samples.size()==4);
    const auto &last=track.samples.back();
    page=bili::next_video_page(range,paged,&last,2,range.offset,cut);
    assert(page.offset==last.offset && page.bytes==last.size);
    ahead=bili::prefetch_video_page(range,paged,track,track.samples.size()-1,2,range.offset,cut);
    assert(ahead.offset==page.offset && ahead.bytes==last.size);
    assert(!bili::prefetch_video_page(range,paged,track,0,128,range.offset,range.bytes).bytes);
    assert(!bili::next_video_page(range,paged,nullptr,32,page.offset,page.bytes).bytes);
    auto large=words({0,1,16000,0,0,1,1226165,80000,0x90000000});
    bili::DashStream large_stream;
    large_stream.defaults.track=1;
    assert(bili::parse_segment_index(large,0,large_stream));
    large[24]|=0x80;
    assert(!bili::parse_segment_index(large,0,large_stream));
    bili::Track audio;
    audio.timescale=16000;
    bili::DashRangeParser audio_parser;
    const auto partial=prefix.first(offset+3);
    assert(audio_parser.append(partial,prefix.size(),912,stream,audio));
    assert(audio.samples.size()==2 && audio.sync_samples.empty());
    assert(bili::window(audio.samples[0],912,partial.data(),partial.size()));
    assert(!bili::window(audio.samples[1],912,partial.data(),partial.size()));
    assert(audio_parser.append(prefix,prefix.size(),912,stream,audio));
    assert(audio.samples.size()==4 && bili::window(audio.samples[1],912,prefix.data(),prefix.size()));
    // The partial body already includes the first moof and its time table.
    // Only complete sample payloads can provide network scheduling headroom.
    assert(bili::buffered_through(audio,0,912,partial.size(),audio.samples.front().pts));
    assert(!bili::buffered_through(audio,0,912,partial.size(),audio.samples[1].pts));
    assert(bili::buffered_through(audio,1,912,prefix.size(),audio.samples[1].pts));
    assert(!bili::buffered_through(audio,audio.samples.size(),912,prefix.size(),0));
    media.pop_back();
    assert(!bili::parse_dash_range(media, 912, stream, track));
}
