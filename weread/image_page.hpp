#pragma once
#include "diagnostics.hpp"
#include "image_cache.hpp"
#include <pxa/app.hpp>
#include "document_layout.hpp"
#include <cmath>
#include <chrono>
#include <memory>

namespace weread {
// Visible assets and a few recent images share one pixel budget. One decode at a time;
// Pixels stay on Host. Compressed images use a bounded private disk cache;
// streaming copies need only a temporary 4 KiB Guest buffer.
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
    void configure(pxa::Context& context,pxa::Permission& permission){context_=&context;permission_=&permission;cache_.configure(context);}
    void reset(){
        tasks_.reset();
        std::vector<Entry>{}.swap(entries_);std::vector<Entry>{}.swap(recent_);loading_=false;dirty_=false;waiting_permission_=false;
        pixel_budget_=65536;
    }
    void clear_page(){
        if(tasks_)tasks_->cancel();loading_=false;
        retain();entries_.clear();dirty_=false;waiting_permission_=false;
    }
    void suspend(){if(tasks_)tasks_->cancel();loading_=false;suspended_=true;}
    void resume(){suspended_=false;}
    bool poll(){
        if(tasks_)tasks_->reap();
        if(waiting_permission_&&!loading_&&permission_&&*permission_){waiting_permission_=false;schedule();}
        return std::exchange(dirty_,false);
    }
    const Entry* find(unsigned index)const{
        const auto found=std::ranges::find(entries_,index,&Entry::index);
        return found==entries_.end()?nullptr:&*found;
    }
    std::optional<Entry> take(unsigned index){
        auto found=std::ranges::find(entries_,index,&Entry::index);
        if(found==entries_.end()||!found->asset)return {};
        return std::move(*found);
    }
    void remember(Entry entry){recent_.push_back(std::move(entry));trim(65536);}
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
    void update(std::span<const Request> wanted,bool enabled,unsigned pixel_budget=65536){
        pixel_budget=std::clamp(pixel_budget,4096u,262144u);
        if(pixel_budget_!=pixel_budget){reset();pixel_budget_=pixel_budget;}
        if(!context_||!context_->foreground()||!enabled||wanted.size()>64){if(!entries_.empty()||!recent_.empty())reset();return;}
        const auto count=wanted.size();
        bool same=count==entries_.size();
        for(std::size_t i=0;same&&i<count;++i)same=entries_[i].index==wanted[i].index&&entries_[i].url==wanted[i].url;
        if(same){schedule();return;}
        if(tasks_)tasks_->cancel();loading_=false;
        waiting_permission_=false;
        if(!count){reset();return;}
        retain();entries_.clear();
        std::uint64_t held=0,demand=0;std::array<bool,8> reused{};
        for(const auto& run:wanted){
            const auto old=std::ranges::find(recent_,run.url,&Entry::url);
            if(old!=recent_.end()&&old->asset&&!reused[old-recent_.begin()]){
                reused[old-recent_.begin()]=true;
                const auto d=old->asset->descriptor();held+=std::uint64_t(d.width)*d.height;
            }
            else demand+=std::uint64_t(std::max(1,run.width))*std::max(1,run.height);
        }
        if(held>=pixel_budget&&demand){
            recent_.clear();held=demand=0;
            for(const auto& run:wanted)demand+=std::uint64_t(std::max(1,run.width))*std::max(1,run.height);
        }
        const auto room=held<pixel_budget?pixel_budget-unsigned(held):0u;
        const auto scale=std::min(1.,std::sqrt(double(room)/std::max<std::uint64_t>(1,demand)));
        std::vector<Entry> next;next.reserve(count);
        unsigned assigned=0;
        for(const auto& run:wanted){
            auto old=std::ranges::find(recent_,run.url,&Entry::url);
            if(old!=recent_.end()&&old->asset){
                old->index=run.index;
                log("WEREAD IMAGE MEMORY HIT",{{"index",run.index}});
                log("WEREAD IMAGE READY",{{"index",run.index},{"bytes",old->asset->descriptor().resident_bytes},{"ms",0}});
                next.push_back(std::move(*old));old->asset.reset();continue;
            }
            const auto width=std::clamp(int(run.width*scale),1,1024),height=std::clamp(int(run.height*scale),1,1024);
            Entry entry{run.index,std::uint16_t(width),std::uint16_t(height),std::string(run.url),{},false};
            const auto pixels=unsigned(width)*unsigned(height);
            if(entry.url.empty()||pixels>room-assigned)entry.failed=true;
            else assigned+=pixels;
            next.push_back(std::move(entry));
        }
        std::erase_if(recent_,[](const Entry& e){return !e.asset;});
        trim(pixel_budget-unsigned(held)-assigned);
        entries_=std::move(next);schedule();
    }

private:
    static unsigned pixels(const Entry& entry){
        if(!entry.asset)return 0;
        const auto d=entry.asset->descriptor();return unsigned(d.width)*d.height;
    }
    void trim(unsigned budget){
        unsigned used=0;for(const auto& entry:recent_)used+=pixels(entry);
        while(!recent_.empty()&&(used>budget||recent_.size()>8)){
            used-=pixels(recent_.front());recent_.erase(recent_.begin());
        }
    }
    void retain(){
        for(auto& entry:entries_)if(entry.asset){
            std::erase_if(recent_,[&](const Entry& e){return e.url==entry.url;});
            recent_.push_back(std::move(entry));entry.asset.reset();
        }
        trim(65536);
    }
    void log(std::string_view message,std::initializer_list<Metric> metrics){
        if(auto line=diagnostic(message,metrics))(void)context_->log().write(pxa::LogLevel::info,line->view());
    }
    pxa::Task<pxa::Asset> image(std::string_view url,std::uint16_t width,std::uint16_t height){
        if(!trusted_image_url(url))co_return std::unexpected(pxa::Error::unsupported);
        const pxa::ImageDecodeOptions options{.max_width=width,.max_height=height,
            .max_pixels=std::uint32_t(width)*height,.max_encoded_bytes=524288,.formats=pxa::ImageFormats::any};
        auto file=co_await cache_.find(url);
        if(file){
            auto asset=co_await context_->assets().decode_image(std::move(*file),options);
            if(asset){log("WEREAD IMAGE CACHE HIT",{});co_return std::move(*asset);}
            // Do not evict a good source because of transient decode pressure.
            if(asset.error()!=pxa::Error::resource_limit)
                (void)co_await cache_.invalidate();
        }
        if(!permission_||!*permission_)co_return std::unexpected(pxa::Error::would_block);
        co_return co_await download(std::string(url),width,height);
    }
    pxa::Task<pxa::Asset> download(std::string url,std::uint16_t width,std::uint16_t height,bool cache=true){
        const auto original=url;
        const auto referrer=std::string(origin)+"/";
        const std::array<pxa::NetHeaderView,3> headers{{{"User-Agent",user_agent},{"Accept","image/*"},{"Referer",referrer}}};
        static constexpr std::array<std::string_view,2> wanted{"content-type","location"};
        for(unsigned redirect=0;redirect<=3;++redirect){
            auto opened=co_await context_->http().open({.url=url,.headers=headers,.wanted_headers=wanted,
                .options={.buffer_bytes=4096,.header_bytes=512,.max_body_bytes=524288,.timeout_ms=15000}},*permission_);
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
            const pxa::ImageDecodeOptions options{
                .max_width=width,.max_height=height,.max_pixels=std::uint32_t(width)*height,.max_encoded_bytes=524288,
                .formats=pxa::ImageFormats::any};
            if(cache){
                ImageCache::Transfer transfer;
                auto stored=co_await cache_.store(*response,original,transfer);
                if(!stored){
                    const auto error=stored.error();
                    (void)co_await context_->fs().remove(ImageCache::draft);
                    // No body bytes were consumed if the draft could not be
                    // opened/written. Reuse the live HTTP stream immediately.
                    if(!transfer.consumed)
                        co_return co_await context_->assets().decode_image(std::move(response->body),options);
                    response={};
                    if(transfer.stream_failed)co_return std::unexpected(error);
                    co_return co_await download(original,width,height,false);
                }
                // Drop HTTP/TLS resources before decoding the cached source.
                response={};
                if(stored){
                    auto asset=co_await context_->assets().decode_image(std::move(*stored),options);
                    if(asset){
                        const auto slot=cache_.victim(original,transfer.bytes);
                        if(auto committed=co_await context_->fs().replace(ImageCache::draft,ImageCache::path(slot));committed){
                            cache_.committed(slot);log("WEREAD IMAGE CACHE STORED",{});
                        }
                        co_return std::move(*asset);
                    }
                    (void)co_await context_->fs().remove(ImageCache::draft);
                    co_return std::unexpected(asset.error());
                }
            }
            co_return co_await context_->assets().decode_image(std::move(response->body),options);
        }
        co_return std::unexpected(pxa::Error::limit_exceeded);
    }
    pxa::Task<void> load(){
        loading_=true;
        struct Guard{PageImages& owner;~Guard(){owner.loading_=false;}} guard{*this};
        for(auto& entry:entries_){
            if(entry.asset||entry.failed)continue;
            const auto began=std::chrono::steady_clock::now();
            auto asset=co_await image(entry.url,entry.width,entry.height);
            if(asset){
                const auto descriptor=asset->descriptor();entry.asset=std::move(*asset);
                log("WEREAD IMAGE READY",{{"index",entry.index},{"bytes",descriptor.resident_bytes},
                    {"ms",std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-began).count()}});
            }else if(asset.error()==pxa::Error::would_block){
                waiting_permission_=true;continue;
            }else{
                entry.failed=true;
                log("WEREAD IMAGE ERROR",{{"index",entry.index},{"code",int(asset.error())}});
            }
            dirty_=true;
        }
        co_return pxa::Result<void>{};
    }
    void schedule(){
        if(suspended_||loading_||!context_||!context_->foreground()||
            (waiting_permission_&&(!permission_||!*permission_))||
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
    ImageCache cache_;
    std::vector<Entry> entries_;
    std::vector<Entry> recent_;
    std::unique_ptr<pxa::TaskScope> tasks_;
    bool dirty_=false,loading_=false,waiting_permission_=false,suspended_=false;
    unsigned pixel_budget_=65536;
};
} // namespace weread
