// SPDX-License-Identifier: MIT
#include <pxa/app.hpp>
#include <pxa/ui_environment.hpp>
#include <pxa/ui_layout.hpp>
#include "core.hpp"
#include "reader.hpp"
#include "content.inc"
#include <chrono>

using namespace pxa;
using namespace pxa::ui;
using namespace pxa::ui::literals;
namespace {
constexpr study::Kind kind=study::Kind(STUDY_KIND);
constexpr const char* titles[]={"口算闯关","诗词小馆","单词记忆卡","汉字练习册","知识卡盒"};
constexpr const char* descriptions[]={
    "每天十道题，练习计算与思考。",
    "从一句诗开始，把喜欢的诗记下来。",
    "先回忆再翻卡，少量多次记单词。",
    "结合词语学读音，按单字查看笔顺。",
    "自然、科学、人文与生活，随身复习。"};
constexpr unsigned K=STUDY_KIND;
constexpr unsigned AGE=STUDY_AGE;
static_assert(K<5&&AGE<4);
constexpr const char* age_names[]={"亲子启蒙","小学","中学","成人"};
constexpr const char* hero_titles[]={"小芽，出发！","每天有新发现","让好奇心走远","学一点新知"};
constexpr const char* hero_lines[]={"和爸妈一起，\n收集小星星。","十道小挑战，\n装满进步口袋。","从记住到理解，\n每次进步一点。","短时练习，\n随时继续。"};
constexpr uint32_t accents[]={0x167d66ff,0xb75d48ff,0x376db4ff,0xae4966ff,0x7255b0ff};
constexpr uint32_t papers[]={0xf3f7edff,0xfff5eaff,0xeff6ffff,0xfff2f4ff,0xf4f0ffff};
constexpr uint32_t tints[]={0xdcefe1ff,0xf9e1cfff,0xdce9fbff,0xf5dce5ff,0xe7def8ff};
constexpr uint32_t ink=0x263b4aff,muted=0x607585ff,white=0xffffffff;
template<class Label> auto text(Label&& value,Font font=Font::body,uint32_t color=ink) {
    return Text(std::forward<Label>(value)).font(font).rgba(color);
}
// Native buttons contain a text child. Constrain that child to the button
// width so long poetry answers wrap rather than extending beyond the screen.
template<class Child> struct WrappedButton : ViewModifiers {
    Child child;
    static constexpr Capacity capacity=capacity_of<Child>;
    static constexpr uint32_t text_node_offset=1;
    explicit WrappedButton(Child value):child(std::move(value)) {}
    template<class Page> bool render(Page& page,uint32_t parent) {
        return page.decorate(child,parent,[](auto& owner,uint32_t root) {
            return owner.transaction().fill(root+1,protocol::width)&&
                owner.transaction().u8(root+1,protocol::text_align,1);
        });
    }
};
template<class Label,class Fn> auto button(Label&& value,Fn&& fn,bool primary=false) {
    return WrappedButton(Button(std::forward<Label>(value)).on_click(std::forward<Fn>(fn))).font(Font::title).padding(4_dp).align(Align::center).justify(Justify::center)
        .background(primary?accents[K]:tints[K]).rgba(primary?white:ink)
        .border_width(0_dp).radius(12_dp);
}
// Explicitly select vertical scrolling; the SDK's generic Scroll factory uses
// the protocol's horizontal axis on the current 2.1 baseline.
template<class Child> struct VerticalScroll : ViewModifiers {
    Child child;
    static constexpr Capacity capacity=container_capacity<Child>();
    explicit VerticalScroll(Child value):child(std::move(value)) {}
    template<class Page> bool render(Page& page,uint32_t parent) {
        auto node=page.create(parent,3);
        return node&&page.transaction().fill(node,protocol::width)&&
            page.transaction().fill(node,protocol::height)&&
            page.transaction().u8(node,protocol::scroll_axis,2)&&
            page.transaction().u64(node,protocol::event_mask,UINT64_C(1)<<3)&&child.render(page,node);
    }
};
template<class Child> auto VScroll(Child child) {return VerticalScroll<Child>(std::move(child));}
// Illustrations are decorative. Keep them out of the Host's key focus order
// and let touch drags reach the containing scroll view.
template<class Child> struct Illustration : ViewModifiers {
    Child child;
    static constexpr Capacity capacity=capacity_of<Child>;
    explicit Illustration(Child value):child(std::move(value)) {}
    template<class Page> bool render(Page& page,uint32_t parent) {
        return page.decorate(child,parent,[](auto& owner,uint32_t root) {
            return owner.transaction().u64(root,protocol::event_mask,0);
        });
    }
};
auto picture(CanvasRef& ref,int width,int height) {return Illustration(Canvas(ref,width,height));}
struct StudyApp {
    Environment<> environment;
    Context* ctx=nullptr;
    study::Progress progress;
    std::vector<study::Item> bank=builtins();
    std::vector<unsigned> session;
    study::Question current;
    unsigned position=0, score=0, card_index=0;
    uint32_t math_seed=0;
    uint16_t day=0;
    bool reviewing=false, loading=true, save_running=false, revealed=false, custom=false, finished=false;
    bool from_cache=false;
    unsigned active_page=0, reader_from=0, reader_index=0;
    std::string full_detail;
    std::vector<std::string> reader_pages;
    unsigned revision=0, saved_revision=0;
    State<bool> ready{false}, home{true}, quiz{false}, result{false}, cards{false}, settings{false}, reading{false};
    State<bool> busy{false}, can_online{true};
    State<bool> has_strokes{false}, has_notice{false}, reader_prev{false}, reader_next{false};
    State<std::string> back_label{"退出"}, reading_text{""}, reading_counter{""};
    State<int> session_progress{0};
    State<std::string> stars{""};
    State<std::string> subtitle{descriptions[K]}, summary{"加载学习记录…"};
    State<std::string> prompt{""}, detail{""}, counter{""}, feedback{""}, status{""};
    State<std::string> a{""},b{""},c{""},d{""}, reveal_label{"展开答案"};
    State<std::string> online_label{K==1?"每日诗句":K==2?"查小词典":"看看笔顺"};
    State<std::string> url{""}, course_note{""};
    std::array<std::byte,study::save_size> load_bytes{};
    std::array<std::byte,study::save_size+96> save_packet{};
    std::array<std::byte,1024> network_chunk{};
    std::vector<study::Stroke> strokes;
    unsigned stroke_step=0;
    CanvasRef stroke_canvas;
    CanvasRef hero_canvas,quantity_canvas;
    CanvasCommands<2800> illustration;
    CanvasCommands<4000> drawing;

