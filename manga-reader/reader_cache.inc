    Task<void> save_cache_settings(){auto r=co_await write_cache_index();busy=false;message(r?"设置已保存":"设置保存失败");co_return Result<void>{};}
    static std::string cache_path(unsigned slot){return "reader-cache-"+std::to_string(slot)+".png";}
    std::string page_label()const{return pages.empty()?"暂无页码":"第 "+std::to_string(page+1)+" / "+std::to_string(pages.size())+" 页";}
    std::string cache_label()const{return std::to_string(cache.count())+" 页 · "+std::to_string((cache.used()+1023)/1024)+" KiB";}
    bool zoomed()const{return image&&image->descriptor().width*scale>metrics.width+1;}
    bool vertical()const{return cache.mode==1||(cache.mode==0&&book.long_strip);}
    void release_page(){image.reset();if(canvas.mounted()){draw.reset();auto box=logical(viewport());draw.rect(box.x,box.y,box.width,box.height,0x141916ff);(void)canvas.present(draw);}dirty=true;}
    std::string pending_cache_key,pending_cache_file;bool image_finishing=false;
    void queue_image_cache(std::string key,std::string file){if(cache.enabled&&cache.limit){pending_cache_key=std::move(key);pending_cache_file=std::move(file);}}
    Task<void> finish_image_cache(){
        if(pending_cache_file.empty())co_return Result<void>{};
        auto r=co_await cache_file(pending_cache_key,pending_cache_file);pending_cache_key.clear();pending_cache_file.clear();co_return std::move(r);
    }
    Task<void> write_cache_index(){
        auto s=cache.save();auto f=co_await ctx->fs().open("reader-cache.tmp",OpenMode::write|OpenMode::create|OpenMode::truncate);if(!f)co_return std::unexpected(f.error());
        auto wrote=co_await write_chunks(*f,std::as_bytes(std::span{s.data(),s.size()}));if(!wrote)co_return std::unexpected(wrote.error());
        auto c=f->close();if(!c)co_return std::unexpected(c.error());co_return co_await ctx->fs().replace("reader-cache.tmp","reader-cache.json");
    }
    Task<void> load_cache(){
        auto f=co_await ctx->fs().open("reader-cache.json");if(f){std::string text;for(;;){auto n=f->read(buffer);if(!n||text.size()+*n>cache.index_max)break;if(!*n){(void)cache.restore(text);break;}text.append(reinterpret_cast<const char*>(buffer.data()),*n);} (void)f->close();}
        for(unsigned i=0;i<cache.slots;++i){if(cache.pages[i].key.empty())continue;auto info=co_await ctx->fs().stat(cache_path(i));if(info&&info->size==cache.pages[i].bytes)continue;cache.pages[i]={};auto r=co_await ctx->fs().remove(cache_path(i));(void)r;}
        // Query the installation's volume without writing a capacity probe.
        auto reserved=co_await reserve_cache_space();(void)reserved;
        (void)ctx->log().write(LogLevel::info,"MANGA cache_active_limit="+std::to_string(cache.limit));
        co_return Result<void>{};
    }
    Task<void> clear_cache_files(){
        for(auto& p:buffered)p.asset.reset();waiting_scroll=0;
        for(unsigned i=0;i<cache.slots;++i){auto r=co_await ctx->fs().remove(cache_path(i));if(!r&&r.error()!=Error::not_found)co_return std::unexpected(r.error());cache.pages[i]={};}
        auto chapter=co_await ctx->fs().remove("reader-chapter.json");(void)chapter;chapter=co_await ctx->fs().remove("reader-reserve.tmp");(void)chapter;cache.limit=0;cache.storage_verified=false;cache_prepare=true;auto written=co_await write_cache_index();if(!written)co_return std::unexpected(written.error());busy=false;message("缓存已清除，书架和进度已保留");co_return Result<void>{};
    }
    Task<void> reserve_cache_space(){
        auto removed=co_await ctx->fs().remove("reader-reserve.tmp");(void)removed;
        cache.limit=0;cache.storage_verified=false;if(!cache.enabled)co_return Result<void>{};
        // The fixed media root follows app placement. Only query its volume;
        // images and indexes remain private and are removed on uninstall.
        auto root=co_await ctx->fs().app_media();if(!root)co_return Result<void>{};
        auto info=co_await root->info();(void)root->close();
        if(info){cache.budget(info->free_bytes,(info->space_known&2)!=0);cache.storage_verified=cache.limit!=0;}
        co_return Result<void>{};
    }
    Task<void> storage_pressure(){
        auto r=co_await ctx->fs().remove("reader-reserve.tmp");(void)r;cache.limit=0;cache.storage_verified=false;
        unsigned freed=0;while(freed<4*1024*1024){int slot=cache.oldest();if(slot<0)break;r=co_await ctx->fs().remove(cache_path(slot));if(!r&&r.error()!=Error::not_found)break;freed+=cache.pages[slot].bytes;cache.pages[slot]={};}
        r=co_await write_cache_index();(void)r;co_return Result<void>{};
    }
    Task<void> cache_file(std::string key,std::string file){
        if(!cache.enabled||!cache.limit||key.size()>1024)co_return Result<void>{};auto info=co_await ctx->fs().stat(file);if(!info||!info->size||info->size>524288||info->size>cache.limit)co_return Result<void>{};
        while(cache.used()+info->size>cache.limit||cache.vacant()<0){int i=cache.oldest();if(i<0)break;auto r=co_await ctx->fs().remove(cache_path(i));if(!r&&r.error()!=Error::not_found)co_return Result<void>{};cache.pages[i]={};}
        int slot=cache.vacant();if(slot<0)co_return Result<void>{};auto invalidated=co_await write_cache_index();if(!invalidated)co_return Result<void>{};auto moved=co_await ctx->fs().replace(file,cache_path(slot));if(moved){cache.pages[slot]={std::move(key),unsigned(info->size),++cache.clock};auto saved=co_await write_cache_index();if(!saved)(void)ctx->log().write(LogLevel::warning,"MANGA cache index write failed");}else{auto reclaimed=co_await storage_pressure();(void)reclaimed;}co_return Result<void>{};
    }
    Task<Asset> decode_file(std::string path,bool foreground,ImageFormats formats){
        if(foreground)release_page();
        for(unsigned pixels=foreground?image_pixels():std::min(image_pixels(),std::max(65536u,(buffer_limit>65536?buffer_limit-65536:0u)/4));pixels>=65536;pixels/=2){auto file=co_await ctx->fs().open(path);if(!file)co_return std::unexpected(file.error());
            auto a=co_await ctx->assets().decode_image(std::move(*file),{.max_width=std::uint16_t(image_width()),.max_height=2048,.max_pixels=pixels,.max_encoded_bytes=524288,.formats=formats});
            if(a){auto d=a->descriptor();(void)ctx->log().write(LogLevel::info,"MANGA image="+std::to_string(d.width)+"x"+std::to_string(d.height)+" source="+std::to_string(d.source_width)+"x"+std::to_string(d.source_height)+" resident="+std::to_string(d.resident_bytes));co_return std::move(a);}
            if(a.error()!=Error::resource_limit&&a.error()!=Error::quota_exceeded)co_return std::unexpected(a.error());
        }co_return std::unexpected(Error::resource_limit);
    }
    Task<Asset> cached_image(std::string key,bool foreground=true){
        int slot=cache.find(key);if(slot<0)co_return std::unexpected(Error::not_found);auto f=co_await ctx->fs().open(cache_path(slot));if(!f){cache.pages[slot]={};co_return std::unexpected(Error::not_found);}
        (void)f->close();auto a=co_await decode_file(cache_path(slot),foreground,ImageFormats::png|ImageFormats::jpeg);
        if(a){cache.touch(slot);(void)ctx->log().write(LogLevel::info,"MANGA cache=hit");co_return std::move(a);}if(a.error()==Error::resource_limit||a.error()==Error::quota_exceeded)co_return std::unexpected(a.error());cache.pages[slot]={};auto r=co_await ctx->fs().remove(cache_path(slot));(void)r;co_return std::unexpected(Error::not_found);
    }
    bool download_storage_failed=false;
    Task<void> download_image(std::string url,int following=-1){download_storage_failed=false;sync_clock();auto started=now_us;
        auto chunk=std::make_unique<std::array<std::byte,16384>>();
        for(unsigned attempt=0;attempt<2;++attempt){std::optional<HttpResponse> response;
            if(!attempt&&wire_url==url){while(wire_running){auto y=co_await ctx->clock().yield();if(!y)co_return std::unexpected(y.error());}if(wire_ready){if(wire_response)response.emplace(std::move(*wire_response));wire_response.reset();source_bytes=std::move(wire_bytes);wire_ready=false;wire_url.clear();if(!response){if(source_bytes.empty())co_return std::unexpected(Error::protocol_error);if(following>=0)schedule_wire(following);(void)ctx->log().write(LogLevel::info,"MANGA wire_consumed_bytes="+std::to_string(source_bytes.size()));co_return Result<void>{};}}}
            if(!response){source_bytes.clear();auto opened=co_await request(url,4*1024*1024,manga::referrer(book));if(!opened){if(!attempt&&transient(opened.error()))continue;co_return std::unexpected(opened.error());}response.emplace(std::move(*opened));}
            // Start the next request as soon as this response has headers,
            // overlapping its latency/body with the current transfer too.
            if(following>=0)schedule_wire(following);
            unsigned received=source_bytes.size();std::optional<File> file;std::optional<Error> failed;
            for(;;){auto n=co_await response->body.read_some(*chunk);if(!n){failed=n.error();break;}if(!*n)break;received+=*n;if(received>4*1024*1024)co_return std::unexpected(Error::limit_exceeded);
                if(!file&&source_bytes.size()+*n<=524288)source_bytes.insert(source_bytes.end(),chunk->begin(),chunk->begin()+*n);
                else{if(!file){auto opened=co_await ctx->fs().open("manga-source.tmp",OpenMode::write|OpenMode::create|OpenMode::truncate);if(!opened){download_storage_failed=true;co_return std::unexpected(opened.error());}file.emplace(std::move(*opened));auto wrote=co_await write_chunks(*file,source_bytes);if(!wrote){download_storage_failed=true;co_return std::unexpected(wrote.error());}std::vector<std::byte>{}.swap(source_bytes);}
                    auto wrote=co_await write_chunks(*file,std::span{*chunk}.first(*n));if(!wrote){download_storage_failed=true;co_return std::unexpected(wrote.error());}}
            }
            if(file){auto closed=file->close();if(!closed){download_storage_failed=true;co_return std::unexpected(closed.error());}}
            if(failed){if(!attempt&&transient(*failed))continue;co_return std::unexpected(*failed);}if(!received)co_return std::unexpected(Error::protocol_error);sync_clock();(void)ctx->log().write(LogLevel::info,"MANGA input_ready_bytes="+std::to_string(received)+" ram="+std::to_string(!file)+" elapsed_us="+std::to_string(now_us-started));co_return Result<void>{};
        }co_return std::unexpected(Error::io_error);
    }
    Task<Asset> decoded_download(std::string key,bool foreground=true){
        std::array<std::byte,12> header{};std::size_t count=0;
        if(!source_bytes.empty()){count=std::min(header.size(),source_bytes.size());std::copy_n(source_bytes.begin(),count,header.begin());}
        else{auto file=co_await ctx->fs().open("manga-source.tmp");if(!file)co_return std::unexpected(file.error());auto n=file->read(header);if(!n)co_return std::unexpected(n.error());count=*n;(void)file->close();}
        bool webp=count>=12&&std::memcmp(header.data(),"RIFF",4)==0&&std::memcmp(header.data()+8,"WEBP",4)==0;bool jpeg=count>=2&&header[0]==std::byte{255}&&header[1]==std::byte{216};
        if(!webp&&!source_bytes.empty()){auto written=co_await spool_source();if(!written)co_return std::unexpected(written.error());}
        bool native=false;if(!webp){auto stat=co_await ctx->fs().stat("manga-source.tmp");native=stat&&stat->size<=524288;}
        if(native){auto a=co_await decode_file("manga-source.tmp",foreground,ImageFormats::png|ImageFormats::jpeg);if(a||a.error()==Error::resource_limit||a.error()==Error::quota_exceeded){if(a)queue_image_cache(key,"manga-source.tmp");std::vector<std::byte>{}.swap(source_bytes);co_return std::move(a);}if(!jpeg||(a.error()!=Error::unsupported&&a.error()!=Error::limit_exceeded))co_return std::unexpected(a.error());}
        auto a=co_await convert_download(foreground);if(a)queue_image_cache(std::move(key),"manga-image.tmp");co_return std::move(a);
    }
    Task<bool> chapter_cache(){
        if(refresh_chapter){refresh_chapter=false;co_return false;}auto f=co_await ctx->fs().open("reader-chapter.json");if(!f)co_return false;std::string text;
        for(;;){auto n=f->read(buffer);if(!n||text.size()+*n>98304)co_return false;if(!*n)break;text.append(reinterpret_cast<const char*>(buffer.data()),*n);}manga::Json j(text);if(!j.valid()||j.get("book").string().value_or("")!=book.id||j.get("chapter").string().value_or("")!=book.chapter)co_return false;
        std::vector<std::string> next,backup;bool good=true;bool valid=j.get("pages").each([&](auto row){auto u=row.string();if(!u||!manga::web(*u)){good=false;return false;}next.push_back(std::move(*u));return true;},512);
        bool valid_backup=j.get("originals").each([&](auto row){auto u=row.string();if(!u||(!u->empty()&&!manga::web(*u))){good=false;return false;}backup.push_back(std::move(*u));return true;},512);
        if(!valid||!valid_backup||!good||next.empty()||(!backup.empty()&&backup.size()!=next.size()))co_return false;pages=std::move(next);originals=std::move(backup);chapter_from_cache=true;(void)ctx->log().write(LogLevel::info,"MANGA chapter_cache=hit");co_return true;
    }
    Task<void> store_chapter(){
        if(!cache.enabled||!cache.limit)co_return Result<void>{};std::string s="{\"book\":"+manga::quote(book.id)+",\"chapter\":"+manga::quote(book.chapter)+",\"pages\":[";
        bool first=true;for(auto& u:pages){if(!first)s+=',';first=false;s+=manga::quote(u);if(s.size()>96000)co_return Result<void>{};}s+="],\"originals\":[";first=true;for(auto& u:originals){if(!first)s+=',';first=false;s+=manga::quote(u);if(s.size()>98000)co_return Result<void>{};}s+="]}";
        auto f=co_await ctx->fs().open("reader-chapter.tmp",OpenMode::write|OpenMode::create|OpenMode::truncate);if(!f)co_return Result<void>{};auto bytes=std::as_bytes(std::span{s.data(),s.size()});while(!bytes.empty()){auto n=f->write(bytes.first(std::min<size_t>(4096,bytes.size())));if(!n||!*n)co_return Result<void>{};bytes=bytes.subspan(*n);}if(!f->close())co_return Result<void>{};auto moved=co_await ctx->fs().replace("reader-chapter.tmp","reader-chapter.json");(void)moved;co_return Result<void>{};
    }
