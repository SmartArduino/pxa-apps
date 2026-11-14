// SPDX-License-Identifier: Apache-2.0
// Exercise the maintained CameraApp and SDK coroutine/RPC implementation.
// The Host fixture supplies FS replies and codec lifetime spies; it does not
// implement confirmation, selection, busy checks, deletion or pagination.
#if defined(__clang__)
#pragma clang diagnostic push
// Native tests do not import wasm, and the included existing Camera closes
// several unrelated handles without consuming their old return values.
#pragma clang diagnostic ignored "-Wunknown-attributes"
#pragma clang diagnostic ignored "-Wunused-value"
#endif
#include <pxa/app.hpp>
#undef PXA_APPLICATION
#define PXA_APPLICATION(AppType)
#include "../main.cpp"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <cassert>
#include <deque>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {
struct Request {
    uint16_t opcode{};
    uint64_t token{};
    std::vector<std::byte> bytes;
    std::string path;
    uint16_t service=5;
};
struct OpenFile { std::string path; bool directory{}; };
std::deque<Request> pending,asset_pending;
std::vector<std::string> effects;
std::map<std::string,std::vector<std::byte>> files;
std::map<uint64_t,OpenFile> handles;
std::set<OpusDecoder*> decoders;
std::vector<std::byte> photo_fixture,video_fixture;
uint64_t next_handle=UINT64_C(0x100000100);
uint64_t rejected_close{};
unsigned stat_requests{},remove_requests{},decoder_closes{};
constexpr uint64_t image_handle=UINT64_C(0x100000021);

std::string request_path(const std::byte* bytes,size_t size) {
    assert(size>=4 && pxa::wire::get16(bytes)==1);
    auto length=pxa::wire::get16(bytes+2);
    assert(size>=4u+length);
    return {reinterpret_cast<const char*>(bytes+4),length};
}
std::vector<std::byte> read_fixture(const char* name) {
    std::ifstream input(name,std::ios::binary);
    assert(input && "Pass existing native Camera JPEG/MKV fixtures");
    std::vector<char> bytes((std::istreambuf_iterator<char>(input)),{});
    assert(!bytes.empty());
    std::vector<std::byte> result(bytes.size());
    std::memcpy(result.data(),bytes.data(),bytes.size());
    return result;
}
void seed_files() {
    files.clear();
    for(unsigned i=1;i<=5;++i) {
        char name[40];std::snprintf(name,sizeof(name),"album/photo-%06u.jpg",i);
        files.emplace(name,photo_fixture);
    }
    files.emplace("album/video-000001.mkv",video_fixture);
    files.emplace("album/video-000002.mkv",video_fixture);
}
}

extern "C" int32_t pxa_call(uint32_t route,uint64_t token,const uint8_t* data,uint32_t length) {
    const auto service=uint16_t(route),opcode=uint16_t(route>>16);
    const auto* bytes=reinterpret_cast<const std::byte*>(data);
    if(service==1 && opcode==2) {
        assert(length==8);const auto handle=pxa::wire::get64(bytes);
        effects.push_back("close:"+std::to_string(handle));
        if(handle==rejected_close)return int32_t(pxa::Error::busy);
        handles.erase(handle);
        return 0;
    }
    if(service==21 && opcode==6) {
        assert(token && data);asset_pending.push_back(Request{opcode,token,{bytes,bytes+length},{},21});return 0;
    }
    if(service!=5)return 0; // Log/window/cancellation acknowledgements.
    assert(token && data);
    Request request{opcode,token,{bytes,bytes+length},{}};
    if(opcode!=7)request.path=request_path(bytes,length);
    if(opcode==5)++stat_requests;
    if(opcode==3) {
        ++remove_requests;effects.push_back("remove:"+request.path);
        // Real POSIX FS rejects removing a regular file while open. Make the
        // close-before-remove contract fail here rather than silently allowing it.
        for(const auto& [handle,file]:handles) {
            (void)handle;assert(file.directory || file.path!=request.path);
        }
    }
    pending.push_back(std::move(request));
    return 0;
}
extern "C" int32_t pxa_io(uint64_t,uint32_t,uint8_t*,uint32_t) {
    return int32_t(pxa::Error::would_block);
}
// Deletion tests cover decoder ownership, not Opus signal decoding. Actual
// playback/decode remains covered by the real simulator fixture integration.
extern "C" OpusDecoder* opus_decoder_create(opus_int32,int,int* error) {
    if(error)*error=OPUS_OK;
    auto* value=reinterpret_cast<OpusDecoder*>(new unsigned(0));
    assert(decoders.insert(value).second);return value;
}
extern "C" void opus_decoder_destroy(OpusDecoder* decoder) {
    assert(decoders.erase(decoder)==1);++decoder_closes;
    effects.push_back("decoder-close");delete reinterpret_cast<unsigned*>(decoder);
}
extern "C" int opus_decode(OpusDecoder*,const unsigned char*,opus_int32,opus_int16*,int,int) {
    return OPUS_INVALID_PACKET;
}