    void set_status(std::string value) {status.set(std::move(value));has_notice.set(!status.get().empty());}
    void page(unsigned n) {
        set_status("");
        active_page=n;home.set(n==0);quiz.set(n==1);result.set(n==2);cards.set(n==3);settings.set(n==4);reading.set(n==5);
        back_label.set(n==0?"退出":"返回");
        if(n!=3&&n!=5){strokes.clear();has_strokes.set(false);}
        if(n==0)refresh_summary();
        if(n==0)draw_hero();
    }
    void set_detail(std::string value) {full_detail=std::move(value);detail.set(full_detail);}
    void reader_update() {
        reading_text.set(reader_pages[reader_index]);
        reading_counter.set("阅读 "+std::to_string(reader_index+1)+" / "+std::to_string(reader_pages.size()));
        reader_prev.set(reader_index>0);reader_next.set(reader_index+1<reader_pages.size());
    }
    void open_reader(std::string value) {
        reader_from=active_page;reader_index=0;
        const auto& display=environment.display().get();
        const auto density=std::max(1u,display.density_q16);
        const unsigned width=unsigned(uint64_t(display.width)*65536/density);
        const unsigned height=unsigned(uint64_t(display.height)*65536/density);
        reader_pages=study::reading_pages(value,width>48?(width-48)/10:12,height>160?(height-160)/26:3);
        reader_update();page(5);
    }
    void read_detail() {open_reader(prompt.get()+"\n\n"+full_detail);}
    void read_help() {
        open_reader(std::string(titles[K])+" · "+age_names[AGE]+"\n\n"+
            "三键操作：上、下逐项选择，确认执行。页面右上角可返回；首页右上角可退出应用。触摸设备也可直接点击。\n\n"
            "每轮最多10题。答错后看解释，再从首页复习。翻卡先回忆、展开核对，再选择记住或再学。长解释可点“分页阅读”。\n\n"
            "学习记录自动保存在内部存储，不需要SD卡。复习间隔为1、3、7、14、30、60天。系统日期未校准时仍能复习错题。\n\n"+
            (K==0?"全部计算题本地生成，保留最近8道错题，答对后移除。":
             K==1?"每日诗句为随机在线内容，不属于分级练习。内置古诗文正文为公版作品。":
             K==2?"小词典按需联网，显示英文释义、音标、来源与许可；首版不播放发音。":
             K==3?"笔顺按需联网并缓存，显示笔画中线，可用“下一笔”逐步观察，不提供书写评分。":
                   "知识卡盒支持自己提供的HTTPS课程，一次4至24张，8KB以内，年龄必须与本安装包一致。")+
            "\n\n联网需要系统Wi-Fi已开启并授权。最多8项内部缓存，失败时保留已有内容。清理缓存不会删除学习记录或已下载课程。");
    }
    void refresh_summary() {
        const auto& s=progress.stats[progress.level]; unsigned count=0,wrong=0,due=0;
        for(const auto& i:bank)if(i.level==progress.level) {
            ++count;const auto* r=progress.find(i.id);
            if(r&&r->wrong)++wrong;
            if(r&&day&&r->due<=day)++due;
        }
        if constexpr(K==0)for(auto seed:progress.math_mistakes[progress.level])wrong+=seed!=0;
        char text[200];std::snprintf(text,sizeof(text),"练习 %u 题 · 错题 %u · 到期 %u",s.answered,wrong,due);
        summary.set(text);
        stars.set(AGE<3?"已收集 "+std::to_string(s.correct)+" 颗小星星":"正确率 "+std::to_string(s.answered?uint64_t(s.correct)*100/s.answered:0)+"%");
        course_note.set(custom?"正在使用下载的课程。恢复内置内容会保留记录。":"使用内置精选内容。下载的课程难度应为 "+std::to_string(AGE)+"（"+age_names[AGE]+"）。");
    }
    void persist() {
        ++revision;
        if(loading||save_running||!ctx)return;
        save_running=true;
        if(!ctx->tasks().start(save_loop())) {save_running=false;set_status("保存任务未启动，请返回后重试。");}
    }
    Task<void> save_loop() {
        while(saved_revision!=revision) {
            unsigned writing=revision;
            auto bytes=study::encode(progress,kind);
            auto saved=co_await ctx->storage().set("progress-v2",std::as_bytes(std::span{bytes}),save_packet);
            if(!saved) {set_status("保存失败，学习记录暂留在本次会话。");break;}
            saved_revision=writing;
        }
        save_running=false;
        co_return Result<void>{};
    }
    Task<void> load() {
        progress.level=AGE;
        auto value=co_await ctx->storage().get("progress-v2",load_bytes);
        if(value&&!study::decode(progress,kind,reinterpret_cast<const uint8_t*>(load_bytes.data()),*value))
            set_status("旧记录无效，已使用新的学习记录。");
        else if(!value&&value.error()!=Error::not_found)set_status("读取学习记录失败，暂用新记录。");
        auto time=co_await ctx->clock().wall_time();
        if(time) {
            auto days=std::chrono::duration_cast<std::chrono::hours>(time->local().time_since_epoch()).count()/24;
            if(days>18262&&days<65000)day=uint16_t(days);
        }
        if constexpr(K==4) {
            auto file=co_await ctx->fs().open("course.json");
            if(file) {
                std::string bytes;std::array<std::byte,512> part;
                for(;;) {auto n=file->read(part);if(!n||!*n)break;bytes.append(reinterpret_cast<char*>(part.data()),*n);if(bytes.size()>8192)break;}
                std::vector<study::Item> imported;
                if(study::parse_course(bytes,imported)&&std::all_of(imported.begin(),imported.end(),[](const auto& item){return item.level==AGE;})) {bank=std::move(imported);custom=true;}
            }
        }
        progress.level=AGE;
        loading=false;ready.set(true);refresh_summary();
        (void)ctx->window().fullscreen(WindowBarMode::hidden,WindowBarMode::hidden);
        draw_hero();
        if(K!=0&&!day&&status.get().empty())set_status("离线可用 · 日期未校准，可复习错题。");
        co_return Result<void>{};
    }
    Result<void> on_start(Context& context,std::span<const std::byte> config) {
        ctx=&context;auto init=environment.initialize(config);if(!init)return init;
        return ctx->tasks().start(load());
    }
    Result<bool> on_event(Context&,const Event& event) {return environment.update(event);}
    void on_foreground(Context& context) {
        (void)context.window().fullscreen(WindowBarMode::hidden,WindowBarMode::hidden);
        draw_strokes();
        draw_hero();
    }
    void on_error(Context&,Error) {busy.set(false);set_status("操作失败，请检查网络或内部存储后重试。");}
    BackAction on_back() {if(home.get())return BackAction::close;page(reading.get()?reader_from:0);return BackAction::stay;}
    void go_back() {if(home.get())(void)ctx->window().request_back();else page(reading.get()?reader_from:0);}
    void start(bool review) {
        if(busy.get())return;
        reviewing=review;position=score=0;
        if constexpr(K==0) {
            session.clear();
            if(review)for(unsigned i=0;i<8;++i)if(progress.math_mistakes[progress.level][i])session.push_back(i);
            if(!review)session.resize(10);
        } else session=study::selection(bank,progress,review,day);
        if(session.empty()) {set_status(review?"本档没有错题或到期卡片，试试新练习。":"这份课程在本档没有内容，请恢复内置课程。");return;}
        next_question();
    }
    void next_question() {
        if(position>=session.size()) {
            finished=true;
            prompt.set("本轮完成");
            set_detail("答对 "+std::to_string(score)+" / "+std::to_string(session.size())+" 题\n本轮已记录。错题可以在首页继续复习。");
            feedback.set("每次一点点，今天也有进步。");counter.set("练习小结");page(2);return;
        }
        finished=false;
        if constexpr(K==0) {
            math_seed=reviewing?progress.math_mistakes[progress.level][session[position]]:progress.seed;
            uint32_t seed=math_seed;current=study::math_question(AGE,seed);
            if(!reviewing)progress.seed=seed;
        } else current=study::choice(bank,session[position],progress.seed);
        prompt.set(current.item.question);set_detail("");feedback.set("");
        a.set(current.options[0]);b.set(current.options[1]);
        c.set(current.options[2]);d.set(current.options[3]);
        counter.set(std::string(reviewing?"复习 ":"练习 ")+std::to_string(position+1)+" / "+std::to_string(session.size()));
        session_progress.set(int(position*100/session.size()));
        page(1);
        draw_quantity();
    }
    void answer(unsigned index) {
        if(!quiz.get())return;
        bool correct=index==current.correct;
        progress.record(current.item.id,correct,day);
        if constexpr(K==0) {
            auto& mistakes=progress.math_mistakes[progress.level];
            auto old=std::find(mistakes.begin(),mistakes.end(),math_seed);
            if(correct) {if(old!=mistakes.end())*old=0;}
            else if(old==mistakes.end()) {
                auto free=std::find(mistakes.begin(),mistakes.end(),0);
                if(free==mistakes.end()) {std::rotate(mistakes.begin(),mistakes.begin()+1,mistakes.end());free=mistakes.end()-1;}
                *free=math_seed;
            }
        }
        if(correct)++score;
        feedback.set(AGE<3?(correct?"叮！收获一颗小星星":"差一点点，一起看看吧"):(correct?"回答正确":"核对一下解题思路"));
        set_detail("正确答案："+current.item.answer+"\n\n"+current.item.explanation);
        ++position;session_progress.set(int(position*100/session.size()));persist();page(2);
    }
    std::vector<unsigned> level_cards() const {
        std::vector<unsigned> ids;for(unsigned i=0;i<bank.size();++i)if(bank[i].level==progress.level)ids.push_back(i);return ids;
    }
    void open_cards() {
        if(busy.get())return;
        auto ids=level_cards();if(ids.empty()) {set_status("本档没有卡片，请恢复内置课程。");return;}
        card_index=progress.bookmark[progress.level]%ids.size();show_card();
    }
    void show_card() {
        auto ids=level_cards();if(ids.empty())return;
        card_index%=ids.size();current.item=bank[ids[card_index]];
        prompt.set(current.item.question);set_detail("先想一想，再展开答案。");
        counter.set("卡片 "+std::to_string(card_index+1)+" / "+std::to_string(ids.size()));
        revealed=false;reveal_label.set("展开答案");strokes.clear();has_strokes.set(false);
        progress.bookmark[progress.level]=card_index;page(3);draw_strokes();
    }
    void browse(int delta) {
        auto ids=level_cards();if(ids.empty())return;
        card_index=(int(card_index)+int(ids.size())+delta)%ids.size();show_card();persist();
    }
    void reveal() {
        revealed=!revealed;
        set_detail(revealed?current.item.answer+"\n\n"+current.item.explanation:"先想一想，再展开答案。");
        reveal_label.set(revealed?"遮住答案":"展开答案");
    }
    void rate(bool remembered) {
        if(!revealed){set_status("先展开答案核对，再记录掌握情况。");return;}
        progress.record(current.item.id,remembered,day);browse(1);
        set_status(remembered?"已记住，之后会安排复习。":"已加入错题，下次继续复习。");
    }
    void draw_strokes() {
        if constexpr(K==3) {
            if(!stroke_canvas.mounted())return;
            drawing.reset();drawing.rect(0,0,160,160,0xf8fafcff);
            drawing.line(0,80,160,80,0xcbd5e1ff,1).line(80,0,80,160,0xcbd5e1ff,1);
            for(unsigned i=0;i<strokes.size();++i) {
                const auto& stroke=strokes[i];
                const unsigned segments=std::min<unsigned>(stroke.size()-1,3);
                for(unsigned j=1;j<=segments;++j) {
                    const auto& before=stroke[(j-1)*(stroke.size()-1)/segments];
                    const auto& after=stroke[j*(stroke.size()-1)/segments];
                    auto x=[](int v){return std::clamp(10+v*140/1024,0,159);};
                    auto y=[](int v){return std::clamp(10+(900-v)*140/1024,0,159);};
                    drawing.line(x(before.x),y(before.y),x(after.x),y(after.y),
                        i<stroke_step?0x0d9488ff:0xd7dfe7ff,5);
                }
            }
            (void)stroke_canvas.present(drawing);
        }
    }
    void advance_stroke() {
        if(strokes.empty()) {set_status("先点击“看看笔顺”，或读取已有缓存。");return;}
        stroke_step=(stroke_step+1)%(strokes.size()+1);draw_strokes();
        set_status("笔顺中线示意：第 "+std::to_string(stroke_step)+" / "+std::to_string(strokes.size())+" 笔");
    }
    void draw_hero() {
        if(!hero_canvas.mounted())return;
        illustration.reset();illustration.rect(0,0,108,82,papers[K]);
        if constexpr(AGE==3) {
            illustration.rect(16,12,76,58,white,12).rect(24,20,27,42,tints[K],5).rect(55,20,27,42,tints[K],5);
            for(int y=30;y<58;y+=10)illustration.line(30,y,44,y,accents[K],2).line(61,y,76,y,accents[K],2);
        } else {
            illustration.rect(8,21,86,52,tints[K],24);
            illustration.rect(31,18,48,47,0xffe5b9ff,23);
            illustration.rect(42,34,5,7,ink,2).rect(63,34,5,7,ink,2);
            illustration.rect(35,45,9,4,0xf4a9a0ff,2).rect(66,45,9,4,0xf4a9a0ff,2);
            illustration.arc(55,44,8,30,150,0xb55b4aff,2);
            illustration.line(55,20,57,9,0x568b49ff,3).rect(39,4,18,9,0x8cbf69ff,5).rect(57,1,19,10,0x6b9d52ff,5);
            illustration.rect(22,57,66,22,accents[K],6).line(55,59,55,76,white,2);
            illustration.rect(19,59,10,10,0xffe5b9ff,5).rect(80,59,10,10,0xffe5b9ff,5);
            auto spark=[this](int x,int y,uint32_t color) {illustration.line(x-4,y,x+4,y,color,2).line(x,y-4,x,y+4,color,2);};
            spark(15,10,0xe7b44dff);spark(94,22,0xe7b44dff);spark(98,65,accents[K]);
        }
        (void)hero_canvas.present(illustration);
    }
    void draw_quantity() {
        if constexpr(K==0&&AGE==0) {
            if(!quantity_canvas.mounted())return;
            int first=0,second=0;std::sscanf(current.item.question.c_str(),"%d + %d",&first,&second);
            illustration.reset();illustration.rect(0,0,280,46,papers[K]);
            for(int i=0;i<first;++i)illustration.rect(8+(i%5)*20,6+(i/5)*22,15,15,accents[K],7);
            illustration.line(132,20,148,20,muted,2).line(140,12,140,28,muted,2);
            for(int i=0;i<second;++i)illustration.rect(168+(i%5)*20,6+(i/5)*22,15,15,0xe6a447ff,7);
            (void)quantity_canvas.present(illustration);
        }
    }
    Task<void> write_file(std::string_view path,std::string_view bytes) {
        auto file=co_await ctx->fs().open("download.tmp",OpenMode::write|OpenMode::create|OpenMode::truncate);
        if(!file)co_return std::unexpected(file.error());
        auto remaining=std::as_bytes(std::span{bytes.data(),bytes.size()});
        while(!remaining.empty()) {auto n=file->write(remaining);if(!n||!*n)co_return std::unexpected(Error::io_error);remaining=remaining.subspan(*n);}
        auto closed=file->close();if(!closed)co_return std::unexpected(closed.error());
        co_return co_await ctx->fs().rename("download.tmp",path);
    }
    Task<std::string> cached(std::string key) {
        auto path="cache-"+std::to_string(study::cache_slot(key))+".bin";
        auto file=co_await ctx->fs().open(path);if(!file)co_return std::unexpected(file.error());
        std::string data;
        for(;;) {auto n=file->read(network_chunk);if(!n)co_return std::unexpected(n.error());if(!*n)break;
            data.append(reinterpret_cast<char*>(network_chunk.data()),*n);if(data.size()>8600)co_return std::unexpected(Error::resource_limit);}
        auto newline=data.find('\n');
        if(newline==data.npos||data.substr(0,newline)!=key)co_return std::unexpected(Error::not_found);
        co_return data.substr(newline+1);
    }
    Task<std::string> download(std::string address,std::string origin) {
        auto grant=co_await ctx->permissions().acquire("net.client",origin);
        if(!grant)co_return std::unexpected(grant.error());
        static constexpr std::array<std::string_view,1> wanted{"content-type"};
        static constexpr std::array<NetHeaderView,1> headers{{{"user-agent","PXAStudy/0.1.0"}}};
        auto exchange=co_await ctx->http().open({.url=address,.headers=headers,.wanted_headers=wanted,
            .options={.buffer_bytes=1024,.header_bytes=512,.max_body_bytes=8192,.timeout_ms=12000}},*grant);
        if(!exchange)co_return std::unexpected(exchange.error());
        auto response=co_await std::move(*exchange).response();
        if(!response)co_return std::unexpected(response.error());
        if(response->status_code!=200)co_return std::unexpected(Error::io_error);
        std::string data;
        for(;;) {auto n=co_await response->body.read_some(network_chunk);if(!n)co_return std::unexpected(n.error());if(!*n)break;
            data.append(reinterpret_cast<char*>(network_chunk.data()),*n);if(data.size()>8192)co_return std::unexpected(Error::resource_limit);}
        co_return data;
    }
    Task<std::string> fetch(std::string address,std::string origin,bool refresh) {
        from_cache=false;
        if(!refresh) {auto saved=co_await cached(address);if(saved) {from_cache=true;co_return std::move(*saved);}}
        auto live=co_await download(address,origin);
        if(live)co_return std::move(*live);
        auto saved=co_await cached(address);
        if(saved) {from_cache=true;co_return std::move(*saved);}
        co_return std::unexpected(live.error());
    }
    void online() {
        if(loading||busy.get())return;busy.set(true);set_status("读取缓存或获取在线内容…");
        if(!ctx->tasks().start(load_online())) {busy.set(false);set_status("网络任务未启动。");}
    }
    Task<void> load_online() {
        // Snapshot the selected card. Navigating during a request cannot apply
        // the response to a different card or change its learning record.
        auto item=current.item;char encoded[384];media::json::encode(item.lookup.c_str(),encoded,sizeof(encoded));
        std::string address,origin;
        if constexpr(K==1) {origin="https://v1.jinrishici.com";address=origin+"/all.json";}
        if constexpr(K==2) {origin="https://api.dictionaryapi.dev";address=origin+"/api/v2/entries/en/"+encoded;}
        if constexpr(K==3) {origin="https://cdn.jsdelivr.net";address=origin+"/npm/hanzi-writer-data@2.0.1/"+encoded+".json";}
        if constexpr(K==4) {address=url.get();origin="web";
            if(!study::valid_feed_url(address)){busy.set(false);set_status("请输入完整的 HTTPS 课程地址。");co_return Result<void>{};}}
        auto data=co_await fetch(address,origin,K==1||K==4);
        if(!data) {busy.set(false);set_status(K==4?"下载失败，请检查课程地址和网络。原课程已保留。":"在线资源暂时不可用，稍后再试。离线内容仍可学习。");co_return Result<void>{};}
        bool valid=study::complete_json(*data);std::string message;
        std::vector<study::Stroke> parsed_strokes;
        std::vector<study::Item> imported;
        auto root=study::json(*data);
        if constexpr(K==1) {
            auto text=study::string(root.get("content"));auto author=study::string(root.get("author"),120);auto title=study::string(root.get("origin"),180);
            valid=valid&&!text.empty()&&!author.empty();message=text+"\n\n"+author+" · "+title+"\n来源：今日诗词（随机诗句，不属于分级练习）";
        }
        if constexpr(K==2) {
            media::json::Array entries(root);media::json::Value entry;valid=valid&&entries.next(entry);
            media::json::Array meanings(entry.get("meanings"));media::json::Value meaning;valid=valid&&meanings.next(meaning);
            media::json::Array definitions(meaning.get("definitions"));media::json::Value definition;valid=valid&&definitions.next(definition);
            auto text=study::string(definition.get("definition"),1800);
            valid=valid&&!text.empty();
            message=study::string(entry.get("phonetic"),100)+"\n"+text;
            auto example=study::string(definition.get("example"),600);if(!example.empty())message+="\n例句："+example;
            message+="\n来源：Free Dictionary API / Wiktionary\n许可："+study::string(entry.get("license").get("name"),100);
            media::json::Array sources(entry.get("sourceUrls"));media::json::Value source;if(sources.next(source))message+="\n"+study::string(source,200);
        }
        if constexpr(K==3)valid=study::parse_strokes(*data,parsed_strokes);
        if constexpr(K==4)valid=study::parse_course(*data,imported)&&std::all_of(imported.begin(),imported.end(),[](const auto& card){return card.level==AGE;});
        if(!valid) {busy.set(false);set_status("响应不是支持的内容格式，未替换已有内容。");co_return Result<void>{};}
        if constexpr(K==4) {
            auto saved=co_await write_file("course.json",*data);
            if(!saved) {busy.set(false);set_status("课程保存失败，已保留原课程。");co_return Result<void>{};}
            bank=std::move(imported);custom=true;refresh_summary();
            set_status("课程已下载到内部存储，无需 SD 卡。");
        } else {
            Result<void> saved;
            if(!from_cache)saved=co_await write_file("cache-"+std::to_string(study::cache_slot(address))+".bin",address+"\n"+*data);
            const bool same=cards.get()&&current.item.id==item.id;
            if(same) {
                if constexpr(K==3) {strokes=std::move(parsed_strokes);stroke_step=1;has_strokes.set(true);draw_strokes();
                    set_detail("笔顺中线示意 · 点击“下一笔”逐步查看\n数据：Hanzi Writer Data / Make Me A Hanzi\n© 1999 Arphic Technology · Arphic Public License");}
                else set_detail(message);
            }
            set_status(from_cache?"已读内部缓存 · 无需联网":saved?"内容已缓存 · 最多8项":"已获取内容，但缓存写入失败。");
        }
        busy.set(false);co_return Result<void>{};
    }
    void clear_cache() {
        if(loading||busy.get())return;busy.set(true);
        if(!ctx->tasks().start(clear_files()))busy.set(false);
    }
    Task<void> clear_files() {
        for(unsigned i=0;i<8;++i)(void)co_await ctx->fs().remove("cache-"+std::to_string(i)+".bin");
        (void)co_await ctx->fs().remove("download.tmp");
        busy.set(false);set_status("缓存已清理，学习记录已保留。");co_return Result<void>{};
    }
    Task<void> restore() {
        busy.set(true);
        auto removed=co_await ctx->fs().remove("course.json");
        if(removed||removed.error()==Error::not_found) {bank=builtins();custom=false;refresh_summary();set_status("已恢复内置课程。");}
        else set_status("恢复失败，请检查内部存储。");
        busy.set(false);co_return Result<void>{};
    }
    auto header() {
        return Row(text(titles[K],Font::title).grow(),
            text(age_names[AGE],Font::caption,accents[K]).background(tints[K]).radius(7_dp).padding(5_dp),
            button(back_label,[this]{go_back();}).width(54_dp).height(34_dp)
        ).fill().gap(7_dp);
    }
    auto welcome() {
        return Row(picture(hero_canvas,108,82),Column(
            text(hero_titles[AGE],Font::headline,accents[K]).fill(),
            text(hero_lines[AGE],Font::body,muted).fill()
        ).grow().gap(6_dp)).fill().align(Align::center).gap(8_dp);
    }
    auto progress_card() {
        return Column(text(stars,Font::title,accents[K]),
            text(summary,Font::caption,muted)
        ).fill().gap(4_dp).padding(10_dp).background(white).radius(14_dp);
    }
    auto home_panel() {
        if constexpr(K==0) {
            return Column(welcome(),progress_card(),
                Row(button(AGE<3?"开始小挑战":"开始练习",[this]{start(false);},true).enabled(ready).grow().height(45_dp),
                    button(AGE<3?"再试错题":"错题复习",[this]{start(true);}).enabled(ready).grow().height(45_dp)).fill().gap(8_dp),
                button(AGE<3?"给小向导的说明":"使用说明",[this]{page(4);}).height(32_dp).fill()
            ).visible(home).fill().gap(6_dp);
        } else {
            return Column(welcome(),progress_card(),
                Row(button(AGE<3?"开始小挑战":"开始练习",[this]{start(false);},true).enabled(ready).grow().height(45_dp),
                    button(AGE<3?"翻开知识卡":"翻卡学习",[this]{open_cards();}).enabled(ready).grow().height(45_dp)).fill().gap(8_dp),
                Row(button(AGE<3?"再试一次":"错题 / 到期",[this]{start(true);}).enabled(ready).grow().height(33_dp),
                    button(AGE<3?"我的小背包":"设置与资源",[this]{refresh_summary();page(4);}).grow().height(33_dp)).fill().gap(8_dp)
            ).visible(home).fill().gap(6_dp);
        }
    }
    auto answers() {
        if constexpr(K==0||K==2||K==3) {
            return Column(
                Row(button(a,[this]{answer(0);}).height(44_dp).grow(),button(b,[this]{answer(1);}).height(44_dp).grow()).fill().gap(8_dp),
                Row(button(c,[this]{answer(2);}).height(44_dp).grow(),button(d,[this]{answer(3);}).height(44_dp).grow()).fill().gap(8_dp)
            ).fill().gap(8_dp);
        } else {
            return Column(button(a,[this]{answer(0);}).min_height(44_dp).fill(),
                button(b,[this]{answer(1);}).min_height(44_dp).fill(),
                button(c,[this]{answer(2);}).min_height(44_dp).fill(),
                button(d,[this]{answer(3);}).min_height(44_dp).fill()).fill().gap(6_dp);
        }
    }
    auto quiz_panel() {
        return Column(
            Row(text(counter,Font::caption,accents[K]).grow(),text(AGE<3?"想一想，再选择":"选择正确答案",Font::caption,muted)).fill(),
            Progress(session_progress,0,100).background(tints[K]).rgba(accents[K]).fill().height(4_dp),
            Column(text(prompt,Font::headline).fill()).fill().padding(8_dp).background(white).radius(14_dp),
            picture(quantity_canvas,280,46).visible(K==0&&AGE==0),
            answers()
        ).visible(quiz).fill().gap(6_dp);
    }
    auto result_panel() {
        return Column(
            text(counter,Font::caption,muted),
            text(feedback,Font::title,accents[K]),
            Column(text(prompt,Font::body).fill(),text(detail,Font::label).fill()).fill().gap(8_dp).padding(12_dp).background(white).radius(14_dp),
            button("分页阅读讲解",[this]{read_detail();}).height(36_dp).fill(),
            button(AGE<3?"继续探险  >":"继续",[this]{if(finished)page(0);else next_question();},true).height(43_dp).fill()
        ).visible(result).fill().gap(10_dp);
    }
    auto card_panel() {
        return Column(text(counter,Font::caption,accents[K]),
            Column(text(prompt,K==2?Font::display:Font::headline).fill(),text(detail,Font::label).fill()).fill().padding(12_dp).gap(8_dp).background(white).radius(14_dp),
            button("分页阅读",[this]{read_detail();}).height(32_dp).fill(),
            Row(button(reveal_label,[this]{reveal();},true).height(36_dp).grow(),
                button(online_label,[this]{online();}).visible(K>=1&&K<=3).enabled(can_online).height(36_dp).grow()).fill().gap(8_dp),
            picture(stroke_canvas,160,160).visible(has_strokes),
            button("再看下一笔",[this]{advance_stroke();}).visible(has_strokes).height(34_dp).fill(),
            Row(button(AGE<3?"还想再学":"再学",[this]{rate(false);}).grow().height(36_dp),
                button(AGE<3?"我记住啦":"记住",[this]{rate(true);}).grow().height(36_dp)).fill().gap(8_dp),
            Row(button("< 上一张",[this]{browse(-1);}).grow().height(32_dp),
                button("下一张 >",[this]{browse(1);}).grow().height(32_dp)).fill().gap(8_dp)
        ).visible(cards).fill().gap(9_dp);
    }
    auto math_help() {
        constexpr const char* lessons[]={"看看圆点，再算一算。\n绿色一组，黄色一组，合起来是多少？","乘法口诀和100以内减法。\n答完题后，试着说说自己的计算方法。","一元一次方程。\n等式两边做相同的运算，逐步找出x。","百分比与折扣。\n把比例、价格和实际生活联系起来。"};
        return Column(button("阅读使用说明",[this]{read_help();},true).height(38_dp).fill(),text("学习小指南",Font::title,accents[K]),text(lessons[AGE],Font::body).fill(),
            text("上下键逐项选择，确认执行；右上角返回或退出，也支持触摸。每轮最多10题，答错后可以看解题步骤。",Font::body).fill(),
            text("保留最近8道错题。答对后从错题列表移除。全部题目本地生成，无需网络或SD卡。",Font::body).fill()
        ).visible(settings).fill().gap(12_dp).padding(12_dp).background(white).radius(14_dp);
    }
    auto resources() {
        return Column(button("阅读使用说明",[this]{read_help();},true).height(38_dp).fill(),text(AGE<3?"我的小背包":"设置与资源",Font::title,accents[K]),
            text("上下键逐项选择，确认执行；右上角返回或退出。每轮最多10题，也支持触摸。",Font::body).fill(),
            text("先回忆，再核对。复习间隔为1、3、7、14、30、60天；答错的内容可以随时再练。",Font::body).fill(),
            text("核心练习离线可用。联网由你主动开启，内容保存在设备内部，不需要SD卡。",Font::body).fill(),
            text(K==1?"每日诗句是随机在线内容，不属于本年龄段的课程。内置古诗文为公版作品，解释为原创简述。":K==2?"联网可以补充英文释义和音标。词条来源与许可随内容显示。":K==3?"笔顺显示笔画中线示意。数据来自Hanzi Writer Data / Make Me A Hanzi，适用Arphic Public License。":"可以下载自己的HTTPS课程。一次最多24张卡、8KB。课程难度必须与本安装包一致。",Font::body).fill(),
            Column(text(course_note,Font::caption,muted).fill(),text("HTTPS 课程地址",Font::body).fill(),
                TextInput(url).max_bytes(384).single_line().rgba(ink).background(tints[K]).fill().height(36_dp),
                button("下载本年龄课程",[this]{online();},true).height(38_dp).fill(),
                button("恢复内置课程",[this]{if(!busy.get())(void)ctx->tasks().start(restore());}).height(34_dp).fill()
            ).visible(K==4).fill().gap(8_dp),
            button("清理在线缓存",[this]{clear_cache();}).height(34_dp).fill(),
            text("最多8项缓存，每项正文不超过8KB。清理缓存不会删除学习进度。",Font::caption,muted).fill()
        ).visible(settings).fill().gap(12_dp).padding(12_dp).background(white).radius(14_dp);
    }
    auto reader_panel() {
        return Column(text(reading_counter,Font::caption,accents[K]),
            text(reading_text,Font::title).fill().padding(10_dp).background(white).radius(14_dp),
            Row(button("上一页",[this]{if(reader_index){--reader_index;reader_update();}}).enabled(reader_prev).grow().height(40_dp),
                button("下一页",[this]{if(reader_index+1<reader_pages.size()){++reader_index;reader_update();}},true).enabled(reader_next).grow().height(40_dp)
            ).fill().gap(8_dp)
        ).visible(reading).fill().gap(10_dp);
    }
    auto view() {
        if constexpr(K==0) {
            return SafeArea(environment.display(),Column(header(),
                VScroll(home_panel()).visible(home).grow().fill(),
                VScroll(quiz_panel()).visible(quiz).grow().fill(),
                VScroll(result_panel()).visible(result).grow().fill(),
                VScroll(math_help()).visible(settings).grow().fill(),
                VScroll(reader_panel()).visible(reading).grow().fill(),
                text(status,Font::caption,muted).visible(has_notice)
            ).fill().fill_height().gap(7_dp)).background(papers[K]).padding(11_dp);
        } else {
            return SafeArea(environment.display(),Column(header(),
                VScroll(home_panel()).visible(home).grow().fill(),
                VScroll(quiz_panel()).visible(quiz).grow().fill(),
                VScroll(result_panel()).visible(result).grow().fill(),
                VScroll(card_panel()).visible(cards).grow().fill(),
                VScroll(resources()).visible(settings).grow().fill(),
                VScroll(reader_panel()).visible(reading).grow().fill(),
                text(status,Font::caption,muted).visible(has_notice)
            ).fill().fill_height().gap(7_dp)).background(papers[K]).padding(11_dp);
        }
    }

};
}
PXA_APPLICATION(StudyApp)
