    struct BufferedPage {unsigned index=0;std::optional<Asset> asset;};
    std::array<BufferedPage,6> buffered{};
    unsigned buffer_slots=6,buffer_limit=4*1024*1024,buffer_page_bytes=262144;
    bool prefetch_running=false,cache_prepare=false;std::uint64_t prefetch_retry=0;float waiting_scroll=0;
    int buffered_slot(unsigned index)const{for(unsigned i=0;i<buffered.size();++i)if(buffered[i].asset&&buffered[i].index==index)return int(i);return -1;}
    unsigned buffered_bytes()const{unsigned n=0;for(auto& p:buffered)if(p.asset)n+=std::max(p.asset->descriptor().resident_bytes,p.asset->descriptor().decoded_bytes);return n;}
    unsigned buffered_count()const{unsigned n=0;for(auto& p:buffered)n+=bool(p.asset);return n;}
    void clear_buffers(){for(auto& p:buffered)p.asset.reset();waiting_scroll=0;}
    void trim_buffers(unsigned extra=0){
        while(buffered_count()&&(buffered_count()>=buffer_slots||buffered_bytes()+extra>buffer_limit)){
            int victim=-1;unsigned distance=0;
            for(unsigned i=0;i<buffered.size();++i)if(buffered[i].asset){auto at=buffered[i].index;unsigned score=(at>page?at-page:page-at)+unsigned(at<page)*2;if(victim<0||score>distance){victim=i;distance=score;}}
            buffered[victim].asset.reset();
        }
    }
    void remember(unsigned index,Asset asset){
        auto bytes=std::max(asset.descriptor().resident_bytes,asset.descriptor().decoded_bytes);
        buffer_page_bytes=std::max(buffer_page_bytes,bytes);
        if(!buffer_slots||bytes>buffer_limit)return;
        auto slot=buffered_slot(index);if(slot>=0)buffered[slot].asset.reset();trim_buffers(bytes);
        for(auto& p:buffered)if(!p.asset){p.index=index;p.asset.emplace(std::move(asset));return;}
    }
    unsigned ahead_pages()const{return buffer_limit<buffer_page_bytes?0:std::min({4u,buffer_slots,buffer_limit/buffer_page_bytes});}
    std::optional<Asset> take_buffered(unsigned index){auto slot=buffered_slot(index);if(slot<0)return {};auto result=std::move(buffered[slot].asset);buffered[slot].asset.reset();return result;}
    void memory_pressure(bool critical){
        buffer_slots=critical?0:1;buffer_limit=critical?0:262144;clear_buffers();prefetch_retry=now_us+30000000;
        (void)ctx->log().write(LogLevel::info,"MANGA buffer_pressure="+std::to_string(buffer_slots));
    }
    void progress(){
        book.page=page;book.language=language;book.pages=pages.size();
        if(image){auto d=image->descriptor();if(d.height>unsigned(d.width)*1.8f)book.long_strip=true;}
        auto at=std::find_if(shelf.begin(),shelf.end(),[&](auto& b){return manga::same(b,book);});
        if(at!=shelf.end())*at=book;else if(shelf.size()<24)shelf.insert(shelf.begin(),book);
        save();message(page_label());
    }
    float strip_height(const Asset& asset)const{return float(metrics.width)*asset.descriptor().height/asset.descriptor().width;}
    void normalize_strip(){
        if(!image||!vertical()||zoomed())return;
        for(unsigned n=0;n<6;++n){auto h=strip_height(*image);bool forward=pan_y>=h;bool backward=pan_y<0&&page;
            if(!forward&&!backward)break;unsigned next=forward?page+1:page-1;auto asset=take_buffered(next);if(!asset)break;
            if(forward)pan_y-=h;else pan_y+=strip_height(*asset);
            auto old=rendered_page;auto previous=std::move(*image);image.emplace(std::move(*asset));page=rendered_page=next;remember(old,std::move(previous));
            scale=float(metrics.width)/image->descriptor().width;pan_x=0;prefetch_retry=0;progress();dirty=true;
        }
    }
    bool needs_prefetch()const{
        if(screen!=reading||busy||!image||!cache.enabled||!buffer_slots||prefetch_running||now_us<prefetch_retry)return false;
        const unsigned ahead=ahead_pages();
        for(unsigned i=page+1;i<pages.size()&&i<=page+ahead;++i)if(buffered_slot(i)<0)return true;
        return false;
    }
    Task<Asset> fetch_page(unsigned index,bool foreground){
        if(index>=pages.size())co_return std::unexpected(Error::not_found);auto key=manga::page_cache_key(book,index);if(key.empty())key=pages[index];auto a=co_await cached_image(key,foreground);
        if(!a&&a.error()==Error::not_found&&key!=pages[index])a=co_await cached_image(pages[index],foreground);
        if(a)co_return std::move(a);if(a.error()==Error::resource_limit||a.error()==Error::quota_exceeded)co_return std::unexpected(a.error());
        std::string url=pages[index];
        if(manga::source_kind(book.source)==manga::SourceKind::town){
            if(index>=originals.size())co_return std::unexpected(Error::protocol_error);
            if(originals[index].empty()){auto text=co_await json(pages[index],false,262144);if(!text)co_return std::unexpected(text.error());auto value=manga::town_image(*text);if(!value)co_return std::unexpected(Error::protocol_error);originals[index]=std::move(*value);}url=originals[index];
        }
        auto downloaded=co_await download_image(url);if(!downloaded&&download_storage_failed){auto reclaimed=co_await storage_pressure();(void)reclaimed;downloaded=co_await download_image(url);}if(!downloaded)co_return std::unexpected(downloaded.error());a=co_await decoded_download(key,foreground);
        if(!a&&book.source.empty()&&index<originals.size()&&originals[index]!=url&&(a.error()==Error::unsupported||a.error()==Error::limit_exceeded)){
            downloaded=co_await download_image(originals[index]);if(downloaded)a=co_await decoded_download(key,foreground);
        }
        co_return std::move(a);
    }
    Task<void> prefetch_pages(){
        // One cancellable producer owns the shared scratch files/buffer. Page
        // navigation cancels it before starting any foreground request.
        const unsigned anchor=page,ahead=ahead_pages();
        for(unsigned i=anchor+1;i<pages.size()&&i<=anchor+ahead;++i){
            if(screen!=reading||busy||!cache.enabled||i>page+ahead_pages())break;if(buffered_slot(i)>=0)continue;
            trim_buffers(524288);auto asset=co_await fetch_page(i,false);
            if(!asset){
                if(asset.error()==Error::resource_limit||asset.error()==Error::quota_exceeded){
                    // A denied extra image does not invalidate the neighbors
                    // already resident. Keep the proven working capacity.
                    buffer_slots=std::max(1u,buffered_count());buffer_limit=std::max(buffer_page_bytes,buffered_bytes());
                    (void)ctx->log().write(LogLevel::info,"MANGA buffer_capacity="+std::to_string(buffer_slots));
                }
                prefetch_retry=now_us+30000000;(void)ctx->log().write(LogLevel::info,"MANGA prefetch_failed="+std::to_string(i)+" error="+std::to_string(int(asset.error())));break;
            }
            remember(i,std::move(*asset));dirty=true;
            (void)ctx->log().write(LogLevel::info,"MANGA prefetched="+std::to_string(i)+" resident="+std::to_string(buffered_count())+" bytes="+std::to_string(buffered_bytes()));
            if(waiting_scroll&&vertical()&&!zoomed()){pan_y+=waiting_scroll;waiting_scroll=0;clamp();normalize_strip();changed();}
            auto y=co_await ctx->clock().yield();if(!y)break;
        }
        prefetch_running=false;co_return Result<void>{};
    }