namespace {
struct TestCamera:CameraApp {
    static inline TestCamera* current{};
    // Keep the actual Camera state/methods; omit only UI mounting and camera
    // discovery so host responses can be advanced deliberately by this test.
    void view()=delete;
    pxa::Result<void> on_start(pxa::Context& ctx,std::span<const std::byte>) {
        current=this;context=&ctx;
        return {};
    }
    void on_foreground(pxa::Context&) {}
    pxa::Result<bool> on_event(pxa::Context&,const pxa::Event&) {return false;}
    pxa::Task<void> hold_playback_file() {
        auto opened=co_await context->fs().open("album/"+media_name,pxa::OpenMode::read);
        assert(opened);playback_file=std::move(*opened);co_return pxa::Result<void>{};
    }
};
using Runtime=pxa::AppRuntime<TestCamera>;
TestCamera& camera() {assert(TestCamera::current);return *TestCamera::current;}
void event_phase() {camera().context->transport().phase(pxa::Phase::event);}
void stop() {
    rejected_close=0;
    Runtime::stop(0);TestCamera::current=nullptr;
    pending.clear();asset_pending.clear();handles.clear();assert(decoders.empty());
}
void reset(std::string name="photo-000005.jpg",bool video=false,uint32_t sequence=5) {
    stop();effects.clear();stat_requests=remove_requests=decoder_closes=0;
    seed_files();assert(Runtime::start(nullptr,0)==0);
    auto& app=camera();app.foreground=true;app.album=true;app.gallery=false;
    app.preview=app.running=app.photograph=app.record=app.loading=app.playing=false;
    app.album_video=video;app.media_name=std::move(name);app.media_sequence=sequence;
    auto found=files.find("album/"+app.media_name);
    app.media_size=found==files.end()?photo_fixture.size():found->second.size();
    app.media_epoch=42;app.grid_page=7;app.grid_anchor=123;app.grid_newer=true;
    event_phase();
}
Request take(uint16_t opcode) {
    assert(!pending.empty());auto request=std::move(pending.front());pending.pop_front();
    assert(request.opcode==opcode);return request;
}
void deliver(const Request& request,std::span<const std::byte> body,pxa::Error error=pxa::Error{}) {
    std::vector<std::byte> packet(24+body.size());
    pxa::wire::put16(packet.data(),request.service);pxa::wire::put16(packet.data()+2,request.opcode);
    pxa::wire::put64(packet.data()+4,request.token);
    pxa::wire::put32(packet.data()+12,uint32_t(body.size()+4));
    pxa::wire::put32(packet.data()+20,uint32_t(error));
    std::copy(body.begin(),body.end(),packet.begin()+24);
    assert(Runtime::event(reinterpret_cast<const uint8_t*>(packet.data()),uint32_t(packet.size()))==1);
}
void stat_reply(pxa::FileKind kind=pxa::FileKind::regular,std::optional<uint64_t> size={}) {
    auto request=take(5);assert(request.path=="album/"+camera().media_name);
    std::array<std::byte,9> bytes{};bytes[0]=std::byte(kind);
    pxa::wire::put64(bytes.data()+1,size.value_or(camera().media_size));deliver(request,bytes);
}
uint64_t opened_reply() {
    auto request=take(1);const auto flags=pxa::wire::get32(request.bytes.data()+request.bytes.size()-4);
    const auto handle=next_handle++;handles.emplace(handle,OpenFile{request.path,bool(flags&64)});
    std::array<std::byte,8> bytes{};pxa::wire::put64(bytes.data(),handle);deliver(request,bytes);return handle;
}
void directory_entry(const std::string& name,bool end=false) {
    auto request=take(7);
    if(end) {
        std::array<std::byte,4> payload{};pxa::wire::put16(payload.data(),7);deliver(request,payload);return;
    }
    const auto found=files.find("album/"+name);assert(found!=files.end());
    std::vector<std::byte> bytes(4+name.size()+5+12);
    pxa::wire::put16(bytes.data(),4);pxa::wire::put16(bytes.data()+2,uint16_t(name.size()));
    std::memcpy(bytes.data()+4,name.data(),name.size());size_t at=4+name.size();
    pxa::wire::put16(bytes.data()+at,5);pxa::wire::put16(bytes.data()+at+2,1);bytes[at+4]=std::byte{1};at+=5;
    pxa::wire::put16(bytes.data()+at,6);pxa::wire::put16(bytes.data()+at+2,8);
    pxa::wire::put64(bytes.data()+at+4,found->second.size());deliver(request,bytes);
}
void remove_reply(pxa::Error error=pxa::Error{}) {
    auto request=take(3);
    if(error==pxa::Error{})assert(files.erase(request.path)==1);
    deliver(request,{},error);
}
uint64_t hold_file() {
    event_phase();assert(camera().context->tasks().start(camera().hold_playback_file()));
    const auto handle=opened_reply();assert(camera().playback_file.handle()==handle);return handle;
}
void hold_image() {
    auto& app=camera();pxa::AssetDescriptor descriptor{};descriptor.kind=pxa::AssetKind::image;
    descriptor.width=descriptor.height=1;
    app.image.emplace(app.context->transport(),image_handle,descriptor);
}
void request_and_confirm() {
    event_phase();camera().request_delete();assert(camera().delete_target);
    assert(camera().delete_target->name==camera().media_name);
    camera().confirm_delete();assert(camera().deleting && !camera().delete_target);
    // Repeated touch/controller activation cannot schedule a second removal.
    camera().confirm_delete();camera().request_delete();
    assert(stat_requests==1 && remove_requests==0);
}
void assert_no_delete(const std::map<std::string,std::vector<std::byte>>& before) {
    assert(!remove_requests && files==before);
}
void cancel_preserves_selected_resources() {
    reset();const auto before=files;hold_image();
    event_phase();camera().request_delete();assert(camera().delete_target);
    assert(camera().delete_target->name=="photo-000005.jpg");
    camera().cancel_delete();
    assert(!camera().delete_target && camera().media_name=="photo-000005.jpg");
    assert(camera().image && camera().image->handle()==image_handle && effects.empty());
    assert(!stat_requests);assert_no_delete(before);
    // System back cancels the confirmation before it navigates away.
    camera().request_delete();assert(camera().on_back()==pxa::BackAction::stay);
    assert(!camera().delete_target && !camera().gallery && camera().media_name=="photo-000005.jpg");
    assert_no_delete(before);
}
void confirmed_exact_photo_preserves_every_other_file() {
    reset();const auto before=files;hold_image();request_and_confirm();
    const auto target=camera().media_name;
    // Normal navigation/capture handlers cannot change the active confirmation.
    camera().load_album(1);camera().select_mode(true);camera().close_album();camera().return_to_grid();
    camera().take_photo();camera().toggle_record();
    assert(camera().media_name==target && camera().deleting && !camera().gallery && !camera().photograph && !camera().record);
    stat_reply();assert(remove_requests==1 && !camera().image);
    auto close=std::find(effects.begin(),effects.end(),"close:"+std::to_string(image_handle));
    auto remove=std::find(effects.begin(),effects.end(),"remove:album/photo-000005.jpg");
    assert(close!=effects.end() && remove!=effects.end() && close<remove);
    remove_reply();assert(!files.contains("album/photo-000005.jpg"));
    for(const auto& [name,bytes]:before)if(name!="album/photo-000005.jpg")assert(files.at(name)==bytes);
    assert(remove_requests==1 && camera().gallery && camera().grid_page==0 && camera().grid_anchor==0 && !camera().grid_newer);
    assert(!pending.empty() && pending.front().opcode==1 && pending.front().path=="album");
}
void rejects_busy_and_invalid_selection() {
    for(unsigned which=0;which<11;++which) {
        reset();const auto before=files;auto& app=camera();
        switch(which) {
            case 0:app.loading=true;break;case 1:app.playing=true;break;
            case 2:app.running=true;break;case 3:app.photograph=true;break;
            case 4:app.record=true;break;case 5:app.discovering=true;break;
            case 6:app.pending_back=CameraApp::BackTarget::gallery;break;
            case 7:app.foreground=false;break;case 8:app.gallery=true;break;
            case 9:app.album=false;break;case 10:app.preview=true;break;
        }
        app.request_delete();assert(!app.delete_target && !stat_requests);assert_no_delete(before);
    }
    // A just-paused playback task still has loading=true until it unwinds.
    reset("video-000001.mkv",true,1);camera().playing=false;camera().loading=true;
    camera().request_delete();assert(!camera().delete_target && !stat_requests && !remove_requests);
    // Recording shutdown can own a file after record became false.
    reset();const auto opened=hold_file();camera().video_file=std::move(camera().playback_file);
    event_phase();camera().request_delete();assert(!camera().delete_target && !stat_requests && !remove_requests);
    assert(camera().video_file.handle()==opened);
    for(const std::string& name:std::array<std::string,8>{"../photo-000005.jpg","/photo-000005.jpg","photo-000005.jpg/../photo-000001.jpg",
        "photo-000005.jpg\\",std::string("photo-000005.jpg\0other",22),"video-000001.mkv","photo-000000.jpg","photo-4294967296.jpg"}) {
        reset(name);const auto before=files;camera().request_delete();
        assert(!camera().delete_target && !stat_requests);assert_no_delete(before);
    }
    reset();camera().media_sequence=4;camera().request_delete();assert(!camera().delete_target && !stat_requests);
}
void stale_confirmation_and_delayed_stat_cannot_remove() {
    reset();auto before=files;camera().request_delete();++camera().media_epoch;
    camera().confirm_delete();assert(!camera().delete_target && !stat_requests);assert_no_delete(before);
    for(unsigned changed=0;changed<6;++changed) {
        reset();before=files;request_and_confirm();
        auto request=take(5);const auto size=camera().media_size;
        switch(changed) {
            case 0:++camera().media_epoch;break;case 1:camera().media_name="photo-000004.jpg";break;
            case 2:camera().album_video=true;break;case 3:camera().loading=true;break;
            case 4:camera().foreground=false;break;case 5:camera().playing=true;break;
        }
        std::array<std::byte,9> body{};body[0]=std::byte{1};pxa::wire::put64(body.data()+1,size);deliver(request,body);
        assert_no_delete(before);
    }
    for(unsigned altered=0;altered<3;++altered) {
        reset();before=files;request_and_confirm();
        if(altered==0)stat_reply(pxa::FileKind::regular,camera().media_size+1);
        else if(altered==1)stat_reply(pxa::FileKind::directory);
        else deliver(take(5),{},pxa::Error::not_found);
        assert_no_delete(before);
    }
}
void paused_video_closes_before_exact_remove() {
    reset("video-000001.mkv",true,1);files.emplace("album/video-000001.avi",photo_fixture);
    const auto before=files;const auto file=hold_file();hold_image();
    int error{};camera().playback_decoder=opus_decoder_create(16000,1,&error);assert(error==OPUS_OK);
    request_and_confirm();stat_reply();assert(remove_requests==1 && !camera().playback_file && !camera().image && !camera().playback_decoder && decoder_closes==1);
    const auto closed=std::find(effects.begin(),effects.end(),"close:"+std::to_string(file));
    const auto removed=std::find(effects.begin(),effects.end(),"remove:album/video-000001.mkv");
    assert(closed!=effects.end() && removed!=effects.end() && closed<removed);
    remove_reply();for(const auto& [name,bytes]:before)if(name!="album/video-000001.mkv")assert(files.at(name)==bytes);
    assert(!files.contains("album/video-000001.mkv") && files.contains("album/video-000001.avi"));
    // Rejecting a close retains ownership and must prevent deletion.
    reset("video-000001.mkv",true,1);const auto preserved=files;const auto held=hold_file();rejected_close=held;
    request_and_confirm();stat_reply();assert(camera().playback_file.handle()==held);assert_no_delete(preserved);
}
void failed_remove_reopens_the_exact_video_name() {
    reset("video-000001.mkv",true,1);files.emplace("album/video-000001.avi",photo_fixture);
    const auto before=files;hold_file();request_and_confirm();stat_reply();
    assert(remove_requests==1);remove_reply(pxa::Error::busy);assert(files==before);
    assert(!pending.empty() && pending.front().opcode==1 && pending.front().path=="album");
    opened_reply();directory_entry("video-000001.mkv");directory_entry("video-000001.avi");directory_entry({},true);
    assert(!pending.empty() && pending.front().opcode==1 && pending.front().path=="album/video-000001.mkv");
    assert(camera().media_name=="video-000001.mkv" && remove_requests==1);
}
void background_during_failed_remove_restore_drops_late_open() {
    reset("video-000001.mkv",true,1);const auto before=files;
    hold_file();request_and_confirm();stat_reply();remove_reply(pxa::Error::busy);
    // Advance the actual failed-remove rescan until its selected-file open is
    // pending. The real on_background runs before that response arrives.
    opened_reply();directory_entry("video-000001.mkv");directory_entry({},true);
    assert(camera().loading && !camera().playback_file && pending.size()==1 &&
        pending.front().path=="album/video-000001.mkv");
    event_phase();camera().on_background(*camera().context);
    const auto opened=opened_reply();
    assert(!camera().foreground && !camera().loading && !camera().playback_file && !camera().image);
    assert(!handles.contains(opened) && pending.empty() && asset_pending.empty());
    assert(std::find(effects.begin(),effects.end(),"close:"+std::to_string(opened))!=effects.end());
    assert(files==before && remove_requests==1);
}
void background_during_photo_decode_drops_late_asset() {
    reset();const auto before=files;
    event_phase();camera().load_album(5,"photo-000005.jpg");
    opened_reply();directory_entry("photo-000005.jpg");directory_entry({},true);
    const auto source=opened_reply();
    assert(camera().loading && !camera().image && asset_pending.size()==1);
    event_phase();camera().on_background(*camera().context);
    auto request=std::move(asset_pending.front());asset_pending.pop_front();
    const auto decoded=next_handle++;
    std::array<std::byte,36> body{};
    pxa::wire::put64(body.data(),decoded);body[8]=std::byte(pxa::AssetKind::image);
    pxa::wire::put16(body.data()+12,1);pxa::wire::put16(body.data()+14,1);
    pxa::wire::put32(body.data()+16,4);pxa::wire::put32(body.data()+20,4);pxa::wire::put32(body.data()+24,4);
    pxa::wire::put16(body.data()+28,320);pxa::wire::put16(body.data()+30,240);
    pxa::wire::put32(body.data()+32,uint32_t(photo_fixture.size()));deliver(request,body);
    assert(!camera().foreground && !camera().loading && !camera().playback_file && !camera().image);
    assert(!handles.contains(source) && pending.empty() && asset_pending.empty());
    assert(std::find(effects.begin(),effects.end(),"close:"+std::to_string(decoded))!=effects.end());
    assert_no_delete(before);
}
void large_photo_skips_decode_but_remains_exactly_deletable() {
    reset();
    constexpr auto limit=512u*1024u;
    static_assert(CameraApp::viewing_jpeg_limit==limit);
    files.at("album/photo-000005.jpg").resize(limit+1,std::byte{0x5a});
    const auto before=files;
    event_phase();camera().load_album(5,"photo-000005.jpg");
    opened_reply();directory_entry("photo-000005.jpg");directory_entry({},true);
    assert(!camera().loading && !camera().playback_file && !camera().image && asset_pending.empty() && pending.empty());
    assert(camera().media_name=="photo-000005.jpg" && camera().media_sequence==5 && camera().media_size==limit+1);
    assert(camera().status.get()=="文件较大，暂时无法查看");
    request_and_confirm();stat_reply();assert(remove_requests==1);remove_reply();
    assert(!files.contains("album/photo-000005.jpg"));
    for(const auto& [name,bytes]:before)if(name!="album/photo-000005.jpg")assert(files.at(name)==bytes);
}
void deleting_last_photo_resets_empty_last_page() {
    reset();
    for(unsigned i=1;i<=4;++i){char name[40];std::snprintf(name,sizeof(name),"album/photo-%06u.jpg",i);files.erase(name);}
    const auto videos=files.at("album/video-000001.mkv");
    request_and_confirm();stat_reply();remove_reply();
    assert(camera().gallery && camera().grid_page==0 && camera().grid_anchor==0);
    opened_reply();directory_entry("video-000001.mkv");directory_entry("video-000002.mkv");directory_entry({},true);
    assert(!camera().loading && !camera().item_count && !camera().media_total);
    assert(camera().page_label.get().starts_with("1 / 1") && pending.empty());
    assert(files.size()==2 && files.at("album/video-000001.mkv")==videos && remove_requests==1);
}
}

int main(int argc,char** argv) {
    assert(argc==3 && "usage: gallery_delete_test EXISTING_JPEG EXISTING_MKV");
    photo_fixture=read_fixture(argv[1]);video_fixture=read_fixture(argv[2]);
    assert(camera_media::jpeg_size(photo_fixture).width);
    cancel_preserves_selected_resources();
    confirmed_exact_photo_preserves_every_other_file();
    rejects_busy_and_invalid_selection();
    stale_confirmation_and_delayed_stat_cannot_remove();
    paused_video_closes_before_exact_remove();
    failed_remove_reopens_the_exact_video_name();
    background_during_failed_remove_restore_drops_late_open();
    background_during_photo_decode_drops_late_asset();
    large_photo_skips_decode_but_remains_exactly_deletable();
    deleting_last_photo_resets_empty_last_page();
    stop();
    std::puts("Camera actual gallery: cancel/back, exact confirmation, preservation, busy/stale/stat guards, paused playback close, restore identity, late background cleanup, large-file selection and empty pagination passed");
}
