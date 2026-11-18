template<class Owner> bool Reader::Controls::render(Owner& owner,std::uint32_t parent){
    const auto a=app->area();const float d=app->density()/app->ui_factor();const int x=a.x,y=a.y,w=a.width,h=a.height;
    constexpr uint32_t ink=0x243b31ff,muted=0x748178ff,accent=0x27664dff,paper=0xfffefaff;
    bool ok=true;size_t n=0;auto screen=app->screen;
    auto box=[&](int xx,int yy,int ww,int hh){return CanvasRegion{int(xx/d),int(yy/d),std::max(1,int(ww/d)),std::max(1,int(hh/d))};};
    auto text=[&] [[gnu::noinline]] (int xx,int yy,int ww,int hh,std::string_view value,Font font=Font::caption,uint32_t color=ink,uint8_t align=0){
        int size=font==Font::title?20:font==Font::body?16:font==Font::label?14:12;
        auto ratio=std::max(1.f,app->density()/app->ui_factor());auto s=manga::ellipsize(value,ww,int(std::ceil(size*ratio)));
        hh=std::max(hh,int(std::ceil((size+4)*ratio)));
        ok=Text(s,WidgetStyle{box(xx,yy,ww,hh),color,0,font,0,align}).render(owner,parent)&&ok;
    };
    auto rect=[&](int xx,int yy,int ww,int hh,uint32_t color,int radius=0){ok=Box(WidgetStyle{box(xx,yy,ww,hh),0,color,Font::caption,radius}).render(owner,parent)&&ok;};
    auto button=[&] [[gnu::noinline]] (int xx,int yy,int ww,int hh,std::string_view label,Act action,int arg=0,bool selected=false,bool enabled=true,bool silent=false){
        if(n==callbacks.size()){ok=owner.fail(Error::resource_limit);return;}auto& cb=callbacks[n++];cb={app,action,arg};
        bool icon=screen==reading&&label.size()<=3;
        const bool arrow=screen==reading&&(action==previous_page||action==next_page);
        ok=Button(label,WidgetStyle{box(xx,yy,ww,hh),(silent||arrow)?0u:screen==reading?0xf5f6eeff:enabled?(selected?0xffffffff:ink):muted,
            arrow?0u:screen==reading?0x14251edc:selected?accent:paper,screen==reading?(icon?Font::title:Font::caption):Font::label,6,1}).on_click_ref(cb).enabled(enabled&&app->ready).render(owner,parent)&&ok;
    };
    auto row=[&](int xx,int yy,int rw,std::string_view title,std::string_view detail,Act action,int arg,bool selected=false,uint32_t spine=accent,bool menu=false){
        const int height=app->dense_fonts()?52:38;
        auto label=manga::ellipsize(title,rw-(menu?74:42),14);
        button(xx,yy,rw-(menu?46:0),height,label,action,arg,false,true,true);
        rect(xx+3,yy+7,2,24,spine,1);
        text(xx+12,yy+3,rw-(menu?72:40),app->dense_fonts()?24:19,label,Font::label);
        text(xx+12,yy+(app->dense_fonts()?30:22),rw-(menu?72:40),app->dense_fonts()?22:15,detail,Font::caption,muted);
        if(menu){button(xx+rw-42,yy,42,height,std::string(title)+" · 目录",open_book,arg,false,true,true);
            for(int line=-1;line<=1;++line)rect(xx+rw-30,yy+height/2+line*5-1,18,2,muted,1);}
        else text(xx+rw-25,yy+9,18,24,selected?"✓":"›",Font::label,selected?accent:muted,1);
    };
    auto nav=[&]{int third=(w-8)/3;std::array<std::string_view,3> titles{"书架","发现","书源"};std::array<Act,3> actions{shelf_tab,discover_tab,source_tab};
        for(int i=0;i<3;++i){int xx=x+i*(third+4),ww=i==2?w-2*(third+4):third;bool active=screen==i;
            button(xx,y,ww,32,titles[i],actions[i],0,false,true,true);
            text(xx,y+5,ww,24,titles[i],active?Font::body:Font::label,active?ink:muted,1);
            if(active)rect(xx+ww/4,y+32,ww/2,2,accent,1);
        }
    };
    auto feedback=[&](int yy){text(x,yy,w,22,app->status,Font::caption,muted);};
    auto centered_panel=[&](int xx,int yy,int ww,int hh,std::string_view label){
        ok=Box(WidgetStyle{box(xx,yy,ww,hh),0,0x14251edc,Font::caption,6},
            Text(label).font(Font::caption).rgba(0xe2ede6ff).text_align(TextAlign::center))
            .align(Align::center).justify(Justify::center).render(owner,parent)&&ok;
    };
    if(screen==reading){
        auto edge_notice=[&](StripNotice notice,bool previous){
            if(notice==StripNotice::hidden)return;
            const bool failed=notice==StripNotice::failed;
            const std::string_view label=notice==StripNotice::end?(previous?"没有上一页":"没有下一页"):
                failed?(previous?"上一页加载失败，重试":"加载失败，点此重试"):(previous?"正在加载上一页…":"正在加载下一页…");
            const int width=std::min(w,notice==StripNotice::end?144:216),xx=x+(w-width)/2;
            const int yy=previous?y+(app->bars?36:0):y+h-((app->bars||!app->failure.empty())?66:34);
            if(failed)button(xx,yy,width,30,label,retry_buffer,previous?-1:1);
            else centered_panel(xx,yy,width,30,label);
        };
        edge_notice(app->strip_notice,false);edge_notice(app->previous_notice,true);
        if(app->bars||!app->failure.empty()){
            button(x,y,38,30,"≡",show_contents);button(x+42,y,42,30,app->zoom_mode==0?"适宽":app->zoom_mode==1?"放大":"整页",zoom,0,false,!app->busy&&bool(app->image));
            if(!app->busy)button(x+w-80,y,42,30,"设置",show_settings);
            button(x+w-34,y,34,30,"?",show_help);
            button(x,y+h-30,38,30,"上一页",previous_page,0,false,!app->busy&&app->page>0);button(x+w-38,y+h-30,38,30,"下一页",next_page,0,false,!app->busy&&app->page+1<app->pages.size());
            button(x+42,y+h-30,w-84,30,app->page_label(),jump_page,0,false,!app->busy&&!app->pages.empty());
        }
        if(app->busy){centered_panel(x+42,y+h-70,w-84,34,app->status);button(x+w-80,y,42,30,"取消",cancel);}
        else if(!app->failure.empty()){rect(x,y+h/2-38,w,72,0x14251edc,6);text(x,y+h/2-34,w,24,app->status,Font::caption,0xe2ede6ff,1);button(x+(w-80)/2,y+h/2-4,80,30,"重试",retry);}
        return ok;
    }
    if(screen==settings_screen){
        const bool local=app->book_settings();button(x,y,52,30,"返回",go_back);text(x+64,y+4,w-64,24,local?"本漫画 · "+app->book.title:std::string("缓存设置"),Font::label);
        if(local)button(x,y+40,w,30,std::string("阅读模式：")+(app->book.reading_mode==0?"自动":app->book.reading_mode==1?"条漫":"分页"),reading_mode,0,false,!app->busy);
        else text(x,y+44,w,26,"阅读方式在每本漫画内单独设置",Font::caption,muted);
        button(x,y+76,w,30,std::string("自动缓存：")+(app->cache.enabled?"开启":"关闭"),toggle_cache,0,false,!app->busy);
        text(x,y+112,w-100,22,app->cache_label(),Font::caption,muted);
        button(x+w-92,y+110,92,30,"清除缓存",clear_cache,0,false,!app->busy);
        text(x,y+146,w,32,app->status,Font::caption,muted);return ok;
    }
    if(screen==help){button(x,y,52,30,"返回",go_back);text(x+64,y+4,w-64,24,"阅读操作",Font::label);
        int yy=y+42;for(auto s:{"页漫横滑翻页 · 长页上下拖动","条漫连续滚动 · 自动拼接图片","双指缩放 · 放大后可拖动","≡ 打开目录 · 设置调整阅读"}){text(x,yy,w,24,s);yy+=26;}return ok;
    }
    if(screen==page_jump){button(x,y,52,30,"返回",go_back);text(x+64,y+4,w-128,24,"跳转页码",Font::label);button(x+w-56,y,56,30,"跳转",jump_submit,0,true);
        rect(x,y+38,w-80,34,paper,6);button(x+w-74,y+38,34,34,"-",jump_step,-1);button(x+w-36,y+38,36,34,"+",jump_step,1);text(x,y+82,w,22,"输入 1–"+std::to_string(app->pages.size())+" 之间的页码",Font::caption,muted);
        feedback(y+h-62);return ok;}
    if(screen==contents){
        button(x,y,app->contents_from_reader?80:48,30,app->contents_from_reader?"返回阅读":"返回",go_back);
        if(app->contents_from_reader)button(w>=204?x+84:x+w-48,y,48,30,"书架",shelf_tab);
        else text(x+60,y+4,w-120,24,"章节目录",Font::label);
        auto saved=std::any_of(app->shelf.begin(),app->shelf.end(),[&](auto& b){return manga::same(b,app->book);});if(!app->contents_from_reader||w>=204)button(x+w-56,y,56,30,saved?"已收藏":"收藏",collect,0,false,!app->busy);
        text(x,y+36,w-60,24,app->book.title,Font::label);button(x+w-56,y+36,56,24,"刷新",refresh_contents,0,false,!app->busy);
        const bool selector=app->book.source.empty()||(app->book.source=="copymanga"&&!app->groups.empty());
        const int order_width=w<240?54:70,selector_width=w<240?52:72,remaining=w-order_width-6-(selector?selector_width+6:0);
        if(!app->book.chapter.empty())button(x,y+64,remaining,30,remaining<64?"续读":"继续阅读",resume,0,false,!app->busy);
        else text(x,y+68,remaining,20,app->active_source(),Font::caption,muted);
        if(app->book.source.empty())button(x+w-order_width-selector_width-6,y+64,selector_width,30,app->language==0?"简中 ▾":app->language==1?"繁中 ▾":"英文 ▾",language_change,0,false,!app->busy);
        else if(selector)button(x+w-order_width-selector_width-6,y+64,selector_width,30,manga::ellipsize(app->groups[app->group_index].title,selector_width-12,12),language_change,0,false,!app->busy&&app->groups.size()>1);
        button(x+w-order_width,y+64,order_width,30,app->descending?"倒序":"正序",toggle_order,0,false,!app->busy);
    }else nav();
    if(screen==source_screen){
        auto rows=app->source_rows();auto start=std::min<unsigned>(app->source_start,((manga::sources.size()-1)/rows)*rows);
        for(unsigned i=start;i<manga::sources.size()&&i<start+rows;++i){auto& option=manga::sources[i];bool active=app->custom==option.id;int yy=y+42+(i-start)*app->source_cell();
            button(x,yy,w,app->source_cell()-6,option.title,choose_source,i,active,true,true);
            rect(x+10,yy+11,3,24,active?0xe3eee7ff:accent,1);
            text(x+23,yy+4,w-60,app->dense_fonts()?24:21,option.title,Font::label,active?0xffffffff:ink);
            text(x+23,yy+(app->dense_fonts()?30:26),w-60,app->dense_fonts()?22:16,option.detail,Font::caption,active?0xe3eee7ff:muted);
            text(x+w-30,yy+13,20,24,active?"✓":"›",Font::label,active?0xffffffff:muted,1);
        }
        button(x,y+h-28,36,28,"<",source_previous,0,false,start>0);
        button(x+w-36,y+h-28,36,28,">",source_next,0,false,start+rows<manga::sources.size());
        text(x+44,y+h-23,w-88,22,std::to_string(start/rows+1)+" / "+std::to_string((manga::sources.size()+rows-1)/rows),Font::caption,muted,1);
        return ok;
    }
    if(screen==discover){
        rect(x,y+38,w-52,34,paper,6);
        if(app->input.get().empty())text(x+8,y+46,w-76,20,"搜索漫画名称",Font::caption,muted);
        button(x+w-48,y+38,48,34,"搜索",search,0,true,!app->busy);
        button(x,y+76,w-66,24,app->active_source()+"  ›",source_tab,0,false,true,true);
        text(x+3,y+80,w-74,20,app->active_source()+"  ›",Font::caption,muted);
        if(app->custom.empty())button(x+w-62,y+76,62,24,app->language==0?"简中 ▾":app->language==1?"繁中 ▾":"英文 ▾",language_change,0,false,!app->busy);
    }
    if(screen==shelf_screen){text(x,y+42,w-74,22,app->status,Font::caption,muted);button(x+w-68,y+38,68,30,"设置",show_settings);if(app->shelf.empty()&&!app->busy){
        text(x,y+83,w,26,"书架还没有漫画",Font::body,ink,1);
        button(x+(w-120)/2,y+120,120,34,"去发现",discover_tab,0,true);return ok;}}
    auto count=screen==shelf_screen?app->shelf.size():screen==discover?app->results.size():app->chapters.size();
    int yy=y+app->list_y();unsigned shown=0;
    if(!app->busy&&app->failure.empty())for(size_t i=app->list_start;i<count&&shown<app->row_count();++i,++shown){
        if(screen==contents){auto label=manga::ellipsize(app->chapters[i].title,w-28,14);bool current=app->chapters[i].id==app->book.chapter;button(x,yy,w,32,label,open_chapter,i,false,true,true);if(current)rect(x+2,yy+6,2,20,accent);text(x+8,yy+7,w-20,22,label,Font::label,current?accent:ink);}
        else{auto& b=(screen==discover?app->results:app->shelf)[i];std::string detail(manga::source_name(b.source));
            if(!b.chapter.empty()){detail+=" · "+std::to_string(b.page+1)+(b.pages?"/"+std::to_string(b.pages):" 页");if(!b.chapter_title.empty())detail+=" · "+b.chapter_title;}
            else detail+=screen==shelf_screen?" · 未开始阅读":" · 点击开始阅读";
            auto cols=app->columns();int rw=(w-(cols-1)*8)/cols;int xx=x+(shown%cols)*(rw+8);yy=y+app->list_y()+(shown/cols)*app->row_height();row(xx,yy,rw,b.title,detail,read_book,i,false,b.source=="copymanga"?0xb7773dff:b.source=="mangakatana"?0x667c9bff:accent,true);
        }yy+=app->row_height();
    }
    if(app->busy||!app->failure.empty()||!count){
        if(screen!=shelf_screen)feedback(y+app->list_y()+4);
        if(app->busy)button(x+(w-88)/2,y+h-30,88,30,"取消加载",cancel);
        else if(!app->failure.empty())button(x+(w-88)/2,y+h-30,88,30,"重试",retry);
        return ok;
    }
    unsigned total=screen==shelf_screen?count:app->total;
    bool prev=app->list_start||app->offset,next=app->list_start+shown<count||(app->remote_list()&&app->offset+app->batch_size()<total);
    button(x,y+h-28,36,28,"<",previous_list,0,false,prev);button(x+w-36,y+h-28,36,28,">",next_list,0,false,next);
    std::string position=std::to_string((screen==discover&&!app->custom.empty()&&app->custom!="copymanga"?0:app->offset)+app->list_start+1)+"–"+std::to_string((screen==discover&&!app->custom.empty()&&app->custom!="copymanga"?0:app->offset)+app->list_start+shown)+(screen==discover&&!app->custom.empty()&&app->custom!="copymanga"?" · 第 "+std::to_string(app->offset/app->batch_size()+1)+" 组":" / "+std::to_string(total));
    text(x+44,y+h-23,w-88,22,position,Font::caption,muted,1);
    return ok;
}
