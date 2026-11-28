// SPDX-License-Identifier: MIT
#define BILLY_TESTING
#include "../client.hpp"
#include <cassert>
#include <fstream>
extern "C" int32_t pxa_call(uint32_t,uint64_t,const uint8_t*,uint32_t){return 0;}
static std::string_view response_body;
extern "C" int32_t pxa_io(uint64_t handle,uint32_t op,uint8_t* out,uint32_t size){
    if (handle!=42 || op!=1) return PXA_STATUS_WOULD_BLOCK;
    size=std::min<size_t>(size,response_body.size());
    if (size) std::memcpy(out,response_body.data(),size);
    response_body.remove_prefix(size);
    return size;
}
namespace bili {
struct ClientTests {
    static void reply(Client& c,Client::Job job,const std::string& json){
        c.job=job;
        assert(c.api.begin(1,"https://api.bilibili.com/test",c.permission,0,json.size()));
        c.api.token=0;c.api.handle=42;c.api.have_headers=true;c.api.status=200;
        response_body=json;
        c.api.pump(1);
        assert(c.api.done && !c.api.failed && c.api.used==json.size());
        c.complete();
    }
    static void draw(Client& c){for(auto [w,h]:{std::pair{320,520},std::pair{569,280},std::pair{320,280}}){media::Canvas canvas;canvas.viewport.width=w;canvas.viewport.height=h;canvas.viewport.content={0,0,w,h};canvas.begin(w,h,0xffffff);c.render(canvas,false,"");assert(!canvas.frame.failed&&canvas.frame.records>0&&canvas.count<=64);}}
    static void run(){Client c;c.permission=uint64_t(1)<<32;c.account_loaded=c.history_loaded=c.saved_loaded=c.searches_loaded=true;
        std::string view=R"({"code":0,"data":{"bvid":"BV1J4411v7g6","title":"多 P 视频","cid":1,"aid":10,"owner":{"mid":20,"name":"UP"},"duration":90,"pages":[{"cid":1,"part":"第一 P","duration":90},{"cid":2,"part":"第二 P","duration":120}]}})";
        Video v;std::snprintf(v.bvid,sizeof(v.bvid),"BV1J4411v7g6");std::snprintf(v.title,sizeof(v.title),"加载中");c.open(v);c.click(140);assert(c.page==Page::detail); // A pending view must not expose stale parts.
        reply(c,Client::Job::view,view);assert(c.parts.size()==2);draw(c);c.click(140);assert(c.page==Page::section&&c.section_data.videos.size()==2);draw(c);
        c.click(401);reply(c,Client::Job::view,view);assert(c.selected.cid==2&&c.selected.duration==120);c.record(33000000);c.click(110);assert(c.resume_us==33000000);c.play_request=false;
        c.selected.cid=1;c.click(110);assert(c.resume_us==0);c.play_request=false;c.back();assert(c.page==Page::section);c.back();assert(c.page==Page::detail);c.back();assert(c.page==Page::home);
        c.click(122);assert(*c.error&&!c.api.active);draw(c);c.click(102);c.click(132);assert(c.page==Page::section&&*c.error);draw(c);c.back();assert(c.page==Page::library);
        assert(c.account.accept_url("https://passport.biligame.com/crossDomain?SESSDATA=fake&bili_jct=fake&DedeUserID=1"));std::snprintf(c.account.name,sizeof(c.account.name),"账号昵称测试");draw(c);
        c.click(133);reply(c,Client::Job::section,R"({"code":0,"data":{"list":[{"id":123,"title":"收藏夹","media_count":7}],"has_more":false}})");assert(c.section==Section::folders&&c.section_data.entries.size()==1);draw(c);
        c.click(600);assert(c.section==Section::favorites&&c.folder_id==123);reply(c,Client::Job::section,R"({"code":0,"data":{"medias":[{"bvid":"BV1J4411v7g6","title":"收藏的视频","cover":"https://i0.hdslb.com/a.jpg","upper":{"name":"UP"},"duration":24977}],"has_more":false}})");assert(c.section_data.videos.size()==1);draw(c);c.back();assert(c.section==Section::folders);
        c.page=Page::home;c.click(122);reply(c,Client::Job::section,R"({"code":0,"data":{"items":[],"offset":"next","has_more":true}})");c.click(106);assert(c.home_page==2&&c.home_cursors.back()=="next");c.click(102);c.click(133);c.back();c.click(101);assert(c.home_page==2&&c.home_cursors.back()=="next");
        c.click(130);reply(c,Client::Job::qr,R"({"code":0,"data":{"url":"https://account.bilibili.com/h5/account-h5/auth/scan-web?navhide=1&qrcode_key=0123456789abcdef0123456789abcdef","qrcode_key":"0123456789abcdef0123456789abcdef"}})");assert(c.login.valid);draw(c);
        reply(c,Client::Job::poll,R"({"code":0,"data":{"code":86090}})");assert(c.login.scanned);c.back();assert(!c.login.valid&&c.page==Page::home);c.click(102);
        c.click(130);reply(c,Client::Job::poll,R"({"code":0,"data":{"code":0,"url":"https://passport.biligame.com/crossDomain?DedeUserID=42&SESSDATA=newfake&bili_jct=newcsrf"}})");
        assert(c.account.logged_in()&&!std::strcmp(c.account.uid,"42")&&c.account_dirty&&c.page==Page::library&&c.trail.empty());
        reply(c,Client::Job::nav,R"({"code":0,"data":{"isLogin":true,"uname":"已登录昵称"}})");assert(!std::strcmp(c.account.name,"已登录昵称"));
        c.click(131);assert(c.account.logged_in());c.click(131);assert(!c.account.logged_in()&&c.account_dirty);draw(c);
    }
};
}
int main(){bili::ClientTests::run();std::puts("Client navigation, selected-part resume, login gating, account logout, folder and cursor isolation, QR and three UI frame sizes passed");}
