    Task<void> save_cache_settings(){auto r=co_await write_cache_index();busy=false;message(r?"设置已保存":"设置保存失败");co_return Result<void>{};}
    static std::string cache_path(unsigned slot){return "reader-cache-"+std::to_string(slot)+".png";}
    std::string page_label()const{return pages.empty()?"暂无页码":"第 "+std::to_string(page+1)+" / "+std::to_string(pages.size())+" 页";}
    std::string cache_label()const{return std::to_string(cache.count())+" 页 · "+std::to_string((cache.used()+1023)/1024)+" KiB";}
    bool zoomed()const{return image&&image->descriptor().width*scale>metrics.width+1;}
    bool vertical()const{return book.reading_mode==1||(book.reading_mode==0&&book.long_strip);}
    bool book_settings()const{return settings_prior==reading||(settings_prior==help&&prior==reading);}
    void restore_view(){zoom_mode=book.view_mode;custom_zoom=book.zoom_permille/1000.f;}
    void remember_view(){
        if(!manga::valid_book(book))return;book.view_mode=zoom_mode;book.zoom_permille=std::clamp(unsigned(std::round(custom_zoom*1000)),1u,4000u);
        auto found=std::find_if(shelf.begin(),shelf.end(),[&](auto& b){return manga::same(b,book);});
        if(found!=shelf.end())*found=book;else if(shelf.size()<24)shelf.insert(shelf.begin(),book);save();
    }
    void release_page(){image.reset();if(canvas.mounted()){draw.reset();auto box=logical(viewport());draw.rect(box.x,box.y,box.width,box.height,0x141916ff);(void)canvas.present(draw);}dirty=true;}
    std::string pending_cache_key,pending_cache_file;unsigned pending_cache_bytes=0,download_encoded_bytes=0,converted_encoded_bytes=0;bool image_finishing=false;
    void queue_image_cache(std::string key,std::string file,unsigned bytes){if(cache.enabled&&cache.limit){pending_cache_key=std::move(key);pending_cache_file=std::move(file);pending_cache_bytes=bytes;}}
    Task<void> finish_image_cache(){
        if(pending_cache_file.empty())co_return Result<void>{};
        auto r=co_await cache_file(pending_cache_key,pending_cache_file,pending_cache_bytes);pending_cache_key.clear();pending_cache_file.clear();co_return std::move(r);
    }
    Task<void> write_cache_index(){
        auto pause=co_await metadata_pause();if(!pause)co_return std::unexpected(pause.error());auto s=cache.save();co_return co_await write_file("reader-cache.json",std::as_bytes(std::span{s.data(),s.size()}));
    }
    Task<void> load_cache(){
        auto f=co_await open_file("reader-cache.json");if(f){std::string text;for(;;){auto n=f->read(buffer);if(!n||text.size()+*n>cache.index_max)break;if(!*n){(void)cache.restore(text);break;}text.append(reinterpret_cast<const char*>(buffer.data()),*n);} (void)f->close();}
        // Validate cache files on use. Opening/stat-ing every saved page here
        // can block the runtime for seconds on an SD-backed private volume.
        // Query the installation's volume without writing a capacity probe.
        auto reserved=co_await reserve_cache_space();(void)reserved;
        (void)ctx->log().write(LogLevel::info,"MANGA cache_active_limit="+std::to_string(cache.limit));
        co_return Result<void>{};
    }
    Task<void> clear_cache_files(){
        for(auto& p:buffered)p.asset.reset();waiting_scroll=0;
        for(unsigned i=0;i<9;++i){auto part=co_await remove_file(source_part(i));(void)part;part=co_await remove_file(source_part(i)+".pending");(void)part;}
        for(auto path:{"manga-source.tmp","manga-source.tmp.pending","manga-image.tmp","manga-image.tmp.pending"}){auto part=co_await remove_file(path);(void)part;}
        directory_cache.entries.clear();auto directory=co_await remove_file("reader-directory.json");(void)directory;directory=co_await remove_file("reader-directory.tmp");(void)directory;
        for(unsigned i=0;i<cache.slots;++i){auto r=co_await remove_file(cache_path(i));if(!r&&r.error()!=Error::not_found)co_return std::unexpected(r.error());cache.pages[i]={};}
        auto chapter=co_await remove_file("reader-chapter.json");(void)chapter;chapter=co_await remove_file("reader-reserve.tmp");(void)chapter;cache.limit=0;cache.storage_verified=false;cache_prepare=true;auto written=co_await write_cache_index();if(!written)co_return std::unexpected(written.error());busy=false;message("缓存已清除，书架和进度已保留");co_return Result<void>{};
    }
    Task<void> reserve_cache_space(){
        auto removed=co_await remove_file("reader-reserve.tmp");(void)removed;
        cache.limit=0;cache.storage_verified=false;if(!cache.enabled)co_return Result<void>{};
        // The fixed media root follows app placement. Only query its volume;
        // images and indexes remain private and are removed on uninstall.
        auto root=co_await ctx->fs().app_media();if(!root)co_return Result<void>{};
        auto info=co_await root->info();(void)root->close();
        if(info){cache.budget(info->free_bytes,(info->space_known&2)!=0);cache.storage_verified=cache.limit!=0;}
        co_return Result<void>{};
    }
    Task<void> storage_pressure(){
        auto r=co_await remove_file("reader-reserve.tmp");(void)r;cache.limit=0;cache.storage_verified=false;
        unsigned freed=0;while(freed<4*1024*1024){int slot=cache.oldest();if(slot<0)break;r=co_await remove_file(cache_path(slot));if(!r&&r.error()!=Error::not_found)break;freed+=cache.pages[slot].bytes;cache.pages[slot]={};}
        r=co_await write_cache_index();(void)r;co_return Result<void>{};
    }
    Task<void> cache_file(std::string key,std::string file,unsigned encoded_bytes){
        if(!cache.enabled||!cache.limit||key.size()>1024||!encoded_bytes||encoded_bytes>524288||encoded_bytes>cache.limit)co_return Result<void>{};auto pause=co_await metadata_pause();if(!pause)co_return std::unexpected(pause.error());
        while(cache.used()+encoded_bytes>cache.limit||cache.vacant()<0){int i=cache.oldest();if(i<0)break;auto r=co_await remove_file(cache_path(i));if(!r&&r.error()!=Error::not_found)co_return Result<void>{};cache.pages[i]={};}
        int slot=cache.vacant();if(slot<0)co_return Result<void>{};auto invalidated=co_await write_cache_index();if(!invalidated)co_return Result<void>{};auto moved=co_await replace_file(file,cache_path(slot));if(moved){cache.pages[slot]={std::move(key),encoded_bytes,++cache.clock};auto saved=co_await write_cache_index();if(!saved)(void)ctx->log().write(LogLevel::warning,"MANGA cache index write failed");}else{auto reclaimed=co_await storage_pressure();(void)reclaimed;}co_return Result<void>{};
    }
    Task<Asset> decode_file(std::string path,bool foreground,ImageFormats formats){
        if(foreground)release_page();
        auto pause=co_await metadata_pause();if(!pause)co_return std::unexpected(pause.error());
        for(unsigned pixels=foreground?image_pixels():std::min(image_pixels(),std::max(65536u,(buffer_limit>65536?buffer_limit-65536:0u)/4));pixels>=65536;pixels/=2){auto file=co_await open_file(path);if(!file)co_return std::unexpected(file.error());
            auto a=co_await ctx->assets().decode_image(std::move(*file),{.max_width=std::uint16_t(image_width()),.max_height=2048,.max_pixels=pixels,.max_encoded_bytes=524288,.formats=formats});
            if(a){auto d=a->descriptor();(void)ctx->log().write(LogLevel::info,"MANGA image="+std::to_string(d.width)+"x"+std::to_string(d.height)+" source="+std::to_string(d.source_width)+"x"+std::to_string(d.source_height)+" resident="+std::to_string(d.resident_bytes));co_return std::move(a);}
            if(a.error()!=Error::resource_limit&&a.error()!=Error::quota_exceeded)co_return std::unexpected(a.error());
        }co_return std::unexpected(Error::resource_limit);
    }
    Task<Asset> cached_image(std::string key,bool foreground=true){
        int slot=cache.find(key);if(slot<0)co_return std::unexpected(Error::not_found);auto a=co_await decode_file(cache_path(slot),foreground,ImageFormats::png|ImageFormats::jpeg);
        if(a){cache.touch(slot);(void)ctx->log().write(LogLevel::info,"MANGA cache=hit");co_return std::move(a);}if(a.error()==Error::resource_limit||a.error()==Error::quota_exceeded)co_return std::unexpected(a.error());cache.pages[slot]={};auto r=co_await remove_file(cache_path(slot));(void)r;co_return std::unexpected(Error::not_found);
    }
    bool download_storage_failed=false;
    static std::string source_part(unsigned n){return "manga-source-part-"+std::to_string(n)+".tmp";}
    Task<void> assemble_source(const std::vector<unsigned>& parts,unsigned total){
        auto output=co_await disk.begin(*ctx,"manga-source.tmp",total);if(!output)co_return std::unexpected(output.error());
        for(unsigned i=0;i<parts.size();++i){auto input=co_await open_file(source_part(i));if(!input)co_return std::unexpected(input.error());
            unsigned remaining=parts[i];while(remaining){auto n=input->read(std::span{buffer}.first(std::min<std::size_t>(remaining,buffer.size())));if(!n||!*n)co_return std::unexpected(n?Error::io_error:n.error());
                auto wrote=co_await output->write(std::span{buffer}.first(*n));if(!wrote)co_return std::unexpected(wrote.error());remaining-=*n;}
            auto closed=input->close();if(!closed)co_return std::unexpected(closed.error());}
        auto committed=co_await output->commit();if(!committed)co_return std::unexpected(committed.error());
        for(unsigned i=0;i<parts.size();++i){auto removed=co_await remove_file(source_part(i));(void)removed;}co_return Result<void>{};
    }
    Task<void> download_image(std::string url,int following=-1){download_storage_failed=false;download_encoded_bytes=0;sync_clock();auto started=now_us;
        auto chunk=std::make_unique<std::array<std::byte,16384>>();
        for(unsigned attempt=0;attempt<2;++attempt){std::optional<HttpResponse> response;
            if(!attempt&&wire_url==url){while(wire_running){auto y=co_await ctx->clock().yield();if(!y)co_return std::unexpected(y.error());}
                if(wire_ready){if(wire_response)response.emplace(std::move(*wire_response));wire_response.reset();source_bytes=std::move(wire_bytes);wire_ready=false;wire_url.clear();
                    if(!response){if(source_bytes.empty())co_return std::unexpected(Error::protocol_error);download_encoded_bytes=source_bytes.size();if(following>=0)schedule_wire(following);(void)ctx->log().write(LogLevel::info,"MANGA wire_consumed_bytes="+std::to_string(source_bytes.size()));co_return Result<void>{};}}}
            if(!response){source_bytes.clear();auto opened=co_await request(url,4*1024*1024,manga::referrer(book));if(!opened){if(!attempt&&transient(opened.error()))continue;co_return std::unexpected(opened.error());}response.emplace(std::move(*opened));}
            if(following>=0)schedule_wire(following);
            unsigned received=source_bytes.size();std::optional<manga::AsyncDisk::Session> file;std::vector<unsigned> parts;std::optional<Error> failed;
            for(;;){auto n=co_await response->body.read_some(*chunk);if(!n){failed=n.error();break;}if(!*n)break;received+=*n;if(received>4*1024*1024)co_return std::unexpected(Error::limit_exceeded);
                if(!file&&source_bytes.size()+*n<=524288){source_bytes.insert(source_bytes.end(),chunk->begin(),chunk->begin()+*n);auto y=co_await yield_work();if(!y)co_return std::unexpected(y.error());}
                else if(!response->body_length){
                    // Unknown-length bodies stay bounded: publish 512 KiB pieces,
                    // then join them asynchronously once the true size is known.
                    auto wrote=co_await write_file(source_part(parts.size()),source_bytes);if(!wrote){download_storage_failed=true;co_return std::unexpected(wrote.error());}
                    parts.push_back(source_bytes.size());source_bytes.clear();source_bytes.insert(source_bytes.end(),chunk->begin(),chunk->begin()+*n);
                }else{
                    if(!file){auto opened=co_await disk.begin(*ctx,"manga-source.tmp",*response->body_length);if(!opened){download_storage_failed=true;co_return std::unexpected(opened.error());}file.emplace(std::move(*opened));
                        auto wrote=co_await file->write(source_bytes);if(!wrote){download_storage_failed=true;co_return std::unexpected(wrote.error());}std::vector<std::byte>{}.swap(source_bytes);}
                    auto wrote=co_await file->write(std::span{*chunk}.first(*n));if(!wrote){download_storage_failed=true;co_return std::unexpected(wrote.error());}
                }
            }
            if(failed){if(!attempt&&transient(*failed))continue;co_return std::unexpected(*failed);}if(!received)co_return std::unexpected(Error::protocol_error);
            bool spooled=bool(file)||!parts.empty();
            if(file){auto closed=co_await file->commit();if(!closed){download_storage_failed=true;co_return std::unexpected(closed.error());}}
            if(!parts.empty()){
                if(!source_bytes.empty()){auto wrote=co_await write_file(source_part(parts.size()),source_bytes);if(!wrote)co_return std::unexpected(wrote.error());parts.push_back(source_bytes.size());std::vector<std::byte>{}.swap(source_bytes);}
                auto joined=co_await assemble_source(parts,received);if(!joined){download_storage_failed=true;co_return std::unexpected(joined.error());}}
            download_encoded_bytes=received;sync_clock();(void)ctx->log().write(LogLevel::info,"MANGA input_ready_bytes="+std::to_string(received)+" ram="+std::to_string(!spooled)+" elapsed_us="+std::to_string(now_us-started));co_return Result<void>{};
        }co_return std::unexpected(Error::io_error);
    }
    Task<Asset> decoded_download(std::string key,bool foreground=true){
        std::array<std::byte,12> header{};std::size_t count=0;
        if(!source_bytes.empty()){count=std::min(header.size(),source_bytes.size());std::copy_n(source_bytes.begin(),count,header.begin());}
        else{auto file=co_await open_file("manga-source.tmp");if(!file)co_return std::unexpected(file.error());auto n=file->read(header);if(!n)co_return std::unexpected(n.error());count=*n;(void)file->close();}
        bool webp=count>=12&&std::memcmp(header.data(),"RIFF",4)==0&&std::memcmp(header.data()+8,"WEBP",4)==0;bool jpeg=count>=2&&header[0]==std::byte{255}&&header[1]==std::byte{216};
        if(!webp&&!source_bytes.empty()){auto written=co_await spool_source();if(!written)co_return std::unexpected(written.error());}
        const bool native=!webp&&download_encoded_bytes&&download_encoded_bytes<=524288;
        if(native){auto a=co_await decode_file("manga-source.tmp",foreground,ImageFormats::png|ImageFormats::jpeg);if(a||a.error()==Error::resource_limit||a.error()==Error::quota_exceeded){if(a)queue_image_cache(key,"manga-source.tmp",download_encoded_bytes);std::vector<std::byte>{}.swap(source_bytes);co_return std::move(a);}if(!jpeg||(a.error()!=Error::unsupported&&a.error()!=Error::limit_exceeded))co_return std::unexpected(a.error());}
        auto a=co_await convert_download(foreground);if(a)queue_image_cache(std::move(key),"manga-image.tmp",converted_encoded_bytes);co_return std::move(a);
    }
    Task<bool> chapter_cache(){
        if(refresh_chapter){refresh_chapter=false;co_return false;}auto f=co_await open_file("reader-chapter.json");if(!f)co_return false;std::string text;
        for(;;){auto n=f->read(buffer);if(!n||text.size()+*n>98304)co_return false;if(!*n)break;text.append(reinterpret_cast<const char*>(buffer.data()),*n);}manga::Json j(text);if(!j.valid()||j.get("book").string().value_or("")!=book.id||j.get("chapter").string().value_or("")!=book.chapter)co_return false;
        std::vector<std::string> next,backup;bool good=true;bool valid=j.get("pages").each([&](auto row){auto u=row.string();if(!u||!manga::web(*u)){good=false;return false;}next.push_back(std::move(*u));return true;},512);
        bool valid_backup=j.get("originals").each([&](auto row){auto u=row.string();if(!u||(!u->empty()&&!manga::web(*u))){good=false;return false;}backup.push_back(std::move(*u));return true;},512);
        if(!valid||!valid_backup||!good||next.empty()||(!backup.empty()&&backup.size()!=next.size()))co_return false;pages=std::move(next);originals=std::move(backup);chapter_from_cache=true;(void)ctx->log().write(LogLevel::info,"MANGA chapter_cache=hit");co_return true;
    }
    Task<void> store_chapter(){
        if(!cache.enabled||!cache.limit)co_return Result<void>{};std::string s="{\"book\":"+manga::quote(book.id)+",\"chapter\":"+manga::quote(book.chapter)+",\"pages\":[";
        bool first=true;for(auto& u:pages){if(!first)s+=',';first=false;s+=manga::quote(u);if(s.size()>96000)co_return Result<void>{};}s+="],\"originals\":[";first=true;for(auto& u:originals){if(!first)s+=',';first=false;s+=manga::quote(u);if(s.size()>98000)co_return Result<void>{};}s+="]}";
        auto stored=co_await write_file("reader-chapter.json",std::as_bytes(std::span{s.data(),s.size()}));(void)stored;co_return Result<void>{};
    }
