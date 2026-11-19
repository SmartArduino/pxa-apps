    manga::DirectoryCache directory_cache;
    bool contents_from_reader=false;
    std::string directory_key()const {
        std::string group;
        if(book.source=="copymanga"&&!groups.empty()){
            unsigned default_index=0;for(unsigned i=0;i<groups.size();++i)if(groups[i].id=="default")default_index=i;
            if(group_index!=default_index)group=groups[group_index].id;
        }
        return "["+manga::quote(book.source)+","+manga::quote(book.id)+","+std::to_string(language)+","+
            std::to_string(descending)+","+std::to_string(offset)+","+manga::quote(group)+"]";
    }
    std::string directory_payload()const {
        std::string s="{\"total\":"+std::to_string(total)+",\"groupIndex\":"+std::to_string(group_index);
        auto add=[&](std::string_view name,const auto& rows){s+=",\"";s+=name;s+="\":[";bool first=true;
            for(auto& row:rows){if(!first)s+=',';first=false;s+="{\"id\":"+manga::quote(row.id)+",\"title\":"+manga::quote(row.title)+'}';}s+=']';};
        add("chapters",chapters);add("groups",groups);return s+'}';
    }
    bool restore_directory(std::string_view payload){
        manga::Json j(payload);if(!j.valid())return false;std::vector<manga::Chapter> next,next_groups;bool good=true;
        auto rows=[&](manga::Json rows,auto& target,bool grouping,unsigned limit){return rows.each([&](manga::Json row){
            auto id=row.get("id").string(1024),title=row.get("title").string(192);auto candidate=book;
            if(id)candidate.chapter=*id;
            if(!id||!title||title->empty()||(grouping?!manga::slug(*id):!manga::valid_book(candidate)))return good=false;
            target.push_back({std::move(*id),std::move(*title)});return true;
        },limit);};
        if(!rows(j.get("chapters"),next,false,4096)||!rows(j.get("groups"),next_groups,true,32)||!good)return false;
        auto count=j.get("total").number(1000001),group=j.get("groupIndex").number(33);
        if(count>1000000||count<next.size()||(next_groups.empty()?group!=0:group>=next_groups.size()))return false;
        chapters=std::move(next);groups=std::move(next_groups);total=count;group_index=group;list_start=0;return true;
    }
    Task<void> load_directory_cache(){
        auto file=co_await open_file("reader-directory.json");if(!file)co_return Result<void>{};std::string s;
        for(;;){auto n=file->read(buffer);if(!n||s.size()+*n>manga::DirectoryCache::file_maximum)co_return Result<void>{};if(!*n)break;s.append(reinterpret_cast<const char*>(buffer.data()),*n);}
        (void)file->close();(void)directory_cache.restore(s);co_return Result<void>{};
    }
    Task<void> store_directory_cache(){
        // Directory metadata is bounded independently of the much larger image
        // budget. A failed optional write keeps the in-memory directory usable.
        if(!cache.enabled)co_return Result<void>{};auto s=directory_cache.save();
        if(s.size()>manga::DirectoryCache::file_maximum)co_return Result<void>{};
        auto stored=co_await write_file("reader-directory.json",std::as_bytes(std::span{s.data(),s.size()}));(void)stored;co_return Result<void>{};
    }
    Task<void> load_chapters(bool refresh=false){
        auto key=directory_key();
        if(!refresh){auto payload=directory_cache.get(key);if(payload&&restore_directory(*payload)){
            busy=screen==reading;message(std::to_string(total)+" 个章节 · 已缓存");
            (void)ctx->log().write(LogLevel::info,"MANGA directory_cache=hit");co_return Result<void>{};}}
        auto loaded=co_await load_chapters_network();if(!loaded)co_return std::unexpected(loaded.error());
        busy=true;
        if(directory_cache.put(std::move(key),directory_payload())){auto stored=co_await store_directory_cache();(void)stored;}
        busy=screen==reading;changed();
        co_return Result<void>{};
    }
    Task<void> read_first_chapter(){
        auto loaded=co_await load_chapters();if(!loaded)co_return std::unexpected(loaded.error());
        if(chapters.empty()){screen=contents;busy=false;message("暂无可阅读章节");co_return Result<void>{};}
        book.chapter=chapters.front().id;book.chapter_title=chapters.front().title;book.page=0;
        co_return co_await load_pages();
    }
