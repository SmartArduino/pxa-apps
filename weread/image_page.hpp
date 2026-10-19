#pragma once
#include "diagnostics.hpp"
#include <pxa/app.hpp>
#include "document_layout.hpp"
#include <cmath>
#include <memory>

namespace weread {
// Only assets on the current page are retained. One HTTP/decode at a time;
// the encoded stream and pixels stay on Host, never in Guest or a temp file.
class PageImages {
public:
    PageImages()=default;
    ~PageImages(){reset();}
    struct Entry {
        std::uint16_t index=0,width=0,height=0;
        std::string url;
        std::optional<pxa::Asset> asset;
        bool failed=false;
    };
    void configure(pxa::Context& context,pxa::Permission& permission){context_=&context;permission_=&permission;}
    void reset(){
        tasks_.reset();
        std::vector<Entry>{}.swap(entries_);loading_=false;dirty_=false;
    }
    bool poll(){if(tasks_)tasks_->reap();return std::exchange(dirty_,false);}
    const Entry* find(unsigned index)const{
        const auto found=std::ranges::find(entries_,index,&Entry::index);
        return found==entries_.end()?nullptr:&*found;
    }
    void retry(unsigned index){
        auto entry=std::ranges::find(entries_,index,&Entry::index);
        if(entry!=entries_.end()&&!entry->url.empty())entry->failed=false;
        schedule();
    }
    struct Request {std::uint16_t index;int width,height;std::string_view url;};
    void update(const ReadingPage& page,std::span<const BookImage> images,bool enabled){
        std::array<Request,64> wanted{};std::size_t count=0;
        for(std::size_t i=0;i<page.count;++i)if(page.lines[i].is_image()&&page.lines[i].image<images.size()){
            const auto& run=page.lines[i];wanted[count++]={run.image,run.width,run.height,images[run.image].url};
        }
        update(std::span(wanted).first(count),enabled);
    }
    // Borrowed visible image requests let covers share the exact same cache,
    // decoder, cancellation and pixel budget as chapter illustrations.
    void update(std::span<const Request> wanted,bool enabled){
        if(!context_||!context_->foreground()||!enabled||wanted.size()>64){if(!entries_.empty())reset();return;}
        const auto count=wanted.size();
        bool same=count==entries_.size();
        for(std::size_t i=0;same&&i<count;++i)same=entries_[i].index==wanted[i].index&&entries_[i].url==wanted[i].url;
        if(same){schedule();return;}
        if(tasks_)tasks_->cancel();loading_=false;
        if(!count){reset();return;}
        std::uint64_t held=0,demand=0;
        for(const auto& run:wanted){
            const auto* old=find(run.index);
            if(old&&old->url==run.url&&old->asset){const auto d=old->asset->descriptor();held+=std::uint64_t(d.width)*d.height;}
            else demand+=std::uint64_t(std::max(1,run.width))*std::max(1,run.height);
        }
        constexpr unsigned pixel_budget=65536;
        if(held>=pixel_budget&&demand){
            for(auto& entry:entries_)entry.asset.reset();held=demand=0;
            for(const auto& run:wanted)demand+=std::uint64_t(std::max(1,run.width))*std::max(1,run.height);
        }
        const auto room=held<pixel_budget?pixel_budget-unsigned(held):0u;
        const auto scale=std::min(1.,std::sqrt(double(room)/std::max<std::uint64_t>(1,demand)));
        std::vector<Entry> next;next.reserve(count);
        unsigned assigned=0;
        for(const auto& run:wanted){
            auto old=std::ranges::find(entries_,run.index,&Entry::index);
            if(old!=entries_.end()&&old->url==run.url&&old->asset){next.push_back(std::move(*old));continue;}
            const auto width=std::clamp(int(run.width*scale),1,1024),height=std::clamp(int(run.height*scale),1,1024);
            Entry entry{run.index,std::uint16_t(width),std::uint16_t(height),std::string(run.url),{},false};
            const auto pixels=unsigned(width)*unsigned(height);
            if(entry.url.empty()||pixels>room-assigned)entry.failed=true;
            else assigned+=pixels;
            next.push_back(std::move(entry));
        }
        entries_=std::move(next);schedule();
    }

private:
    void log(std::string_view message,std::initializer_list<Metric> metrics){
        if(auto line=diagnostic(message,metrics))(void)context_->log().write(pxa::LogLevel::info,line->view());
    }
    pxa::Task<pxa::Asset> download(std::string url,std::uint16_t width,std::uint16_t height){
        const auto referrer=std::string(origin)+"/";
        const std::array<pxa::NetHeaderView,3> headers{{{"User-Agent",user_agent},{"Accept","image/*"},{"Referer",referrer}}};
        static constexpr std::array<std::string_view,2> wanted{"content-type","location"};
        for(unsigned redirect=0;redirect<=3;++redirect){
            auto opened=co_await context_->http().open({.url=url,.headers=headers,.wanted_headers=wanted,
                .options={.buffer_bytes=1024,.header_bytes=512,.max_body_bytes=524288,.timeout_ms=15000}},*permission_);
            if(!opened)co_return std::unexpected(opened.error());
            auto response=co_await std::move(*opened).response();
            if(!response)co_return std::unexpected(response.error());
            const auto status=response->status_code;
            if(status==301||status==302||status==303||status==307||status==308){
                if(redirect==3)co_return std::unexpected(pxa::Error::limit_exceeded);
                auto next=image_redirect(url,response->headers.first("location").value_or(""));
                if(!next)co_return std::unexpected(pxa::Error::unsupported);
                url=std::move(*next);continue;
            }
            if(status<200||status>=300){
                log("WEREAD IMAGE HTTP",{{"status",status}});
                co_return std::unexpected(pxa::Error::protocol_error);
            }
            co_return co_await context_->assets().decode_image(std::move(response->body),{
                .max_width=width,.max_height=height,.max_pixels=std::uint32_t(width)*height,.max_encoded_bytes=524288,
                .formats=pxa::ImageFormats::any});
        }
        co_return std::unexpected(pxa::Error::limit_exceeded);
    }
    pxa::Task<void> load(){
        loading_=true;
        struct Guard{PageImages& owner;~Guard(){owner.loading_=false;}} guard{*this};
        for(auto& entry:entries_){
            if(entry.asset||entry.failed)continue;
            auto asset=co_await download(entry.url,entry.width,entry.height);
            if(asset){
                const auto descriptor=asset->descriptor();entry.asset=std::move(*asset);
                log("WEREAD IMAGE READY",{{"index",entry.index},{"bytes",descriptor.resident_bytes}});
            }else{
                entry.failed=true;
                log("WEREAD IMAGE ERROR",{{"index",entry.index},{"code",int(asset.error())}});
            }
            dirty_=true;
        }
        co_return pxa::Result<void>{};
    }
    void schedule(){
        if(loading_||!context_||!context_->foreground()||!permission_||!*permission_||
            std::ranges::none_of(entries_,[](const Entry& entry){return !entry.asset&&!entry.failed;}))return;
        if(!tasks_){
            tasks_=std::make_unique<pxa::TaskScope>();
            tasks_->on_error(this,[](void* self,pxa::Error)noexcept{
                auto& page=*static_cast<PageImages*>(self);
                for(auto& entry:page.entries_)if(!entry.asset)entry.failed=true;
                page.dirty_=true;
            });
        }
        if(auto started=tasks_->start(load());!started){for(auto& entry:entries_)if(!entry.asset)entry.failed=true;dirty_=true;}
    }
    pxa::Context* context_=nullptr;
    pxa::Permission* permission_=nullptr;
    std::vector<Entry> entries_;
    std::unique_ptr<pxa::TaskScope> tasks_;
    bool dirty_=false,loading_=false;
};
} // namespace weread
