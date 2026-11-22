// SPDX-License-Identifier: MIT
#pragma once
#include "catalog.hpp"
#include "sections.hpp"
#include "covers.hpp"
#include "http.hpp"
#include "widgets.hpp"
#include "../common/media/canvas.hpp"
#include "pxa_storage.h"
namespace bili {
enum class Page {home,search,library,detail,section,login};
class Client {
#ifdef BILLY_TESTING
    friend struct ClientTests;
#endif
    enum class Job {none,popular,search,view,nav,spi,section,qr,poll};
    Http api;
    Account account;
    LoginCode login;
    Section section=Section::popular,home_section=Section::popular;
    SectionData section_data,home_data;
    std::vector<std::string> home_cursors{std::string{}};unsigned home_page=1;
    std::vector<Video> parts;
    std::vector<std::string> cursors{std::string{}};
    std::string mixin;
    Video section_context{};
    unsigned section_page=1;uint64_t folder_id=0;
    uint64_t account_get=0;bool account_loaded=false,account_dirty=false,nav_loaded=false,logout_confirm=false;
    uint8_t qr_commands[65536]{};

    Covers covers;
    Job job=Job::none;
    struct Navigation {
        Page page;int scroll;Video selected,context;Section section;unsigned number;uint64_t folder;
        std::vector<std::string> cursors;SectionData data;std::vector<Video> parts;std::string error;
    };
    std::vector<Navigation> trail;
    std::vector<Video> popular,results;
    Library history,saved;
    SearchHistory searches;
    uint64_t search_get=0;bool searches_loaded=false,searches_dirty=false;unsigned save_kind=0;
    uint64_t sequence=0xb1000000,permission=0,history_get=0,saved_get=0,save_token=0;
    uint64_t now=0;
    bool history_loaded=false,saved_loaded=false,history_dirty=false,saved_dirty=false;
    bool history_loading=true,saved_loading=true;
    bool pointer_down=false,scrolled=false,library_saved=false,initial_fetch=false,deferred_fetch=false;
    int down_y=0,down_scroll=0,max_scroll=0;
    unsigned popular_page=1,search_page=1;
    uint8_t packet[4096]{};
    char error[160]{},info[128]{};
    uint64_t info_until=0;
    static inline uint32_t &background=ui::colors.background,&ink=ui::colors.text,&muted=ui::colors.muted,&pink=ui::colors.primary;
    void clear_account(){auto buvid=std::string(account.buvid);account={};std::snprintf(account.buvid,sizeof(account.buvid),"%s",buvid.c_str());
        account_dirty=true;logout_confirm=false;trail.clear();section_data.clear();home_data.clear();popular.clear();home_cursors={std::string{}};home_page=1;}
    void cancel_api(){api.cancel();job=Job::none;}
    void get(const char *url,Job next) {
        covers.pause();cancel_api();job=next;error[0]=0;
        if(!permission){want_network=true;deferred_fetch=true;job=Job::none;return;}
        auto cookies=account.cookie();char referer[96]{};bool author=next==Job::section&&section==Section::author;
        if(author)std::snprintf(referer,sizeof(referer),"https://space.bilibili.com/%llu",(unsigned long long)section_context.mid);
        if(!api.begin(++sequence,url,permission,now,384*1024,false,0,cookies.c_str(),author?referer:nullptr)){std::snprintf(error,sizeof(error),"无法发起网络请求，请重试");job=Job::none;}
        dirty=true;
    }
    std::string signed_query(std::vector<std::pair<std::string,std::string>> params) {
        auto epoch=std::time(nullptr);auto query=wbi_query(std::move(params),mixin,epoch>0?uint64_t(epoch):0);
        if(query.empty())std::snprintf(error,sizeof(error),"签名或系统时间不可用，请刷新重试");return query;
    }
    void load_nav(){get("https://api.bilibili.com/x/web-interface/nav",Job::nav);}
    void login_start(){login={};get("https://passport.bilibili.com/x/passport-login/web/qrcode/generate",Job::qr);}
    void fetch_section() {
        char url[1400]{};const auto& v=section_context;
        if(needs_account(section)&&!account.logged_in()){std::snprintf(error,sizeof(error),"登录后可查看%s",section_title(section));dirty=true;return;}
        switch(section){
        case Section::recommend:{auto q=signed_query({{"ps","12"},{"fresh_type","4"},{"feed_version","V8"},{"fresh_idx",std::to_string(popular_page)},
            {"fresh_idx_1h",std::to_string(popular_page)},{"fetch_row",std::to_string(popular_page)},{"brush","0"},{"homepage_ver","1"},{"y_num","5"},{"last_y_num","5"},{"web_location","1430650"}});
            if(q.empty())return;std::snprintf(url,sizeof(url),"https://api.bilibili.com/x/web-interface/wbi/index/top/feed/rcmd?%s",q.c_str());break;}
        case Section::dynamic:std::snprintf(url,sizeof(url),"https://api.bilibili.com/x/polymer/web-dynamic/v1/feed/all?type=video&offset=%s",url_encode(page==Page::home?home_cursors.back():cursors.back()).c_str());break;
        case Section::cloud_history:std::snprintf(url,sizeof(url),"https://api.bilibili.com/x/web-interface/history/cursor?%s",cursors.back().empty()?"max=0&view_at=0&business=":cursors.back().c_str());break;
        case Section::folders:std::snprintf(url,sizeof(url),"https://api.bilibili.com/x/v3/fav/folder/created/list?up_mid=%s&pn=%u&ps=20",account.uid,section_page);break;
        case Section::favorites:std::snprintf(url,sizeof(url),"https://api.bilibili.com/x/v3/fav/resource/list?media_id=%llu&pn=%u&ps=12&order=mtime&type=0&tid=0&platform=web",(unsigned long long)folder_id,section_page);break;
        case Section::watch_later:std::snprintf(url,sizeof(url),"https://api.bilibili.com/x/v2/history/toview");break;
        case Section::author:{auto q=signed_query({{"mid",std::to_string(v.mid)},{"pn",std::to_string(section_page)},{"ps","12"},{"order","pubdate"},{"platform","web"},{"web_location","333.1387"},{"tid","0"},{"order_avoided","true"},{"dm_img_list","[]"},{"dm_img_inter",R"({"ds":[],"wh":[0,0,0],"of":[0,0,0]})"},{"dm_img_str",md5(std::to_string(now)+"billy")},{"dm_cover_img_str",md5(std::to_string(now)+"cover")}});
            if(q.empty())return;std::snprintf(url,sizeof(url),"https://api.bilibili.com/x/space/wbi/arc/search?%s",q.c_str());break;}
        case Section::related:std::snprintf(url,sizeof(url),"https://api.bilibili.com/x/web-interface/archive/related?bvid=%s",v.bvid);break;
        case Section::comments:if(!v.aid){notice("请先等待视频信息加载完成");return;}std::snprintf(url,sizeof(url),"https://api.bilibili.com/x/v2/reply?type=1&oid=%llu&pn=%u&ps=20&sort=2",(unsigned long long)v.aid,section_page);break;
        case Section::followings:std::snprintf(url,sizeof(url),"https://api.bilibili.com/x/relation/followings?vmid=%s&pn=%u&ps=20&order=desc",account.uid,section_page);break;
        case Section::parts:dirty=true;return;
        default:return;}
        get(url,Job::section);
    }
    bool push_page(){if(trail.size()>=8){notice("请先返回上一层再继续浏览");return false;}
        Navigation state{page,scroll,selected,section_context,section,section_page,folder_id,cursors,{},{},error};
        if(page==Page::section)state.data=section_data;if(page==Page::detail)state.parts=parts;trail.push_back(std::move(state));return true;}
    void enter_section(Section next){if(!push_page())return;cancel_api();
        section_context=selected;section=next;page=Page::section;section_page=1;cursors={std::string{}};section_data.clear();scroll=0;error[0]=0;dirty=true;
        if(next==Section::parts){section_data.videos=parts;return;}fetch_section();}
    void fetch_list() {
        if(page==Page::section){fetch_section();return;}if(page==Page::login){login_start();return;}
        char url[1024];
        if(page==Page::home && home_section!=Section::popular){section=home_section;fetch_section();return;}
        if(page==Page::home){std::snprintf(url,sizeof(url),"https://api.bilibili.com/x/web-interface/popular?pn=%u&ps=12",popular_page);get(url,Job::popular);}
        else if(page==Page::search && *query){
            std::string trimmed=query;auto first=trimmed.find_first_not_of(" \t\r\n");
            if(first==std::string::npos){query[0]=0;input_changed=dirty=true;return;}
            trimmed=trimmed.substr(first,trimmed.find_last_not_of(" \t\r\n")-first+1);
            std::snprintf(query,sizeof(query),"%s",trimmed.c_str());
            searches.put(query);searches_dirty=true;
            char id[13]{};if(extract_id(query,id)){Video v{};std::memcpy(v.bvid,id,13);std::snprintf(v.title,sizeof(v.title),"%s",id);open(v);return;}
            char encoded[640]{};if(!media::json::encode(query,encoded,sizeof(encoded))){notice("搜索词过长");return;}
            std::snprintf(url,sizeof(url),"https://api.bilibili.com/x/web-interface/search/type?search_type=video&keyword=%s&page=%u&order=totalrank",encoded,search_page);get(url,Job::search);
        }
    }
    void complete() {
        auto old_job=job;job=Job::none;
        if(api.failed){std::snprintf(error,sizeof(error),"网络连接失败，请重试");dirty=true;return;}
        media::json::Value root{api.data.data(),api.used};
        if(old_job==Job::nav){
            auto data=root.get("data"),wbi=data.get("wbi_img");char img[256]{},sub[256]{};wbi.get("img_url").string(img,sizeof(img));wbi.get("sub_url").string(sub,sizeof(sub));mixin=mixin_key(img,sub);
            if(account.logged_in()&&data.get("isLogin").boolean())data.get("uname").string(account.name,sizeof(account.name));
            else if(account.logged_in()&&root.get("code").number(1)!=0){notice("登录已过期，请重新扫码");clear_account();}
            nav_loaded=true;get("https://api.bilibili.com/x/frontend/finger/spi",Job::spi);return;
        }
        if(old_job==Job::spi){char b[129]{};root.get("data").get("b_3").string(b,sizeof(b));if(Account::token(b,sizeof(account.buvid)))std::snprintf(account.buvid,sizeof(account.buvid),"%s",b);
            fetch_list();dirty=true;return;}
        if(root.get("code").number(1)) {
            char message[100]{};root.get("message").string(message,sizeof(message));
            std::snprintf(error,sizeof(error),"%s",*message?message:"站点暂未返回结果，请重试");dirty=true;return;
        }
        if(old_job==Job::qr){if(!login.generate(root.get("data"),now))std::snprintf(error,sizeof(error),"二维码生成失败，请重试");}
        else if(old_job==Job::poll){auto data=root.get("data");auto code=data.get("code").number(1);
            if(code==0){char url[2048]{};bool cut=false;data.get("url").string(url,sizeof(url),&cut);
                if(!account.accept_cookie_headers(api.response_cookies)&&(cut||!account.accept_url(url))){login.valid=false;std::snprintf(error,sizeof(error),"未能读取登录信息，请重新扫码");}
                else{account_loaded=account_dirty=true;login={};page=Page::library;trail.clear();scroll=0;notice("登录成功");load_nav();return;}}
            else if(code==86090)login.scanned=true;
            else if(code!=86101){login.valid=false;std::snprintf(error,sizeof(error),"二维码已过期，请刷新");}
            login.next_poll=now+2500;
        }
        else if(old_job==Job::section){if(page==Page::home){home_data.parse(root,home_section,section_context);popular=home_data.videos;}else section_data.parse(root,section,section_context);}
        else if(old_job==Job::popular)popular=parse_list(root,false);
        else if(old_job==Job::search)results=parse_list(root,true);
        else if(old_job==Job::view){if(!apply_view_data(root))std::snprintf(error,sizeof(error),"视频信息不完整，请重试");}
        std::vector<uint8_t>().swap(api.data);dirty=true;
    }
    void save_pump() {
        if(save_token)return;
        if(account_dirty&&account_loaded){auto bytes=account.encode();save_kind=3;save_token=++sequence;
            if(pxa_storage_request_set(packet,sizeof(packet),save_token,"billy.account.v1",16,bytes.data(),bytes.size())){save_token=0;notice("登录状态保存失败");}else account_dirty=false;return;}
        if(searches_dirty&&searches_loaded){auto bytes=searches.encode();save_kind=2;save_token=++sequence;
            if(pxa_storage_request_set(packet,sizeof(packet),save_token,"bili.search.v1",14,bytes.data(),bytes.size())){save_token=0;notice("搜索记录保存失败");}else searches_dirty=false;return;}
        bool h=history_dirty&&history_loaded,s=saved_dirty&&saved_loaded;if(!h&&!s)return;
        auto bytes=(h?history:saved).encode();auto key=h?"bili.history.v1":"bili.saved.v1";
        save_token=++sequence;save_kind=h?0:1;
        if(pxa_storage_request_set(packet,sizeof(packet),save_token,key,std::strlen(key),bytes.data(),bytes.size())){save_token=0;notice("本地记录保存失败");return;}
        if(h)history_dirty=false;else saved_dirty=false;
    }
    static std::vector<std::string> lines(const char *text,int width,unsigned limit=2) {
        std::vector<std::string> rows;std::string row;int used=0;
        const auto *p=reinterpret_cast<const unsigned char*>(text);
        while(*p && rows.size()<limit) {
            unsigned n=*p<0x80?1:(*p&0xe0)==0xc0?2:(*p&0xf0)==0xe0?3:4;
            for(unsigned i=1;i<n;++i)if(!p[i] || (p[i]&0xc0)!=0x80){n=1;break;}
            int length=*p<0x80?7:14;
            if(used+length>width && !row.empty()){rows.push_back(row);row.clear();used=0;if(rows.size()==limit)break;}
            row.append(reinterpret_cast<const char*>(p),n);used+=length;p+=n;
        }
        if(!row.empty()&&rows.size()<limit)rows.push_back(row);
        if(*p && !rows.empty()){auto& last=rows.back();int extent=0;
            for(unsigned char ch:last)if((ch&0xc0)!=0x80)extent+=ch<0x80?7:14;
            while(!last.empty() && extent+14>width) {
            size_t i=last.size()-1;while(i && (uint8_t(last[i])&0xc0)==0x80)--i;
            extent-=uint8_t(last[i])<0x80?7:14;last.resize(i);}
            last+="…";}
        return rows;
    }
    static int text(media::Canvas& c,int x,int y,int width,const char *value,unsigned count=2,uint32_t color=ink) {
        auto rows=lines(value,width,count);for(auto& row:rows){ui::label(c,x,y,width,19,row.c_str(),color);y+=19;}return int(rows.size())*19;
    }
    static void count_text(uint32_t count,char *out,size_t size) {
        if(count>=100000000)std::snprintf(out,size,"%.1f亿",double(count)/100000000);
        else if(count>=10000)std::snprintf(out,size,"%.1f万",double(count)/10000);else std::snprintf(out,size,"%u",count);
    }
    void cover(media::Canvas& c,const Video& v,int x,int y,int w,int h) {
        c.rect(x,y,w,h,ui::colors.fill,6);uint64_t image=covers.want(v.pic);
        if(image)c.image(x,y,w,h,image);
        else {ui::icon(c,"play",x+w/2,y+h/2,muted,24);}
        char time[32];duration_text(v.duration,time,sizeof(time));
        // The box must exceed the font's line height for Host middle alignment.
        int badge_w=std::min(w-8,std::max(48,int(std::strlen(time))*8+12));
        int bx=x+w-badge_w-4,by=y+h-28;
        c.rect(bx,by,badge_w,24,0x24292f,4);ui::label(c,bx+4,by,badge_w-8,24,time,0xffffff,1);
    }
    void nav(media::Canvas& c) {
        int top=c.h-55;c.rect(0,top,c.w,55,ui::colors.surface);c.line(0,top,c.w,top,ui::colors.border,1);
        for(unsigned i=0;i<2;++i){int x=c.w*int(i*2+1)/4;bool on=i==0?page!=Page::library:page==Page::library;
            if(on)c.rect(x-27,top+5,54,25,ui::colors.selection,12);
            ui::icon(c,i?"person":"home",x,top+17,on?pink:muted,22);ui::label(c,x-48,top+31,96,22,i?"我的":"首页",on?pink:muted,1);
            c.hit(i?102:101,int(i)*c.w/2,top,c.w/2,55);}
    }
    void cards(media::Canvas& c,const std::vector<Video>& videos,int top,int bottom,unsigned base) {
        int columns=c.w>=520?3:c.w>=280?2:1,gap=10,width=(c.w-24-gap*(columns-1))/columns;
        bool compact=c.w>=500&&c.h<380;int image_h=width*9/16,row_h=image_h+(compact?47:65);
        max_scroll=std::max(0,int((videos.size()+columns-1)/columns)*row_h-(bottom-top));scroll=std::clamp(scroll,0,max_scroll);
        c.clip(0,top,c.w,bottom-top);
        for(unsigned i=0;i<videos.size();++i){int x=12+int(i%columns)*(width+gap),y=top+int(i/columns)*row_h-scroll;
            if(y+row_h<top||y>=bottom||(y>=top&&bottom-y<24))continue;
            c.rect(x,y,width,row_h-10,ui::colors.surface,6);cover(c,videos[i],x,y,width,image_h);
            text(c,x+5,y+image_h+7,width-10,videos[i].title,compact?1:2);
            c.text(x+5,y+image_h+(compact?27:46),width-10,videos[i].author,muted);
            c.hit(base+i,x,std::max(top,y),width,std::min(bottom,y+row_h-10)-std::max(top,y));}
        c.unclip();
    }
public:
    Page page=Page::home;
    Video selected{};
    char query[192]{};
    unsigned quality=16;
    uint32_t max_pixels=0;
    bool dirty=true,want_network=true,in_player=false,play_request=false,input_changed=false;
    uint64_t resume_us=0;
    int scroll=0;
    void start() {
        account_get=++sequence;pxa_storage_request_get(account_get,"billy.account.v1",16);
        history_get=++sequence;saved_get=++sequence;search_get=++sequence;
        pxa_storage_request_get(search_get,"bili.search.v1",14);
        pxa_storage_request_get(history_get,"bili.history.v1",15);pxa_storage_request_get(saved_get,"bili.saved.v1",13);
    }
    void notice(const char *message){std::snprintf(info,sizeof(info),"%s",message);info_until=now+3500;dirty=true;}
    void permission_result(bool granted){if(!granted){want_network=false;std::snprintf(error,sizeof(error),"需要网络权限才能浏览视频");dirty=true;}}
    bool event(const pxa_event_t& e) {
        if(covers.event(e))return true;
        if(account_get && e.service==PXA_STORAGE_SERVICE && e.opcode==PXA_STORAGE_GET && e.token==account_get){pxa_storage_get_result_t r{};
            account_loaded=pxa_storage_parse_get(&e,account_get,&r)&&(r.status==PXA_STATUS_NOT_FOUND||(!r.status&&account.decode(r.value.data,r.value.size)));account_get=0;dirty=true;
            if(!account_loaded)notice("登录状态读取失败，原数据已保留");return true;}
        if(api.token && e.service==PXA_NET_SERVICE && e.opcode==PXA_HTTP_OPEN && e.token==api.token)return api.event(e);
        if(search_get && e.service==PXA_STORAGE_SERVICE && e.opcode==PXA_STORAGE_GET && e.token==search_get){
            pxa_storage_get_result_t r{};bool ok=pxa_storage_parse_get(&e,search_get,&r);
            searches_loaded=ok&&(r.status==PXA_STATUS_NOT_FOUND||(!r.status&&searches.decode(r.value.data,r.value.size)));search_get=0;dirty=true;return true;}
        if(e.service==PXA_STORAGE_SERVICE && e.opcode==PXA_STORAGE_GET && (e.token==history_get||e.token==saved_get)) {
            bool h=e.token==history_get;pxa_storage_get_result_t result{};
            bool valid=pxa_storage_parse_get(&e,e.token,&result);
            bool loaded=valid && (result.status==PXA_STATUS_NOT_FOUND || (!result.status && (h?history:saved).decode(result.value.data,result.value.size)));
            if(h){history_loaded=loaded;history_loading=false;history_get=0;}else{saved_loaded=loaded;saved_loading=false;saved_get=0;}
            if(!loaded)notice("本地记录读取失败，已保留原数据");dirty=true;return true;
        }
        if(save_token && e.service==PXA_STORAGE_SERVICE && e.token==save_token) {
            int32_t status=0;
            if(!pxa_storage_parse_status(&e,save_token,PXA_STORAGE_SET,&status)||status){
                if(save_kind==3)account_loaded=false;else if(save_kind==2)searches_loaded=false;else if(save_kind==0)history_loaded=false;else saved_loaded=false;notice("本地记录保存失败");}
            save_token=0;return true;
        }
        return false;
    }
    void pump(uint64_t time,uint64_t network,bool foreground) {
        now=time;permission=network;save_pump();
        if(!foreground || in_player)return;
        if(network&&!initial_fetch&&account_get==0){initial_fetch=true;load_nav();}
        if(network && deferred_fetch){initial_fetch=true;deferred_fetch=false;if(page==Page::detail)open(selected);else fetch_list();}
        api.pump(now);if(api.done){api.done=false;complete();}
        if(page==Page::login&&login.valid){if(now>=login.expires){login.valid=false;std::snprintf(error,sizeof(error),"二维码已过期，请刷新");dirty=true;}
            else if(!api.active&&now>=login.next_poll){char url[180];std::snprintf(url,sizeof(url),"https://passport.bilibili.com/x/passport-login/web/qrcode/poll?qrcode_key=%s",login.key);login.next_poll=now+2500;get(url,Job::poll);}}
        covers.pump(now,network,!api.active&&page!=Page::login);
        if(covers.changed){covers.changed=false;dirty=true;}
        if(info_until && now>info_until){info_until=0;dirty=true;}
    }
    void suspend(){if(api.active)deferred_fetch=true;cancel_api();covers.stop();save_pump();dirty=true;}
    bool busy() const{return api.active;}
    bool search_input() const{return !in_player && page==Page::search;}
    void open(const Video& video) {
        if(page!=Page::detail&&!push_page())return;
        if(std::strcmp(selected.bvid,video.bvid))parts.clear();
        selected=video;page=Page::detail;scroll=0;dirty=true;
        char url[160];std::snprintf(url,sizeof(url),"https://api.bilibili.com/x/web-interface/view?bvid=%s",selected.bvid);get(url,Job::view);
    }
    bool apply_view_data(media::json::Value root){Video video;if(!parse_video(root.get("data"),video))return false;auto wanted=selected.cid;parts=video_parts(root.get("data"),video);
        selected=video;for(const auto& part:parts)if(part.cid==wanted&&!std::strcmp(part.bvid,video.bvid)){selected.cid=part.cid;selected.duration=part.duration;if(parts.size()>1)std::snprintf(selected.title,sizeof(selected.title),"%s",part.title);break;}return true;}
    void apply_view(media::json::Value root){apply_view_data(root);}
    std::string cookie() const{return account.cookie();}
    void record(uint64_t position_us,bool finished=false) {
        if(!video_id(selected.bvid))return;
        uint32_t seconds=finished?selected.duration:uint32_t(std::min<uint64_t>(UINT32_MAX,position_us/1000000));
        history.put(selected,seconds);history_dirty=true;
    }
    void begin_player(){in_player=true;cancel_api();covers.stop();std::vector<uint8_t>().swap(api.data);dirty=true;}
    void leave_player(){in_player=false;play_request=false;page=Page::detail;dirty=true;}
    bool back() {
        logout_confirm=false;cancel_api();if(page==Page::login)login={};
        if(!trail.empty()){auto state=std::move(trail.back());trail.pop_back();page=state.page;scroll=state.scroll;selected=state.selected;section_context=state.context;
            section=state.section;section_page=state.number;folder_id=state.folder;cursors=std::move(state.cursors);std::snprintf(error,sizeof(error),"%s",state.error.c_str());
            if(page==Page::section)section_data=std::move(state.data);if(page==Page::detail)parts=std::move(state.parts);dirty=true;return true;}
        if(page!=Page::home){page=Page::home;section=home_section;scroll=0;error[0]=0;dirty=true;return true;}return false;
    }
    void click(unsigned id) {
        if(in_player)return;
        if(id>=120&&id<=122){home_section=id==120?Section::popular:id==121?Section::recommend:Section::dynamic;section=home_section;popular.clear();home_data.clear();popular_page=home_page=1;home_cursors={std::string{}};scroll=0;fetch_list();dirty=true;}
        else if(id==130){if(page!=Page::login&&!push_page())return;cancel_api();page=Page::login;scroll=0;login_start();dirty=true;}
        else if(id==131){if(!logout_confirm){logout_confirm=true;dirty=true;}else{clear_account();account_loaded=dirty=true;notice("已退出登录");}}
        else if(id>=132&&id<=136){const Section types[]={Section::cloud_history,Section::folders,Section::watch_later,Section::followings,Section::dynamic};enter_section(types[id-132]);}
        else if(id>=140&&id<=143){if(api.active&&job==Job::view){notice("正在读取视频信息…");return;}
            if((id==141&&!selected.aid)||(id==142&&!selected.mid)){notice("请先刷新视频详情");return;}const Section types[]={Section::parts,Section::comments,Section::author,Section::related};enter_section(types[id-140]);}
        else if(id>=400&&id<528&&id-400<section_data.videos.size()){auto v=section_data.videos[id-400];open(v);}
        else if(id>=600&&id<700&&id-600<section_data.entries.size()){
            auto entry=section_data.entries[id-600];if(section==Section::folders){if(!push_page())return;folder_id=entry.id;section=Section::favorites;section_page=1;section_data.clear();scroll=0;fetch_section();}
            else if(section==Section::followings){if(!push_page())return;section_context={};section_context.mid=entry.id;std::snprintf(section_context.author,sizeof(section_context.author),"%s",entry.title);section=Section::author;section_page=1;section_data.clear();scroll=0;fetch_section();}dirty=true;}
        else if(id==100){trail.clear();cancel_api();page=Page::search;scroll=0;error[0]=0;dirty=true;}
        else if(id==101){trail.clear();cancel_api();page=Page::home;section=home_section;scroll=0;error[0]=0;if(popular.empty())fetch_list();dirty=true;}
        else if(id==102){trail.clear();cancel_api();page=Page::library;scroll=0;error[0]=0;dirty=true;}
        else if(id==103)back();
        else if(id==104){if(!permission)want_network=true;else if(page==Page::detail)open(selected);else if(mixin.empty()&&page!=Page::login)load_nav();else fetch_list();}
        else if(id==105||id==106){bool cursor=section==Section::dynamic||section==Section::cloud_history;
            if(page==Page::home&&home_section==Section::dynamic){
                if(id==106){if(!home_data.more||home_cursors.size()>=64){notice("没有更多动态，请刷新后重试");return;}home_cursors.push_back(home_data.next);++home_page;}
                else if(home_page>1){--home_page;home_cursors.pop_back();}}
            else if(page==Page::section){
                if(id==106){if(!section_data.more){notice("已经是最后一页");return;}if(cursor){if(cursors.size()>=64){notice("请刷新后继续浏览");return;}cursors.push_back(section_data.next);}++section_page;}
                else {if(section_page<=1)return;--section_page;if(cursor&&cursors.size()>1)cursors.pop_back();}}
            else {auto& p=page==Page::search?search_page:popular_page;if(id==106)++p;else if(p>1)--p;}
            scroll=0;fetch_list();}
        else if(id==107||id==108){library_saved=id==108;scroll=0;dirty=true;}
        else if(id==109){
            if(saved_loading)notice("正在读取稍后再看…");
            else {if(saved.erase(selected.bvid))notice("已从稍后再看移除");else{saved.put(selected);notice("已加入稍后再看");}saved_dirty=dirty=true;}}
        else if(id==110){play_request=true;auto *r=history.find(selected.bvid);
            resume_us=r && (!r->video.cid||r->video.cid==selected.cid) && r->position>5 && r->position+5<selected.duration?uint64_t(r->position)*1000000:0;
            if(!permission)want_network=true;}
        else if(id>=112 && id<=114){quality=id==112?16:id==113?32:64;dirty=true;}
        else if(id==115){search_page=1;scroll=0;fetch_list();}
        else if(id==170){searches.terms.clear();searches_dirty=dirty=true;}
        else if(id>=180&&id<188&&id-180<searches.terms.size()){
            std::snprintf(query,sizeof(query),"%s",searches.terms[id-180].c_str());input_changed=true;search_page=1;scroll=0;fetch_list();}

        else if(id>=200 && id<224){const auto& videos=page==Page::search?results:popular;if(id-200<videos.size())open(videos[id-200]);}
        else if(id>=300 && id<306){const auto& list=library_saved?saved:history;if(id-300<list.records.size())open(list.records[id-300].video);}
    }
    void pointer(media::Canvas& c,const pxa_ui_pointer_data_t& p) {
        int x=p.x-c.viewport.content.x,y=p.y-c.viewport.content.y;
        if(p.phase==PXA_POINTER_DOWN){pointer_down=true;scrolled=false;down_y=y;down_scroll=scroll;}
        else if(pointer_down && p.phase==PXA_POINTER_MOVE && down_y>=(page==Page::detail?52:page==Page::section?92:page==Page::library?52:(c.h<380?88:103)) && down_y<c.h-(page==Page::detail?66:55)) {
            if(std::abs(y-down_y)>6)scrolled=true;
            if(scrolled){scroll=std::clamp(down_scroll+down_y-y,0,max_scroll);dirty=true;}}
        else if(p.phase==PXA_POINTER_UP){if(pointer_down && !scrolled)click(c.find(x,y));pointer_down=false;}
        else if(p.phase==PXA_POINTER_CANCEL)pointer_down=false;
    }
    void render(media::Canvas& c,bool loading,const char *player_message) {
        covers.begin();max_scroll=0;int w=c.w,h=c.h;
        if(page==Page::login) {
            std::memcpy(qr_commands,c.frame.data,c.frame.length);c.frame.data=qr_commands;c.frame.capacity=sizeof(qr_commands);
            ui::icon(c,"back",23,25,ink);c.hit(103,4,3,42,44);ui::label(c,48,3,w-60,44,"扫码登录",ink,0,1);
            bool wide=w>h*1.25;int area=std::min(wide?w/2-24:w-36,h-(wide?72:154));
            int n=login.valid?qrcodegen_getSize(login.pixels):41;int module=std::clamp(area/(n+8),1,3),size=(n+8)*module;
            int x=wide?std::max(12,(w/2-size)/2):(w-size)/2,y=wide?54:56;
            if(login.valid){
                // One bitmap avoids seams between adjacent rectangles at fractional density.
                auto *pixels=pxa_canvas_bitmap_rgb565_reserve(&c.frame,x+c.viewport.content.x,y+c.viewport.content.y,size,size,size*2);
                if(pixels)for(int row=0;row<size;++row)for(int col=0;col<size;++col){uint8_t value=qrcodegen_getModule(login.pixels,col/module-4,row/module-4)?0:255;
                    size_t at=(size_t(row)*size+col)*2;pixels[at]=pixels[at+1]=value;}}
            else{c.rect(x,y,size,size,ui::colors.surface,8);ui::icon(c,"refresh",x+size/2,y+size/2,muted,36);}
            int tx=wide?w/2:12,tw=wide?w/2-12:w-24,ty=wide?70:y+size+10;
            auto login_lines=lines(login.scanned?"已扫码，请在手机确认":"使用哔哩哔哩 App 扫码",tw,2);
            for(auto& row:login_lines){ui::label(c,tx,ty,tw,19,row.c_str(),ink,1);ty+=19;}ty+=12;
            if(*error){text(c,tx,ty,tw,error,2,ui::colors.danger);ty+=42;}else if(!login.valid){ui::label(c,tx,ty,tw,24,api.active?"正在获取二维码…":"点击刷新重新获取",muted,1);ty+=30;}
            else{ui::label(c,tx,ty,tw,22,"仅在本机保存登录状态",muted,1);ty+=30;}
            ui::pill(c,104,tx+(tw-110)/2,std::min(h-42,ty),110,"刷新二维码",false,pink,muted,ui::colors.surface);
        } else if(page==Page::section) {
            ui::icon(c,"back",23,25,ink);c.hit(103,4,3,42,44);ui::label(c,48,3,w-98,44,section_title(section),ink,0,1);ui::icon(c,"refresh",w-24,25,pink,20);c.hit(104,w-46,3,42,44);
            const bool paged=section!=Section::parts&&section!=Section::related&&section!=Section::watch_later;
            if(paged){char label[40];std::snprintf(label,sizeof(label),"第 %u 页",section_page);ui::label(c,12,52,w-112,30,label,muted);
                ui::icon(c,"back",w-76,67,section_page>1?ink:muted,20);c.hit(105,w-98,47,44,40);ui::icon(c,"next",w-28,67,section_data.more?pink:muted,20);c.hit(106,w-50,47,44,40);}
            else ui::label(c,12,52,w-24,30,section==Section::parts?"选择要播放的分 P / 合集视频":section_context.author,muted);
            int top=92,bottom=h-12;
            if(*error){text(c,20,top+14,w-40,error,3,muted);ui::pill(c,needs_account(section)&&!account.logged_in()?130:104,w/2-50,top+84,100,needs_account(section)&&!account.logged_in()?"扫码登录":"重试",true,pink,ink,ui::colors.surface);}
            else if(api.active&&section_data.videos.empty()&&section_data.entries.empty())ui::label(c,12,top+30,w-24,24,"正在加载…",muted,1);
            else if(section==Section::parts||!section_data.entries.empty()){
                int content=0;if(section==Section::parts)content=int(section_data.videos.size())*66;
                else for(const auto& entry:section_data.entries)content+=*entry.body?62+int(lines(entry.body,w-48,12).size())*19:70;
                max_scroll=std::max(0,content-(bottom-top));scroll=std::clamp(scroll,0,max_scroll);int y=top-scroll;c.clip(0,top,w,bottom-top);
                unsigned index=0;if(section==Section::parts){for(const auto& video:section_data.videos){if(y+62>=top&&y<bottom){c.rect(12,y,w-24,58,ui::colors.surface,8);
                    text(c,22,y+7,w-44,video.title,2);char time[32],duration[64];duration_text(video.duration,time,sizeof(time));std::snprintf(duration,sizeof(duration),"P%u · %s",index+1,time);
                    ui::label(c,22,y+39,w-44,18,duration,muted);c.hit(400+index,12,std::max(top,y),w-24,std::min(bottom,y+58)-std::max(top,y));}y+=66;++index;}}
                else for(const auto& entry:section_data.entries){int height=*entry.body?62+int(lines(entry.body,w-48,12).size())*19:70;
                    if(y+height>=top&&y<bottom){c.rect(12,y,w-24,height-8,ui::colors.surface,8);auto title=lines(entry.title,w-48,1);
                        if(!title.empty())ui::label(c,24,y+5,w-48,24,title[0].c_str(),ink);
                        if(*entry.body)text(c,24,y+33,w-48,entry.body,12);ui::label(c,24,y+height-29,w-48,20,entry.subtitle,muted);
                        if(section==Section::folders||section==Section::followings)c.hit(600+index,12,std::max(top,y),w-24,std::min(bottom,y+height-8)-std::max(top,y));}y+=height;++index;}
                c.unclip();
            }else if(!section_data.videos.empty())cards(c,section_data.videos,top,bottom,400);
            else ui::label(c,12,top+30,w-24,24,section==Section::comments?"暂无可展示的评论":"暂时没有内容",muted,1);
        } else if(page==Page::detail) {
            c.rect(0,0,w,h,ui::colors.surface);
            auto layout=ui::detail_layout(w,h);int bottom=h-58;
            int title_height=int(lines(selected.title,layout.info_w,2).size())*19;
            int info_height=title_height+41+29+42+80+(*error?62:! *selected.description?0:66);
            int content_end=layout.info_y+info_height;
            max_scroll=std::max(0,content_end-layout.body_bottom);scroll=std::clamp(scroll,0,max_scroll);
            c.clip(0,50,w,std::max(1,bottom-50));
            cover(c,selected,layout.cover_x,layout.cover_y-(layout.split?0:scroll),layout.cover_w,layout.cover_h);
            int x=layout.info_x,y=layout.info_y-scroll,iw=layout.info_w;
            c.clip(layout.split?x:0,50,layout.split?iw:w,std::max(1,bottom-50));
            y+=text(c,x,y,iw,selected.title,2);
            c.circle(x+12,y+19,12,ui::colors.selection);ui::label(c,x,y+7,24,24,"UP",pink,1);
            ui::label(c,x+34,y+7,iw-34,24,selected.author,ink);if(selected.mid)c.hit(142,x,std::max(50,y+7),iw,std::max(0,std::min(bottom,y+31)-std::max(50,y+7)));y+=41;
            char views[32],dm[32],stats[96];count_text(selected.views,views,sizeof(views));count_text(selected.danmaku,dm,sizeof(dm));
            std::snprintf(stats,sizeof(stats),"%s 播放  ·  %s 弹幕",views,dm);ui::label(c,x,y,iw,22,stats,muted);y+=29;
            const unsigned qualities[]={16,32,64};const char *labels[]={"360p","480p","720p"};
            int chip=std::min(66,(iw-16)/3),qx=x;
            for(unsigned i=0;i<3;++i){if(i==1 && max_pixels<854*480)continue;if(i==2 && max_pixels<1280*720)continue;
                ui::pill(c,112+i,qx,y,chip,labels[i],quality==qualities[i],pink,muted,ui::colors.fill);qx+=chip+8;}y+=42;
            int bw=(iw-8)/2;const char *actions[]={"选集","评论","作者作品","相关推荐"};
            for(unsigned i=0;i<4;++i){int ax=x+int(i%2)*(bw+8),ay=y+int(i/2)*36;
                ui::pill(c,140+i,ax,ay,bw,actions[i],false,pink,muted,ui::colors.fill);}y+=80;
            if(*error){text(c,x,y,iw-50,error,2,ui::colors.danger);ui::label(c,x+iw-44,y,44,30,"重试",pink,1);c.hit(104,x+iw-48,std::max(50,y),48,32);}
            else if(*selected.description)text(c,x,y,iw,selected.description,3,muted);
            c.unclip();c.unclip();
            for(unsigned i=0;i<c.count;++i){auto& hit=c.hits[i];int end=std::min(bottom,hit.y+hit.h);hit.y=std::max(50,hit.y);hit.h=std::max(0,end-hit.y);}
            c.rect(0,0,w,50,ui::colors.surface);ui::icon(c,"back",23,25,ink);c.hit(103,4,3,42,44);
            ui::label(c,48,3,w-104,44,"视频详情",ink,0,1);
            ui::icon(c,saved.find(selected.bvid)?"bookmark-filled":"bookmark",w-25,25,saved.find(selected.bvid)?pink:muted);c.hit(109,w-48,3,44,44);
            c.rect(0,bottom,w,58,ui::colors.surface);c.line(0,bottom,w,bottom,ui::colors.border,1);
            c.rect(12,bottom+9,w-24,40,pink,20);
            auto *r=history.find(selected.bvid);bool resume=r&&(!r->video.cid||r->video.cid==selected.cid)&&r->position>5&&r->position+5<selected.duration;
            ui::icon(c,"play",34,bottom+29,ui::colors.on_primary,20);
            ui::label(c,56,bottom+9,w-112,40,loading?"正在准备…":resume?"继续播放":"立即播放",ui::colors.on_primary,1);
            if(!loading)c.hit(110,12,bottom+6,w-24,46);
            if(loading){c.rect(0,bottom-25,w,25,ui::colors.surface);ui::label(c,14,bottom-25,w-28,25,player_message,muted,1);}
            if(max_scroll>0){int track=bottom-62,thumb=std::max(16,track*track/(track+max_scroll));
                c.rect(w-4,56+scroll*(track-thumb)/max_scroll,2,thumb,0xd5d9e0,1);}

        } else {
            if(page==Page::home) {
                ui::label(c,12,10,60,36,"billy",pink,0,1);c.rect(76,10,w-88,36,ui::colors.surface,18);ui::icon(c,"search",96,28,muted,20);
                ui::label(c,112,10,w-132,36,"搜索视频 / BV号",muted);c.hit(100,76,8,w-88,42);
            } else if(page==Page::search) {
                ui::icon(c,"back",20,27,ink,22);c.hit(103,3,5,36,42);c.rect(41,10,w-101,36,ui::colors.surface,16);
                ui::label(c,w-59,8,56,38,"搜索",pink,1);c.hit(115,w-59,5,56,42);
            } else ui::label(c,14,10,w-28,36,"我的",ink,0,1);
            if(page==Page::library) {
                auto& list=library_saved?saved:history;int top=52,bottom=h-55,columns=w>=440?5:3,rows=(5+columns-1)/columns;
                int content=112+rows*36+(list.records.empty()?56:int(list.records.size())*76);unsigned first_hit=c.count;
                max_scroll=std::max(0,content-(bottom-top));scroll=std::clamp(scroll,0,max_scroll);int y=top-scroll;c.clip(0,top,w,bottom-top);
                c.rect(12,y,w-24,48,ui::colors.surface,8);ui::icon(c,"person",34,y+24,pink,24);
                auto names=lines(account.logged_in()?(*account.name?account.name:"已登录"):"登录哔哩哔哩",w-156,1);
                if(!names.empty())ui::label(c,56,y,w-156,48,names[0].c_str(),ink);
                ui::label(c,w-92,y,76,48,account.logged_in()?(logout_confirm?"确认退出":"退出登录"):"扫码登录",pink,1);c.hit(account.logged_in()?131:130,w-96,std::max(top,y),80,std::max(0,std::min(bottom,y+48)-std::max(top,y)));y+=58;
                const char *names2[]={"云端历史","我的收藏","稍后再看","我的关注","关注动态"};
                int bw=(w-24-(columns-1)*6)/columns;
                for(unsigned i=0;i<5;++i){int by=y+int(i/columns)*36;ui::pill(c,132+i,12+int(i%columns)*(bw+6),by,bw,names2[i],false,pink,muted,ui::colors.surface);}y+=rows*36+12;
                ui::pill(c,107,12,y,(w-34)/2,"本地记录",!library_saved,pink,muted,ui::colors.surface);
                ui::pill(c,108,22+(w-34)/2,y,(w-34)/2,"本地稍后看",library_saved,pink,muted,ui::colors.surface);y+=42;
                if(list.records.empty())ui::label(c,12,y+12,w-24,32,library_saved?"还没有稍后再看的视频":"还没有观看记录",muted,1);
                for(unsigned i=0;i<list.records.size();++i){if(y+70>=top&&y<bottom){const auto& v=list.records[i].video;c.rect(12,y,w-24,68,ui::colors.surface,8);cover(c,v,12,y,100,56);
                    text(c,122,y+4,w-146,v.title,2);ui::label(c,122,y+44,w-146,20,v.author,muted);c.hit(300+i,12,std::max(top,y),w-24,std::min(bottom,y+68)-std::max(top,y));}y+=76;}
                for(unsigned i=first_hit;i<c.count;++i){auto& hit=c.hits[i];int end=std::min(bottom,hit.y+hit.h);hit.y=std::max(top,hit.y);hit.h=std::max(0,end-hit.y);}c.unclip();
            } else {
                int row=h<380?56:66,top=h<380?88:103;
                if(page==Page::home){const char *tabs[]={"热门","推荐","动态"};for(unsigned i=0;i<3;++i)ui::pill(c,120+i,12+int(i)*58,row-5,52,tabs[i],home_section==(i==0?Section::popular:i==1?Section::recommend:Section::dynamic),pink,muted,ui::colors.surface);}
                else ui::label(c,12,row-5,w-136,28,*query?"搜索结果":searches.terms.empty()?"搜索视频":"最近搜索",ink,0,1);
                unsigned p=page==Page::home?(home_section==Section::dynamic?home_page:popular_page):search_page;char number[24];std::snprintf(number,sizeof(number),"%u",p);
                if(page==Page::home||*query){
                ui::icon(c,"back",w-90,row+8,muted,18);c.hit(105,w-108,row-10,36,36);ui::label(c,w-76,row-6,22,28,number,muted,1);
                ui::icon(c,"next",w-38,row+8,muted,18);c.hit(106,w-56,row-10,36,36);ui::icon(c,"refresh",w-15,row+8,pink,18);c.hit(104,w-32,row-10,32,36);
                }else if(!searches.terms.empty()){ui::label(c,w-64,row-6,52,28,"清空",muted,1);c.hit(170,w-70,row-10,64,36);}

                const auto& videos=page==Page::home?popular:results;
                if(*error&&!(page==Page::search&&!*query)){text(c,20,top+20,w-40,error,2,muted);ui::pill(c,page==Page::home&&home_section==Section::dynamic&&!account.logged_in()?130:104,w/2-50,top+74,100,page==Page::home&&home_section==Section::dynamic&&!account.logged_in()?"扫码登录":"重试",true,pink,ink,ui::colors.surface);}
                else if(api.active && videos.empty()&&!(page==Page::search&&!*query)){ui::label(c,20,top+36,w-40,24,"正在加载…",muted,1);}
                else if(page==Page::search&&!*query&&!searches.terms.empty()){
                    int bottom=h-65;max_scroll=std::max(0,int(searches.terms.size())*44-(bottom-top));scroll=std::clamp(scroll,0,max_scroll);
                    c.clip(0,top,w,bottom-top);
                    for(unsigned i=0;i<searches.terms.size();++i){int y=top+int(i)*44-scroll;if(y+40<top||y>=bottom)continue;
                        c.rect(12,y,w-24,38,ui::colors.surface,8);ui::icon(c,"clock",28,y+19,muted,18);
                        auto rows=lines(searches.terms[i].c_str(),w-74,1);if(!rows.empty())ui::label(c,46,y,w-68,38,rows[0].c_str(),ink);
                        c.hit(180+i,12,std::max(top,y),w-24,std::min(bottom,y+38)-std::max(top,y));}
                    c.unclip();
                }else if(videos.empty()||(page==Page::search&&!*query)){
                    int y=top+std::max(24,(h-65-top)/2-30);ui::icon(c,"search",w/2,y,muted,36);
                    ui::label(c,20,y+28,w-40,24,page==Page::search&&!*query?"输入关键词或 BV 号":"暂时没有视频",muted,1);

                } else cards(c,videos,top,h-65,200);
            }
            nav(c);
        }
        if(info_until>now){c.rect(16,h-102,w-32,32,ink,12);ui::label(c,24,h-102,w-48,32,info,ui::colors.surface,1);}
        covers.prune();dirty=false;
    }
};
}
