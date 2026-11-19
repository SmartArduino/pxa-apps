    struct BufferedPage {unsigned index=0;std::optional<Asset> asset;};
    std::array<BufferedPage,24> buffered{};
    unsigned buffer_slots=24,buffer_limit=4*1024*1024+65536,buffer_page_bytes=262144;
    bool prefetch_running=false,cache_prepare=false;std::uint64_t prefetch_retry=0;float waiting_scroll=0;
    int requested_page=-1,prefetch_page=-1;
    enum class StripNotice {hidden,loading,failed,end};
    StripNotice strip_notice=StripNotice::hidden,previous_notice=StripNotice::hidden;
    int buffered_slot(unsigned index)const{for(unsigned i=0;i<buffered.size();++i)if(buffered[i].asset&&buffered[i].index==index)return int(i);return -1;}
    unsigned buffered_bytes()const{unsigned n=0;for(auto& p:buffered)if(p.asset)n+=std::max(p.asset->descriptor().resident_bytes,p.asset->descriptor().decoded_bytes);return n;}
    unsigned buffered_count()const{unsigned n=0;for(auto& p:buffered)n+=bool(p.asset);return n;}
    bool can_buffer(unsigned index)const{
        if(buffered_count()<buffer_slots)return true;
        for(auto& p:buffered)if(p.asset&&(p.index<page||p.index>index))return true;
        return false;
    }
    void clear_buffers(){for(auto& p:buffered)p.asset.reset();waiting_scroll=0;requested_page=-1;}
    void trim_buffers(unsigned extra=0){
        while(buffered_count()&&(buffered_count()>=buffer_slots||buffered_bytes()+extra>buffer_limit)){
            int victim=-1;unsigned distance=0;
                for(unsigned i=0;i<buffered.size();++i)if(buffered[i].asset){auto at=buffered[i].index;unsigned score=(at>page?at-page:page-at)+unsigned(at<page)*2+(at+1<page?1000000u:0u);if(victim<0||score>distance){victim=i;distance=score;}}
            buffered[victim].asset.reset();
        }
    }
    void remember(unsigned index,Asset asset){
        auto bytes=std::max(asset.descriptor().resident_bytes,asset.descriptor().decoded_bytes);
        if(!buffer_slots||bytes>buffer_limit)return;
        // Follow the most recent panel size: one large cover must not keep
        // limiting every subsequent tiny strip segment to a few buffers.
        buffer_page_bytes=std::max(1u,bytes);
        auto slot=buffered_slot(index);if(slot>=0)buffered[slot].asset.reset();trim_buffers(bytes);
        for(auto& p:buffered)if(!p.asset){p.index=index;p.asset.emplace(std::move(asset));return;}
    }
    unsigned ahead_pages()const{return buffer_limit<buffer_page_bytes?0:std::min({4u,buffer_slots,buffer_limit/buffer_page_bytes});}
    unsigned buffer_capacity()const{
        unsigned slots=std::min<unsigned>(buffered.size(),buffer_slots);if(!slots||buffer_limit<buffer_page_bytes)return 0;
        auto previous=page?buffered_slot(page-1):-1;unsigned retained=previous<0?0:std::max(buffered[previous].asset->descriptor().resident_bytes,buffered[previous].asset->descriptor().decoded_bytes);
        if(retained&&slots>1)--slots;
        unsigned reserve=std::max(524288u,buffer_page_bytes),available=buffer_limit>reserve+retained?buffer_limit-reserve-retained:0;
        // The last candidate is not resident yet; reserve its decode space.
        return std::min(slots,1+available/std::max(1u,buffer_page_bytes));
    }
    std::optional<Asset> take_buffered(unsigned index){auto slot=buffered_slot(index);if(slot<0)return {};auto result=std::move(buffered[slot].asset);buffered[slot].asset.reset();return result;}
    void memory_pressure(bool critical){
        buffer_slots=critical?0:1;buffer_limit=critical?0:1048576;clear_buffers();buffer_page_bytes=262144;prefetch_retry=now_us+30000000;
        (void)ctx->log().write(LogLevel::info,"MANGA buffer_pressure="+std::to_string(buffer_slots));
    }
    void progress(){
        book.page=page;book.language=language;book.pages=pages.size();
        if(image){auto d=image->descriptor();if(d.height>unsigned(d.width)*1.8f)book.long_strip=true;}
        auto at=std::find_if(shelf.begin(),shelf.end(),[&](auto& b){return manga::same(b,book);});
        if(at!=shelf.end())*at=book;else if(shelf.size()<24)shelf.insert(shelf.begin(),book);
        save();status=page_label();dirty=true;if(bars)ui_dirty=true;
    }
    float strip_height(const Asset& asset)const{return float(metrics.width)*asset.descriptor().height/asset.descriptor().width;}
    float strip_zoom()const{return image?scale*image->descriptor().width/metrics.width:1.f;}
    unsigned strip_end()const{unsigned last=page;while(last+1<pages.size()&&last<page+buffered.size()&&buffered_slot(last+1)>=0)++last;return last;}
    float strip_source_height()const{float h=image->descriptor().height;for(unsigned i=page+1;i<=strip_end();++i)h+=strip_height(*buffered[buffered_slot(i)].asset)*image->descriptor().width/metrics.width;return h;}
    StripNotice strip_notice_state(bool previous=false)const{
        if(screen!=reading||busy||!failure.empty()||!image||rendered_page!=page||!vertical()||pages.empty())return StripNotice::hidden;
        if(previous){
            if(pan_y>1)return StripNotice::hidden;
            if(!page)return StripNotice::end;
            if(buffered_slot(page-1)>=0||(waiting_scroll>=0&&requested_page!=int(page-1)))return StripNotice::hidden;
            return !prefetch_running&&(!buffer_slots||prefetch_retry>now_us)?StripNotice::failed:StripNotice::loading;
        }
        if(waiting_scroll<0)return StripNotice::hidden;
        const unsigned end=strip_end();const float z=strip_zoom();float bottom=strip_height(*image)*z-pan_y;
        for(unsigned i=page+1;i<=end;++i)bottom+=strip_height(*buffered[buffered_slot(i)].asset)*z;
        if(bottom>metrics.height+1)return StripNotice::hidden;
        if(end+1==pages.size())return StripNotice::end;
        return !prefetch_running&&(!buffer_slots||prefetch_retry>now_us)?StripNotice::failed:StripNotice::loading;
    }
    void queue_strip_edge(){
        int next=-1;if(waiting_scroll>0&&strip_end()+1<pages.size())next=strip_end()+1;
        if(waiting_scroll<0&&page&&buffered_slot(page-1)<0)next=page-1;
        requested_page=next;
        if(next>=0&&prefetch_running&&prefetch_page>=0&&prefetch_page!=next){ctx->foreground_tasks().cancel();clear_wire();prefetch_running=false;prefetch_page=-1;}
    }
    void normalize_strip(){
        if(!image||!vertical())return;const float z=strip_zoom();
        for(unsigned n=0;n<buffered.size();++n){auto h=strip_height(*image)*z;bool forward=pan_y>=h;bool backward=pan_y<0&&page;
            if(!forward&&!backward)break;unsigned next=forward?page+1:page-1;auto asset=take_buffered(next);if(!asset)break;
            if(forward)pan_y-=h;else pan_y+=strip_height(*asset)*z;
            auto old=rendered_page;auto previous=std::move(*image);image.emplace(std::move(*asset));page=rendered_page=next;remember(old,std::move(previous));
            scale=float(metrics.width)/image->descriptor().width*z;prefetch_retry=0;progress();dirty=true;
            (void)ctx->log().write(LogLevel::info,"MANGA strip_page="+std::to_string(page)+" offset="+std::to_string(int(pan_y)));
        }
    }
    int next_buffer_page()const{
        if(requested_page>=0&&unsigned(requested_page)<pages.size()&&unsigned(requested_page)!=page&&buffered_slot(requested_page)<0)return can_buffer(requested_page)?requested_page:-1;
        const unsigned ahead=ahead_pages();
        float covered=image?image->descriptor().height*scale-pan_y:0;
        const unsigned capacity=buffer_capacity();
        for(unsigned i=page+1;i<pages.size()&&i<=page+capacity;++i){auto slot=buffered_slot(i);if(slot>=0)covered+=strip_height(*buffered[slot].asset)*strip_zoom();
            else if(i<=page+ahead||(vertical()&&covered<metrics.height*6.f))return can_buffer(i)?int(i):-1;else break;}
        return -1;
    }
    int next_wire_page(unsigned index)const{
        const int direction=index<page?-1:1;
        for(int i=int(index)+direction;i>=0&&unsigned(i)<pages.size();i+=direction){
            if(unsigned(i)==rendered_page||buffered_slot(i)>=0)continue;
            auto key=manga::page_cache_key(book,i);if(!key.empty())key+=":rgb3";else key=pages[i];
            if(cache.find(key)<0)return i;
        }
        return -1;
    }
    bool buffered_visible(unsigned index)const{
        if(!image||!vertical())return false;
        const float z=strip_zoom();float top=-pan_y;
        if(index<page)return index+1==page&&top>0;
        top+=strip_height(*image)*z;
        for(unsigned i=page+1;i<index;++i){auto slot=buffered_slot(i);if(slot<0)return false;top+=strip_height(*buffered[slot].asset)*z;}
        return top<metrics.height&&index<=strip_end();
    }
    bool needs_prefetch(){
        // After pressure, retry only the image explicitly needed at the edge.
        // Keep the current image visible while the Host decides what fits.
        if(!buffer_slots&&requested_page>=0&&now_us>=prefetch_retry){buffer_slots=1;buffer_limit=1048576;}
        return screen==reading&&!busy&&image&&!image_finishing&&pending_cache_file.empty()&&buffer_slots&&!prefetch_running&&now_us>=prefetch_retry&&next_buffer_page()>=0;
    }
    Task<void> metadata_pause(){
        // Metadata calls are synchronous in some SD Hosts. Spend the decoded
        // lookahead during a drag, and do optional metadata work when idle.
        while(interacting()&&!busy&&requested_page<0&&image){
            float covered=image->descriptor().height*scale-pan_y;
            if(vertical())for(unsigned i=page+1;i<=strip_end();++i)covered+=strip_height(*buffered[buffered_slot(i)].asset)*strip_zoom();
            if(vertical()?covered<metrics.height*1.25f:buffered_slot(page+1)<0)break;
            if(!co_await frame_work.yield())co_return std::unexpected(Error::resource_limit);
        }
        co_return Result<void>{};
    }
    void apply_waiting_scroll(){
        if(!waiting_scroll||!vertical()||touches.count()==2)return;
        float old=pan_y;pan_y+=waiting_scroll;clamp();waiting_scroll-=pan_y-old;normalize_strip();
        if(std::abs(waiting_scroll)<.5f||(waiting_scroll>0&&strip_end()+1==pages.size())||(waiting_scroll<0&&!page))waiting_scroll=0;
        queue_strip_edge();changed();
    }
    Task<Asset> fetch_page(unsigned index,bool foreground){
        if(index>=pages.size())co_return std::unexpected(Error::not_found);auto key=manga::page_cache_key(book,index);if(key.empty())key=pages[index];else key+=":rgb3";auto a=co_await cached_image(key,foreground);
        if(a)co_return std::move(a);if(a.error()==Error::resource_limit||a.error()==Error::quota_exceeded)co_return std::unexpected(a.error());
        std::string url=pages[index];
        if(manga::source_kind(book.source)==manga::SourceKind::town){
            if(index>=originals.size())co_return std::unexpected(Error::protocol_error);
            if(originals[index].empty()){auto text=co_await json(pages[index],false,262144);if(!text)co_return std::unexpected(text.error());auto value=manga::town_image(*text);if(!value)co_return std::unexpected(Error::protocol_error);originals[index]=std::move(*value);}url=originals[index];
        }
        const int following=next_wire_page(index);auto downloaded=co_await download_image(url,following);if(!downloaded&&download_storage_failed){auto reclaimed=co_await storage_pressure();(void)reclaimed;downloaded=co_await download_image(url,following);}if(!downloaded)co_return std::unexpected(downloaded.error());a=co_await decoded_download(key,foreground);
        if(!a&&book.source.empty()&&index<originals.size()&&originals[index]!=url&&(a.error()==Error::unsupported||a.error()==Error::limit_exceeded)){
            downloaded=co_await download_image(originals[index]);if(downloaded)a=co_await decoded_download(key,foreground);
        }
        co_return std::move(a);
    }
    Task<void> prefetch_pages(){
        // One cancellable producer owns the shared scratch files/buffer. Page
        // navigation cancels it before starting any foreground request.
        for(unsigned work=0;work<12;++work){
            if(screen!=reading||busy||!buffer_slots)break;int candidate=next_buffer_page();if(candidate<0)break;unsigned i=candidate;prefetch_page=candidate;
            trim_buffers(std::max(524288u,buffer_page_bytes));auto asset=co_await fetch_page(i,false);sync_clock();
            if(!asset){
                if(asset.error()==Error::resource_limit||asset.error()==Error::quota_exceeded){
                    // A denied extra image does not invalidate the neighbors
                    // already resident. Keep the proven working capacity.
                    buffer_slots=std::max(1u,buffered_count());buffer_limit=std::max({1048576u,buffer_page_bytes,buffered_bytes()});
                    (void)ctx->log().write(LogLevel::info,"MANGA buffer_capacity="+std::to_string(buffer_slots));
                }
                prefetch_retry=now_us+30000000;(void)ctx->log().write(LogLevel::info,"MANGA prefetch_failed="+std::to_string(i)+" error="+std::to_string(int(asset.error())));break;
            }
            if(!book.long_strip&&asset->descriptor().height>asset->descriptor().width*1.8f){book.long_strip=true;progress();}
            remember(i,std::move(*asset));dirty|=buffered_visible(i);
            (void)ctx->log().write(LogLevel::info,"MANGA prefetched="+std::to_string(i)+" resident="+std::to_string(buffered_count())+" bytes="+std::to_string(buffered_bytes()));
            prefetch_page=-1;if(requested_page==int(i))requested_page=-1;apply_waiting_scroll();
            auto y=co_await ctx->clock().yield();if(!y)break;
            auto stored=co_await finish_image_cache();(void)stored;
        }
        prefetch_running=false;prefetch_page=-1;co_return Result<void>{};
    }
