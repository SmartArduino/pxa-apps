    bool chapter_requested=false,chapter_loading=false,chapter_last=false,chapter_failed=false,chapter_committed=false;
    struct DirectoryRestore {
        Reader& app;std::vector<manga::Chapter> chapters,groups;unsigned offset,total,list_start,group_index;bool descending;
        explicit DirectoryRestore(Reader& r):app(r),chapters(std::move(r.chapters)),groups(std::move(r.groups)),offset(r.offset),total(r.total),list_start(r.list_start),group_index(r.group_index),descending(r.descending){}
        ~DirectoryRestore(){app.chapters=std::move(chapters);app.groups=std::move(groups);app.offset=offset;app.total=total;app.list_start=list_start;app.group_index=group_index;app.descending=descending;}
    };
    Task<std::optional<manga::Chapter>> following_chapter(){
        // Reuse the current directory before searching paginated catalogues.
        auto current=std::find_if(chapters.begin(),chapters.end(),[&](auto& c){return c.id==book.chapter;});
        if(current!=chapters.end()){
            if(!descending&&current+1!=chapters.end())co_return std::optional<manga::Chapter>{*(current+1)};
            if(descending&&current!=chapters.begin())co_return std::optional<manga::Chapter>{*(current-1)};
        }
        const bool known=current!=chapters.end();const unsigned start=known?(descending?total-offset-chapters.size():offset):0;
        DirectoryRestore restore(*this);groups=restore.groups;group_index=restore.group_index;descending=false;offset=start;total=0;list_start=0;
        bool found=false;
        for(unsigned queries=0;queries<256;++queries){
            auto loaded=co_await load_chapters();busy=true;if(!loaded)co_return std::unexpected(loaded.error());
            for(auto& candidate:chapters){
                if(found)co_return std::optional<manga::Chapter>{candidate};
                found=candidate.id==book.chapter;
            }
            const bool paged=book.source.empty()||book.source=="copymanga";
            if(!paged||offset+chapters.size()>=total){if(!found)co_return std::unexpected(Error::not_found);co_return std::optional<manga::Chapter>{};}
            if(chapters.empty())co_return std::unexpected(Error::protocol_error);
            offset+=chapters.size();
        }
        co_return std::unexpected(Error::limit_exceeded);
    }
    Task<void> load_next_chapter(){
        auto next=co_await following_chapter();
        if(!next){chapter_failed=true;chapter_loading=false;co_return std::unexpected(next.error());}
        if(!*next){chapter_last=true;chapter_loading=false;busy=false;message("没有下一章");(void)ctx->log().write(LogLevel::info,"MANGA next_chapter=end");co_return Result<void>{};}
        struct Restore {
            Reader& app;manga::Book book;std::vector<std::string> pages,originals;unsigned page;bool chapter_cached;
            explicit Restore(Reader& r):app(r),book(r.book),pages(std::move(r.pages)),originals(std::move(r.originals)),page(r.page),chapter_cached(r.chapter_from_cache){}
            ~Restore(){if(!app.chapter_committed){app.book=std::move(book);app.pages=std::move(pages);app.originals=std::move(originals);app.page=page;app.chapter_from_cache=chapter_cached;}app.chapter_loading=false;}
        } restore(*this);
        book.chapter=(**next).id;book.chapter_title=(**next).title;book.page=0;chapter_committed=false;
        message("正在加载下一章…");auto loaded=co_await load_pages(false);
        if(!loaded){chapter_failed=true;co_return std::unexpected(loaded.error());}
        (void)ctx->log().write(LogLevel::info,"MANGA next_chapter=ready");co_return Result<void>{};
    }
    void next_chapter(){
        if(chapter_loading||chapter_last||busy)return;
        stop();touches.reset();dragging=false;chapter_loading=true;chapter_failed=false;chapter_committed=false;busy=true;message("正在加载下一章…");start(load_next_chapter());
    }
