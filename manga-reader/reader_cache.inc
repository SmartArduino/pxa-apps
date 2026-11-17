    Task<void> save_cache_settings(){auto r=co_await write_cache_index();busy=false;message(r?"设置已保存":"设置保存失败");co_return Result<void>{};}
    static std::string cache_path(unsigned slot){return "reader-cache-"+std::to_string(slot)+".png";}
    std::string page_label()const{return pages.empty()?"暂无页码":"第 "+std::to_string(page+1)+" / "+std::to_string(pages.size())+" 页";}
    std::string cache_label()const{return std::to_string(cache.count())+" 页 · "+std::to_string((cache.used()+1023)/1024)+" KiB";}
    bool zoomed()const{return image&&image->descriptor().width*scale>metrics.width+1;}
    bool vertical()const{return cache.mode==1||(cache.mode==0&&image&&image->descriptor().height>image->descriptor().width*1.8f);}
    void release_page(){image.reset();if(canvas.mounted()){draw.reset();auto box=logical(viewport());draw.rect(box.x,box.y,box.width,box.height,0x141916ff);(void)canvas.present(draw);}dirty=true;}
    Task<void> write_cache_index(){
        auto s=cache.save();auto f=co_await ctx->fs().open("reader-cache.tmp",OpenMode::write|OpenMode::create|OpenMode::truncate);if(!f)co_return std::unexpected(f.error());
        auto bytes=std::as_bytes(std::span{s.data(),s.size()});while(!bytes.empty()){auto n=f->write(bytes.first(std::min<size_t>(4096,bytes.size())));if(!n||!*n)co_return std::unexpected(n?Error::io_error:n.error());bytes=bytes.subspan(*n);}
        auto c=f->close();if(!c)co_return std::unexpected(c.error());co_return co_await ctx->fs().replace("reader-cache.tmp","reader-cache.json");
    }
    Task<void> load_cache(){
        auto f=co_await ctx->fs().open("reader-cache.json");if(f){std::string text;for(;;){auto n=f->read(buffer);if(!n||text.size()+*n>49152)break;if(!*n){(void)cache.restore(text);break;}text.append(reinterpret_cast<const char*>(buffer.data()),*n);} (void)f->close();}
        for(unsigned i=0;i<cache.slots;++i){auto info=co_await ctx->fs().stat(cache_path(i));if(!cache.pages[i].key.empty()&&info&&info->size==cache.pages[i].bytes)continue;cache.pages[i]={};auto r=co_await ctx->fs().remove(cache_path(i));(void)r;}
        // Use only the application's private FS. A bounded write probe also
        // works on Hosts that do not report removable-volume space/features.
        if(cache.storage_verified)cache.limit=cache.maximum;
        else {auto probe=co_await ctx->fs().open("reader-probe.tmp",OpenMode::write|OpenMode::create|OpenMode::truncate);bool writable=bool(probe);
            if(probe){for(unsigned i=0;i<64&&writable;++i){auto bytes=std::span<const std::byte>{buffer};while(!bytes.empty()){auto n=probe->write(bytes);if(!n||!*n){writable=false;break;}bytes=bytes.subspan(*n);}auto y=co_await ctx->clock().yield();if(!y)writable=false;}(void)probe->close();}
            auto removed=co_await ctx->fs().remove("reader-probe.tmp");(void)removed;if(writable){cache.limit=cache.maximum;cache.storage_verified=true;auto saved=co_await write_cache_index();(void)saved;}
        }
        (void)ctx->log().write(LogLevel::info,"MANGA cache_active_limit="+std::to_string(cache.limit));
        co_return Result<void>{};
    }
    Task<void> clear_cache_files(){
        for(unsigned i=0;i<cache.slots;++i){auto r=co_await ctx->fs().remove(cache_path(i));if(!r&&r.error()!=Error::not_found)co_return std::unexpected(r.error());cache.pages[i]={};}
        auto chapter=co_await ctx->fs().remove("reader-chapter.json");(void)chapter;auto written=co_await write_cache_index();if(!written)co_return std::unexpected(written.error());busy=false;message("缓存已清除，书架和进度已保留");co_return Result<void>{};
    }
    Task<void> cache_file(std::string key,std::string file){
        if(!cache.enabled||!cache.limit||key.size()>1024)co_return Result<void>{};auto info=co_await ctx->fs().stat(file);if(!info||!info->size||info->size>524288||info->size>cache.limit)co_return Result<void>{};
        while(cache.used()+info->size>cache.limit||cache.vacant()<0){int i=cache.oldest();if(i<0)break;auto r=co_await ctx->fs().remove(cache_path(i));if(!r&&r.error()!=Error::not_found)co_return Result<void>{};cache.pages[i]={};}
        int slot=cache.vacant();if(slot<0)co_return Result<void>{};auto invalidated=co_await write_cache_index();if(!invalidated)co_return Result<void>{};auto moved=co_await ctx->fs().replace(file,cache_path(slot));if(moved){cache.pages[slot]={std::move(key),unsigned(info->size),++cache.clock};auto saved=co_await write_cache_index();if(!saved)(void)ctx->log().write(LogLevel::warning,"MANGA cache index write failed");}co_return Result<void>{};
    }
    Task<Asset> cached_image(std::string key){
        int slot=cache.find(key);if(slot<0)co_return std::unexpected(Error::not_found);auto f=co_await ctx->fs().open(cache_path(slot));if(!f){cache.pages[slot]={};co_return std::unexpected(Error::not_found);}release_page();
        auto a=co_await ctx->assets().decode_image(std::move(*f),{.max_width=512,.max_height=2048,.max_pixels=262144,.max_encoded_bytes=524288,.formats=ImageFormats::png|ImageFormats::jpeg});
        if(a){cache.touch(slot);(void)ctx->log().write(LogLevel::info,"MANGA cache=hit");co_return std::move(a);}cache.pages[slot]={};auto r=co_await ctx->fs().remove(cache_path(slot));(void)r;co_return std::unexpected(Error::not_found);
    }
    Task<void> download_image(std::string url){
        for(unsigned attempt=0;attempt<2;++attempt){auto response=co_await request(url,4*1024*1024,manga::referrer(book));if(!response){if(!attempt&&transient(response.error()))continue;co_return std::unexpected(response.error());}
            auto f=co_await ctx->fs().open("manga-source.tmp",OpenMode::write|OpenMode::create|OpenMode::truncate);if(!f)co_return std::unexpected(f.error());unsigned received=0;std::optional<Error> failed;
            for(;;){auto n=co_await response->body.read_some(buffer);if(!n){failed=n.error();break;}if(!*n)break;received+=*n;if(received>4*1024*1024)co_return std::unexpected(Error::limit_exceeded);auto bytes=std::span{buffer}.first(*n);while(!bytes.empty()){auto w=f->write(bytes);if(!w||!*w)co_return std::unexpected(w?Error::io_error:w.error());bytes=bytes.subspan(*w);}}
            auto closed=f->close();if(!closed)co_return std::unexpected(closed.error());if(failed){if(!attempt&&transient(*failed))continue;co_return std::unexpected(*failed);}co_return Result<void>{};
        }co_return std::unexpected(Error::io_error);
    }
    Task<Asset> decoded_download(std::string key){
        auto f=co_await ctx->fs().open("manga-source.tmp");if(!f)co_return std::unexpected(f.error());std::array<std::byte,12> header{};auto n=f->read(header);if(!n)co_return std::unexpected(n.error());
        bool webp=*n>=12&&std::memcmp(header.data(),"RIFF",4)==0&&std::memcmp(header.data()+8,"WEBP",4)==0;bool jpeg=*n>=2&&header[0]==std::byte{255}&&header[1]==std::byte{216};(void)f->close();
        auto stat=co_await ctx->fs().stat("manga-source.tmp");bool native=!webp&&stat&&stat->size<=524288;
        if(jpeg&&native){auto input=co_await ctx->fs().open("manga-source.tmp");if(input){manga::JpegHeader size;auto read=[](void* p,std::span<std::byte> out)->size_t{auto n=static_cast<File*>(p)->read(out);return n?*n:0;};if(manga::jpeg_header(read,&*input,size)&&std::uint64_t(size.width)*size.height>262144)native=false;}}
        if(native){auto input=co_await ctx->fs().open("manga-source.tmp");if(!input)co_return std::unexpected(input.error());release_page();auto a=co_await ctx->assets().decode_image(std::move(*input),{.max_width=512,.max_height=2048,.max_pixels=262144,.max_encoded_bytes=524288,.formats=ImageFormats::png|ImageFormats::jpeg});if(a){auto stored=co_await cache_file(key,"manga-source.tmp");(void)stored;co_return std::move(a);}if(!jpeg||(a.error()!=Error::unsupported&&a.error()!=Error::limit_exceeded))co_return std::unexpected(a.error());}
        auto a=co_await convert_download();if(a){auto stored=co_await cache_file(std::move(key),"manga-image.tmp");(void)stored;}co_return std::move(a);
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
