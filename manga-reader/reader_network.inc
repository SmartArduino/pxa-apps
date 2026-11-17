    std::vector<std::byte> source_bytes,wire_bytes;
    std::optional<HttpResponse> wire_response;std::string wire_url;
    bool wire_running=false,wire_ready=false;std::uint64_t wire_generation=0;
    void clear_wire(){++wire_generation;wire_response.reset();wire_url.clear();wire_running=wire_ready=false;std::vector<std::byte>{}.swap(wire_bytes);std::vector<std::byte>{}.swap(source_bytes);}
    Task<void> write_chunks(File& file,std::span<const std::byte> bytes){
        while(!bytes.empty()){auto tick=profile_short_tick();for(unsigned n=0;n<8&&!bytes.empty();++n){auto wrote=file.write(bytes.first(std::min<std::size_t>(4096,bytes.size())));if(!wrote||!*wrote)co_return std::unexpected(wrote?Error::io_error:wrote.error());bytes=bytes.subspan(*wrote);if(profile_short_us(tick,profile_short_tick())>=4000)break;}auto y=co_await ctx->clock().yield();if(!y)co_return std::unexpected(y.error());}co_return Result<void>{};
    }
    Task<void> spool_source(){auto file=co_await ctx->fs().open("manga-source.tmp",OpenMode::write|OpenMode::create|OpenMode::truncate);if(!file)co_return std::unexpected(file.error());auto r=co_await write_chunks(*file,source_bytes);if(!r)co_return std::unexpected(r.error());co_return file->close();}
    Task<void> warm_network(std::string url,std::uint64_t generation,unsigned index){
        struct End {Reader* app;std::uint64_t generation;~End(){if(app->wire_generation==generation)app->wire_running=false;}} end{this,generation};
        auto r=co_await request(url,4*1024*1024,manga::referrer(book));if(!r)co_return Result<void>{};
        auto chunk=std::make_unique<std::array<std::byte,16384>>();wire_bytes.clear();bool complete=false;
        while(wire_bytes.size()<262144){auto n=co_await r->body.read_some(std::span{*chunk}.first(std::min<std::size_t>(chunk->size(),262144-wire_bytes.size())));if(!n){wire_bytes.clear();co_return Result<void>{};}if(!*n){complete=true;break;}wire_bytes.insert(wire_bytes.end(),chunk->begin(),chunk->begin()+*n);}
        if(wire_generation==generation){if(!complete)wire_response.emplace(std::move(*r));wire_ready=true;(void)ctx->log().write(LogLevel::info,"MANGA wire_prefetched="+std::to_string(index)+" bytes="+std::to_string(wire_bytes.size()));}co_return Result<void>{};
    }
    void schedule_wire(unsigned index){
        if(index>=pages.size()||wire_running||wire_ready||manga::source_kind(book.source)==manga::SourceKind::town)return;
        auto key=manga::page_cache_key(book,index);if(!key.empty())key+=":rgb3";else key=pages[index];if(cache.find(key)>=0)return;
        wire_url=pages[index];wire_ready=false;wire_running=true;auto generation=++wire_generation;
        auto r=ctx->foreground_tasks().start(warm_network(wire_url,generation,index));if(!r){wire_running=false;wire_url.clear();}
    }
