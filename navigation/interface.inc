// SPDX-License-Identifier: MIT
// Included in NavigationApp: layout/presentation stays separate from providers.
    int width=320,height=240;
    navui::Rect safe{0,0,320,240};
    bool map_screen()const{return screen==0||screen==1||screen==7||screen==8;}
    bool illustration()const{return demo||(active_key.empty()&&route.points.empty());}
    void geometry() {
        const auto& d=env.display().get();
        auto dp=[&](std::uint64_t v){return int(v*65536/std::max(1u,d.density_q16));};
        width=std::clamp(navui::logical_extent(d.width,d.density_q16),120,2048);height=std::clamp(navui::logical_extent(d.height,d.density_q16),120,2048);
        auto r=safe_rectangle(d);
        if(window_metrics){
            const auto& b=window_metrics->system_bar_insets;
            const auto& s=window_metrics->safe_insets;
            const int l=std::max<int>(r.x,std::max(s.left,b.left)),t=std::max<int>(r.y,std::max(s.top,b.top));
            const int rr=std::min<int>(r.x+r.width,d.width-std::max(s.right,b.right)),bb=std::min<int>(r.y+r.height,d.height-std::max(s.bottom,b.bottom));
            r={l,t,std::max(1,rr-l),std::max(1,bb-t)};
        }
        safe={dp(r.x),dp(r.y),dp(r.width),dp(r.height)};
        layout.update(width,height,safe,live);
        if(screen==7&&layout.compact){layout.deck.y-=20;layout.deck.h+=20;layout.map_h-=20;}
    }
    void resize_map() {
        geometry();
        // Request the pixels actually shown on the LCD. Matching the image to
        // physical pixels avoids resampling a larger DP-sized bitmap per drag.
        const auto& d=env.display().get();
        mw=std::min(512,navui::pixel_span(width,d.density_q16));
        mh=std::min(512,std::max(64,navui::pixel_span(layout.map_h,d.density_q16)));
        camera.width=mw;camera.height=mh;
    }
    nav::Viewport fit_camera()const {
        const int reserve=live?(layout.compact?80:104):(layout.compact?64:80);
        const int top=(safe.y+reserve)*mh/std::max(1,layout.map_h);
        return nav::fit_insets(route,mw,mh,(safe.x+16)*mw/width,top,
            (width-safe.x-safe.w+(layout.compact?96:70))*mw/width,24);
    }
    nav::Viewport focus_camera(nav::Point point)const {
        auto v=camera;auto c=nav::project(point);const double scale=256<<v.zoom;
        const int reserve=live?(layout.compact?80:104):(layout.compact?64:80);
        const double left=(safe.x+16)*double(v.width)/width;
        const double right=(width-safe.x-safe.w+(layout.compact?96:70))*double(v.width)/width;
        const double top=(safe.y+reserve)*double(v.height)/std::max(1,layout.map_h);
        const double bottom=24.0*v.height/std::max(1,layout.map_h);
        c.lon+=(right-left)/2/scale;c.lat+=(bottom-top)/2/scale;
        c.lon=std::clamp(c.lon,0.0,1.0);c.lat=std::clamp(c.lat,.000001,.999999);
        v.center=nav::unproject(c);v.center.lat=std::clamp(v.center.lat,-85.0,85.0);v.center=*nav::coordinate(nav::coordinate(v.center));return v;
    }
    nav::Point screen_point(nav::Point p)const {
        p=nav::pixel(p,camera);
        return {p.lon*width/std::max(1,camera.width)+pan_x,p.lat*layout.map_h/std::max(1,camera.height)+pan_y};
    }
    nav::Viewport panned()const {
        auto v=camera;auto c=nav::project(v.center);const double scale=256<<v.zoom;
        c.lon-=double(pan_x)*v.width/width/scale;c.lat-=double(pan_y)*v.height/std::max(1,layout.map_h)/scale;
        c.lon=std::clamp(c.lon,0.0,1.0);c.lat=std::clamp(c.lat,.005,.995);v.center=nav::unproject(c);
        return v;
    }
    Task<void> update_map() {
        while(requested_map){
            auto v=*requested_map;requested_map.reset();
            if(illustration()||active_key.empty())continue;
            auto r=co_await basemap(v);
            if(!r&&!requested_map&&!dragging){map_failed=true;ui_dirty=true;draw();}
        }
        map_pending=false;ui_dirty=true;draw();co_return Result<void>{};
    }
    void request_map(nav::Viewport v) {
        map_failed=false;camera=v;pan_x=pan_y=0;ui_dirty=true;draw();remember_view();
        requested_map=v;++map_generation;
        if(!map_pending){map_pending=true;if(!map_jobs.start(update_map()))map_pending=false;}
    }
    void action(unsigned id) {
        using namespace navui;
        if(id==OpenCompass){if(!idle.get())return;compass_return=screen;show(6);refresh_compass();return;}
        if(screen==6 && compass_saving){return;}
        if(id==CalibrateCompass){heading.model.capture.begin();ui_dirty=true;draw();return;}
        if(id==CancelCompassCalibration){heading.model.capture.collecting=false;heading.model.capture.candidate={};ui_dirty=true;draw();return;}
        if(id==SaveCompassCalibration){
            if(compass::valid_cal(heading.model.capture.candidate)){
                compass_saving=true;
                if(!compass_jobs.start(save_compass_calibration()))compass_saving=false;
                draw();
            }
            return;
        }
        if(id==RetryCompass){retry_compass();draw();return;}
        if(id==FirstMagnetometer||id==SecondMagnetometer){
            close_compass();heading.model.select(id==SecondMagnetometer?1:0);
            heading.model.capture.candidate={};compass_probed=false;refresh_compass();
            compass_saving=true;
            if(!compass_jobs.start(save_compass_calibration(false)))compass_saving=false;
            draw();return;
        }
        if(id==Back){(void)ctx->window().request_back();return;}
        if(id==Settings){settings_return_screen=screen;settings_tab=0;show(3);return;}
        if(id==Book){step_index=progress.step;show(2);return;}
        if(id==Plan){plan_return_screen=screen;show(5);return;}
        if(id==Search){begin_search(false);return;}
        if(id==MapSettings||id==LocationSettings||id==CitySettings){
            (void)key_editor.hide_keyboard();(void)city_editor.hide_keyboard();(void)endpoint_editor.hide_keyboard();
            settings_tab=id==CitySettings?2:id==LocationSettings?1:0;ui_dirty=true;draw();return;
        }
        if(id==Previous||id==Next){
            if(id==Previous&&step_index)--step_index;
            if(id==Next&&step_index+1<route.steps.size())++step_index;
            update_step();ui_dirty=true;draw();return;
        }
        if(id==ClearField){
            (void)key_editor.hide_keyboard();(void)city_editor.hide_keyboard();(void)endpoint_editor.hide_keyboard();
            if(settings_tab==0)key.set("");else if(settings_tab==2)city.set("");else endpoint.set("");
            ui_dirty=true;draw();return;
        }
        if(id==Clear){if(!idle.get())return;query.set("");results.clear();list_scroll=0;search_attempted=false;search_failed=false;toast_until=0;ui_dirty=true;draw();return;}
        if(!idle.get()){message("正在处理，请稍候");draw();return;}
        switch(id){
        case GoHere:
            if(selected_place){
                const auto place=*selected_place;invalidate_route();end=place;
                if(fix&&!fix->simulated&&now_ms()-fix->time_ms<=30000&&fix->accuracy<=80)start=nav::Place{"当前位置","",fix->point};
                plan_return_screen=7;show(5);
            }
            break;
        case ClearRoute:
            invalidate_route();image.reset();selected_place.reset();show(0);
            if(!active_key.empty())request_map(camera);
            break;
        case KeepNavigating:show(1);break;
        case EndNavigation:stop();show(1);message("导航已结束，路线已保留");break;
        case Food:case Fuel:case Parking:
            if(screen!=4)begin_search(false);
            query.set(id==Food?"餐饮":id==Fuel?"加油站":"停车场");run(search());break;
        case SelectStart:begin_search(true);break;
        case SelectEnd:begin_search(false);break;
        case Demo:start_demo();toast_until=0;break;
        case Locate:
            if(screen==5){invalidate_route();request_location(true);}
            else if(fix&&now_ms()-fix->time_ms<30000){following=true;request_map(focus_camera(fix->point));}
            else request_location();
            break;
        case RetryMap:request_map(camera);break;
        case Plus:zoom_map(1);break;
        case Minus:zoom_map(-1);break;
        case Overview:if(!route.points.empty()){following=false;request_map(fit_camera());}break;
        case Start:
            if(screen==5){if(active_key.empty()){settings_return_screen=5;settings_tab=0;show(3);message("先连接高德地图服务，即可查询路线");}else{stop();run(plan());}}
            else if(live)show(8);else toggle_live();
            break;
        case Swap:{invalidate_route();std::swap(start,end);break;}
        case Walk:case Bike:case Drive:
            if(live){message("请先结束导航，再切换出行方式");break;}
            mode=id==Walk?nav::Mode::walking:id==Bike?nav::Mode::cycling:nav::Mode::driving;
            invalidate_route();break;
        case Submit:(void)query_editor.hide_keyboard();run(search());break;
        case Save:if(live)message("请先结束导航，再保存设置");else run(save());break;
        case WifiSource:case ForwardSource:
            if(live){message("请先结束导航，再切换定位方式");break;}
            source=id==ForwardSource;break;
        default:if(id>=Result0&&id<Result0+results.size())choose(id-Result0);break;
        }
        ui_dirty=true;draw();
    }
    void pointer(const CanvasPointer& p) {
        if(p.phase==pointer_phase_down){
            if(pointer_down)return;
            pointer_down=true;pointer_id=p.pointer_id;down_x=p.x;down_y=p.y;dragging=false;
            pressed_action=paint.find(p.x,p.y);drag_origin_x=pan_x;drag_origin_y=pan_y;
            return;
        }
        if(!pointer_down||p.pointer_id!=pointer_id)return;
        if(p.phase==pointer_phase_cancel){pointer_down=false;if(dragging){pan_x=drag_origin_x;pan_y=drag_origin_y;}dragging=false;ui_dirty=true;return;}
        if(p.phase==pointer_phase_move){
            const int dx=p.x-down_x,dy=p.y-down_y;
            if((!pressed_action||(screen==4&&pressed_action>=navui::Result0))&&
               (dragging||std::abs(dx)+std::abs(dy)>8)){
                if(!dragging&&map_screen()&&idle.get()){drag_origin_x=pan_x;drag_origin_y=pan_y;++map_generation;requested_map.reset();}
                dragging=true;
                pressed_action=0;
                if(map_screen()&&screen!=8&&idle.get()){following=false;pan_x=std::clamp(drag_origin_x+dx,-width,width);pan_y=std::clamp(drag_origin_y+dy,-height,height);}
                else if(screen==4)list_scroll=std::clamp(list_scroll-(p.y-down_y),0,std::max(0,int(results.size())*66-(safe.h-114)));
                down_y=screen==4?p.y:down_y;
                ui_dirty=true;draw();
            }
            return;
        }
        if(p.phase==pointer_phase_up){
            pointer_down=false;
            if(dragging){if(map_screen()&&idle.get())request_map(panned());dragging=false;return;}
            const auto released=paint.find(p.x,p.y);
            if(pressed_action&&pressed_action==released)action(released);
        }
    }
    void sync_inputs() {
        auto set=[&](State<TextInputBox>& target,navui::Rect r,bool visible){
            TextInputBox b{{r.x,r.y,r.w,r.h},visible,Font::body,colors.ink};if(target.get()!=b)target.set(b);
        };
        const int x=safe.x+16,w=safe.w-32;
        set(query_box,{safe.x+56,safe.y+54,safe.w-152,36},screen==4);
        set(key_box,{x+10,safe.y+(layout.compact?105:112),w-20,layout.compact?32:36},screen==3&&settings_tab==0);
        set(city_box,{x+10,safe.y+(layout.compact?105:112),w-20,layout.compact?32:36},screen==3&&settings_tab==2);
        set(endpoint_box,{x+10,safe.y+126,w-20,36},screen==3&&settings_tab==1&&source==1);
    }
    void title(std::string_view name,std::string_view subtitle={}) {
        using namespace navui;
        paint.rect({0,0,width,height},colors.background);
        paint.icon_button(layout.back,"back",colors,Back);
        paint.text({layout.back.x+layout.back.w+12,layout.back.y+4,safe.w-78,32},name,colors.ink,20);
        if(!subtitle.empty())paint.text({safe.x+18,safe.y+55,safe.w-36,20},subtitle,colors.muted,12);
    }
    void demo_map() {
        const auto mapbg=colors.dark?0x202b2aff:0xeef1eaff;
        const auto block=colors.dark?0x2a3634ff:0xe2e7deff;
        const auto park=colors.dark?0x29493cff:0xd7e8d4ff;
        paint.rect({0,0,width,layout.map_h},mapbg);
        // Fictional geometry follows the same camera as the demo route. It
        // never represents a real basemap and is always visibly labelled.
        auto point=[&](double x,double y){return screen_point({114.052+x*.001,22.541+y*.001});};
        const auto origin=point(0,0);double stride=std::pow(2.0,std::max(0,16-camera.zoom));
        const int min_cell=std::max(layout.compact?72:48,(std::max(width,layout.map_h)+15)/16);
        double cw=0,ch=0;
        do{cw=point(.72*stride,0).lon-origin.lon;ch=origin.lat-point(0,.65*stride).lat;if(cw<min_cell||ch<min_cell)stride*=2;else break;}while(stride<131072);
        cw=std::max(1.0,cw);ch=std::max(1.0,ch);
        const int x0=int(std::floor(-origin.lon/cw))-1,x1=int(std::ceil((width-origin.lon)/cw))+1;
        const int y0=int(std::floor((origin.lat-layout.map_h)/ch))-1,y1=int(std::ceil(origin.lat/ch))+1;
        // Adaptive detail fills every viewport without an unbounded display
        // list when zooming out or dragging beyond the illustrative district.
        for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
            paint.rect({int(origin.lon+x*cw),int(origin.lat-(y+.6)*ch),int(cw*.67),int(ch*.6)},(x+2*y)%7==0?park:block,4);
        }
        auto pa=point(-2.16,1.95),pb=point(-.72,.65);
        navui::Rect green{int(pa.lon),int(pa.lat),int(pb.lon-pa.lon),int(pb.lat-pa.lat)};
        paint.rect(green,park,12);
        for(int i=0;green.w>35&&i<5;++i){
            const int x=green.x+green.w*(i+1)/6,y=green.y+green.h*(i%2?3:1)/4;
            paint.circle(x,y,5,colors.dark?0x365e48ff:0xbcd9b2ff);
        }
        const auto water=colors.dark?0x254c58ff:0xc5e2eaff;
        const double map_scale=std::pow(2.0,camera.zoom-16),river_x=point(2.6,0).lon,amplitude=point(.25,0).lon-origin.lon;
        for(int y=-20;y<layout.map_h+20;y+=20){
            auto river=[&](int yy){return river_x+amplitude*std::sin((origin.lat-yy)/ch*.65*stride);};
            paint.line(river(y),y,river(y+20),y+20,water,std::clamp(int(40*map_scale),4,80));
        }
        const auto road=colors.dark?0x43504bff:0xffffffff,edge=colors.dark?0x1d2926ff:0xdde2d8ff;
        auto roadline=[&](int x,int y,int xx,int yy,int wide){paint.line(x,y,xx,yy,edge,wide+2);paint.line(x,y,xx,yy,road,wide);};
        const int minor=std::clamp(int(6*map_scale),2,8),major=std::clamp(int(14*map_scale),4,22);
        for(int i=x0;i<=x1;++i){int x=int(origin.lon+i*cw);roadline(x,0,x,layout.map_h,minor);}
        for(int i=y0;i<=y1;++i){int y=int(origin.lat-i*ch);roadline(0,y,width,y,minor);}
        roadline(0,point(0,-1).lat,width,point(0,-1).lat,major);roadline(origin.lon,0,origin.lon,layout.map_h,major);roadline(0,point(0,1).lat,width,point(0,1).lat,major);
        auto label=[&](double x,double y,std::string_view s){auto p=point(x,y);paint.text({int(p.lon)-35,int(p.lat)-8,80,18},s,colors.dark?0x9dac9eff:0x85917fff,11);};
        if(camera.zoom>=16){label(-1.5,-1,"梧桐路");label(.1,.55,"云杉街");label(-1.7,1.3,"口袋公园");label(2.7,1.8,"湖畔绿道");}
    }
    std::optional<nav::Viewport> placement_camera,placement_source;
    std::optional<nav::ImagePlacement> placement;
    int placement_width=0,placement_height=0;
    navui::Rect image_rectangle(){
        if(!placement_camera||!placement_source||!nav::same_view(*placement_camera,camera)||!nav::same_view(*placement_source,image_camera)||placement_width!=width||placement_height!=layout.map_h){
            placement_camera=camera;placement_source=image_camera;placement_width=width;placement_height=layout.map_h;
            placement=nav::image_placement(image_camera,camera,width,layout.map_h);
        }
        if(!placement)return {};
        return {int(std::lround(placement->x))+pan_x,int(std::lround(placement->y))+pan_y,int(std::lround(placement->width)),int(std::lround(placement->height))};
    }
    void map_layer() {
        paint.clip({0,0,width,layout.map_h});
        const auto bitmap=image?image_rectangle():navui::Rect{};
        if(image&&bitmap.w>0&&bitmap.h>0)paint.image(bitmap,image->handle());
        else if(illustration())demo_map();
        else{
            paint.rect({0,0,width,layout.map_h},colors.dark?0x26302fff:0xe9eee8ff);
            for(int x=0;x<width;x+=48)paint.line(x,0,x,layout.map_h,colors.dark?0x303d3aff:0xe0e6dfff);
            for(int y=0;y<layout.map_h;y+=48)paint.line(0,y,width,y,colors.dark?0x303d3aff:0xe0e6dfff);
        }
        paint.clip({0,0,width,std::max(1,layout.map_h-24)});
        const auto& points=route_drawing.get(route,route_revision,camera);
        const auto shift=route_drawing.offset(camera);
        for(unsigned pass=0;screen!=7&&pass<2;++pass)for(std::size_t i=1;i<points.size();++i){auto a=points[i-1],b=points[i];
            int x=int((a.lon+shift.lon)*width/camera.width)+pan_x,y=int((a.lat+shift.lat)*layout.map_h/camera.height)+pan_y;
            int xx=int((b.lon+shift.lon)*width/camera.width)+pan_x,yy=int((b.lat+shift.lat)*layout.map_h/camera.height)+pan_y;
            paint.line(x,y,xx,yy,pass?0x4389f5ff:0xffffffff,pass?6:9);
        }
        if(screen!=7&&!route.points.empty()){
            auto a=screen_point(route.points.front()),b=screen_point(route.points.back());
            paint.circle(a.lon,a.lat,7,0xffffffff);paint.circle(a.lon,a.lat,4,0x3c9c7bff);
            paint.circle(b.lon,b.lat,8,0xffffffff);paint.circle(b.lon,b.lat,5,0xf06c61ff);
        }
        if(fix){auto p=screen_point(fix->point);const bool good=(fix->simulated||now_ms()-fix->time_ms<=30000)&&fix->accuracy<=80;
            paint.circle(p.lon,p.lat,18,good?0x4389f52a:0x8c93932a);paint.circle(p.lon,p.lat,10,0xffffffff);
            paint.circle(p.lon,p.lat,7,good?0x347ff1ff:0x8d9695ff);paint.icon("arrow",p.lon,p.lat,0xffffffff,12);
        }
        if(screen==7&&selected_place){
            auto p=screen_point(selected_place->point);
            paint.circle(p.lon,p.lat-10,15,colors.primary);paint.circle(p.lon,p.lat-10,6,colors.surface);
            paint.line(p.lon-8,p.lat,p.lon,p.lat+10,colors.primary,4);paint.line(p.lon,p.lat+10,p.lon+8,p.lat,colors.primary,4);
        }
        paint.unclip();paint.unclip();
        // Keep the provider's own attribution unobscured at the image bottom.
        const auto caption=illustration()?(demo?"模拟导航 · 示意地图":"示意地图 · 非真实道路"):image?"高德地图 © AutoNavi":"底图暂不可用 · 路线预览";
        if(!image||!nav::same_view(camera,image_camera)){
            paint.rect({0,layout.map_h-19,width,19},colors.dark?0x1d2827dc:0xffffffdd);
            paint.text({safe.x+10,layout.map_h-17,safe.w-20,16},caption,colors.dark?0x99aca6ff:0x6a7e75ff,10);
        }
    }
    std::string remaining_text()const {
        auto meters=progress.usable?unsigned(progress.remaining):route.distance;
        return nav::distance(meters);
    }
    std::string duration_text()const {
        if(!route.seconds)return "路线就绪";
        const double ratio=route.distance&&progress.usable?std::clamp(progress.remaining/double(route.distance),0.0,1.0):1.0;
        return nav::unsigned_text(std::max(1,int(std::ceil(route.seconds*ratio/60))))+" 分钟";
    }
    void map_page_draw() {
        using namespace navui;
        paint.rect({0,0,width,height},colors.background);map_layer();
        if(live){
            const int h=layout.compact?57:78;
            Rect turn{safe.x+10,safe.y+10,safe.w-20,h};paint.card(turn,colors,16);
            paint.rect({turn.x+10,turn.y+10,h-20,h-20},mix(colors.surface,colors.primary,24),12);
            std::string instruction=progress.off_route?"已偏离路线，等待重新规划":progress.usable&&!route.steps.empty()?route.steps[progress.step].instruction:"等待有效位置";
            const auto arrow=instruction.find("左")!=instruction.npos?"left":instruction.find("右")!=instruction.npos?"right":"arrow";
            paint.icon(arrow,turn.x+h/2,turn.y+h/2,colors.primary,layout.compact?22:30);
            const auto dist=progress.arrived?std::string("已到达"):progress.usable?nav::distance(unsigned(progress.next_turn)):fix&&now_ms()-fix->time_ms>30000?"位置已过期":fix&&fix->accuracy>80?"定位范围较大":"正在定位";
            paint.text({turn.x+h,turn.y+8,turn.w-h-12-(compass_available()?40:0),32},dist,colors.ink,layout.compact?21:28);
            paint.text({turn.x+h,turn.y+(layout.compact?34:43),turn.w-h-12-(compass_available()?40:0),layout.compact?18:20},instruction,colors.muted,layout.compact?11:13);
        }else{
            auto search=layout.search;
            paint.icon_button(layout.map_back,"back",colors,Back);
            paint.card(search,colors,search.h/2);paint.icon("search",search.x+19,search.y+search.h/2,colors.primary);
            paint.text({search.x+38,search.y+(search.h-20)/2,search.w-48,24},"搜索地点、地址",colors.muted,14);
            paint.hit(search,Search);paint.icon_button(layout.settings,"settings",colors,Settings);
        }
        if(map_failed){
            Rect retry{safe.x+12,safe.y+(live?(layout.compact?76:104):60),std::min(190,safe.w-24),28};
            paint.rect(retry,colors.surface,12);paint.text({retry.x+9,retry.y+6,retry.w-18,18},"底图未更新 · 点击重试",colors.primary,11);paint.hit(retry,RetryMap);
        }
        if(layout.zoom_in.y>=safe.y+(live?70:54)&&layout.zoom_out.y+layout.zoom_out.h+6<=layout.map_h-24){
            paint.card({layout.zoom_in.x,layout.zoom_in.y,layout.compact?76:38,layout.compact?34:68},colors,16);
            paint.icon("plus",layout.zoom_in.x+19,layout.zoom_in.y+17,camera.zoom<17?colors.ink:colors.line);if(camera.zoom<17)paint.hit(layout.zoom_in,Plus);
            if(layout.compact)paint.line(layout.zoom_out.x,layout.zoom_out.y+8,layout.zoom_out.x,layout.zoom_out.y+26,colors.line);
            else paint.line(layout.zoom_in.x+10,layout.zoom_out.y,layout.zoom_in.x+28,layout.zoom_out.y,colors.line);
            paint.icon("minus",layout.zoom_out.x+19,layout.zoom_out.y+17,camera.zoom>1?colors.ink:colors.line);if(camera.zoom>1)paint.hit(layout.zoom_out,Minus);
        }
        // On a short panel, recenter sits on the other side of the map.
        auto locate=layout.recenter;
        if(locate.y>safe.y+(live?70:58)){
            paint.icon_button(locate,"locate",colors,Locate);
            if(following&&fix)paint.icon("locate",locate.x+19,locate.y+19,colors.primary);
            if(live&&!following){
                Rect resume{layout.compact?locate.x+46:locate.x-98,locate.y,layout.compact?76:90,38};
                paint.button(resume,"回到导航",colors,Locate,false);
            }
        }
        compass_badge_draw();
        auto r=layout.deck;paint.card(r,colors,20);
        paint.rect({r.x+r.w/2-16,r.y+6,32,3},mix(colors.surface,colors.muted,40),2);
        if(route.points.empty()){
            paint.text({r.x+16,r.y+(layout.compact?9:12),r.w-(!layout.compact&&!active_key.empty()?104:32),layout.compact?30:36},"随行地图",colors.ink,layout.compact?18:23);
            if(!layout.compact){
                if(!active_key.empty())paint.button({r.x+r.w-76,r.y+14,60,30},"模拟",colors,Demo,false);
                const int gap=8,chip=(r.w-32-gap*2)/3;
                const unsigned ids[]={Food,Fuel,Parking};const char* names[]={"餐饮","加油站","停车场"};
                for(int i=0;i<3;++i)paint.button({r.x+16+i*(chip+gap),r.y+54,chip,32},names[i],colors,ids[i],false);
            }
            paint.button(layout.primary,active_key.empty()?"模拟导航":"搜索目的地",colors,active_key.empty()?Demo:Search);
            paint.button(layout.secondary,"路线",colors,Plan,false);
            if(layout.compact&&!active_key.empty()){
                const int x=layout.secondary.x+layout.secondary.w+4,w=std::min(64,layout.primary.x-x-4);
                if(w>=36){Rect d{x,layout.secondary.y,w,36};paint.rect(d,mix(colors.surface,colors.primary,20),18);paint.text({x+2,d.y+10,w-4,20},"模拟",colors.primary,12,1);paint.hit(d,Demo);}
            }
        }else{
            const int heading=layout.compact?23:29;
            paint.text({r.x+16,r.y+13,r.w/2,layout.compact?30:38},progress.arrived?"已到达目的地":duration_text(),colors.ink,heading);
            if(layout.compact&&live){
                paint.text({r.x+16,r.y+46,r.w/2,20},remaining_text()+" · 路书 ›",colors.muted,11);
                paint.hit({r.x+8,r.y+10,r.w/2-8,r.h-18},Book);
            }else paint.text({r.x+r.w/2,r.y+23,r.w/2-(layout.compact?48:16),22},remaining_text()+" · "+nav::mode_name(mode),colors.muted,12,2);
            if(!layout.compact){
                if(!live)paint.text({r.x+16,r.y+53,r.w-68,22},demo?"模拟行程":end?end->name:"目的地",colors.muted,13);
                if(!live){paint.hit({r.x+12,r.y+47,r.w-62,32},Plan);paint.icon_button({r.x+r.w-44,r.y+45,32,32},"close",colors,ClearRoute);}
                const auto line_y=r.y+(live?48:86);paint.rect({r.x+16,line_y,r.w-32,3},mix(colors.surface,colors.muted,25),2);
                if(progress.usable&&tracker.total()>0)paint.rect({r.x+16,line_y,std::max(2,int((r.w-32)*progress.along/tracker.total())),3},colors.primary,2);
            }
            auto primary=layout.primary;if(layout.compact&&live)primary.y=r.y+20;
            const auto label=progress.arrived?(demo?"再走一次":"重新规划"):live?"结束导航":"开始导航";
            paint.button(primary,label,colors,progress.arrived?(demo?Demo:Plan):Start,true,idle.get());
            if(!(layout.compact&&live)){
                paint.icon_button(layout.book,"book",colors,Book);
                Rect overview{layout.book.x+layout.book.w+8,layout.book.y,layout.compact||safe.w<360?36:60,36};
                paint.icon_button(overview,"route",colors,Overview,idle.get());
            }
            if(layout.compact&&!live){
                paint.hit({r.x+8,r.y+10,r.w-48,32},Plan);
                paint.icon_button({r.x+r.w-40,r.y+8,30,30},"close",colors,ClearRoute);
            }
        }
    }
    void place_draw(){
        using namespace navui;
        paint.rect({0,0,width,height},colors.background);map_layer();
        paint.icon_button(layout.map_back,"back",colors,Back);
        Rect heading=layout.search;paint.card(heading,colors,heading.h/2);
        paint.text({heading.x+10,heading.y+(heading.h-24)/2,heading.w-20,24},"地点详情",colors.ink,14,1);
        paint.icon_button(layout.settings,"settings",colors,Settings);
        auto r=layout.deck;paint.card(r,colors,20);
        if(selected_place){
            paint.text({r.x+16,r.y+12,r.w-32,26},selected_place->name,colors.ink,layout.compact?18:22);
            const auto address=selected_place->address.empty()?nav::coordinate(selected_place->point):selected_place->address;
            paint.paragraph({r.x+16,r.y+(layout.compact?40:46),r.w-32,layout.compact?22:48},address,colors.muted,12,20);
        }
        paint.button(layout.primary,"去这里",colors,GoHere);
        paint.button(layout.secondary,layout.compact?"返回":"返回结果",colors,Back,false);
    }
    void exit_draw(){
        using namespace navui;
        map_page_draw();paint.rect({0,0,width,height},0x17243b70);
        // Only the dialog contributes native focus targets or Canvas hits.
        paint.hit_count=0;
        const int w=std::min(360,safe.w-32),h=layout.compact?148:174;
        Rect r{safe.x+(safe.w-w)/2,safe.y+(safe.h-h)/2,w,h};paint.card(r,colors,20);
        paint.text({r.x+18,r.y+18,r.w-36,28},demo?"结束模拟导航？":"结束本次导航？",colors.ink,20);
        paint.text({r.x+18,r.y+54,r.w-36,22},"路线会保留，可随时重新开始",colors.muted,12);
        const int bw=(r.w-44)/2,y=r.y+r.h-52;
        paint.button({r.x+16,y,bw,36},"继续导航",colors,KeepNavigating,true);
        paint.button({r.x+28+bw,y,bw,36},"结束导航",colors,EndNavigation,false);
    }
    void modes(int y) {
        using namespace navui;
        Rect row{safe.x+16,y,safe.w-32,38};paint.rect(row,mix(colors.surface,colors.muted,15),14);
        const unsigned actions[]={Walk,Bike,Drive};const char* labels[]={"步行","骑行","驾车"};
        for(unsigned i=0;i<3;++i){Rect r{row.x+int(i)*row.w/3+3,row.y+3,row.w/3-6,32};
            if(unsigned(mode)==i)paint.rect(r,colors.surface,11);
            paint.text({r.x,r.y+8,r.w,18},labels[i],unsigned(mode)==i?colors.primary:colors.muted,13,1);paint.hit(r,actions[i]);
        }
    }
    void plan_draw() {
        using namespace navui;
        title("路线规划");const bool c=layout.compact;
        modes(safe.y+52);
        const int y=safe.y+101,line_h=c?std::clamp((safe.h-159)/2,22,39):55;
        Rect card{safe.x+16,y,safe.w-32,line_h*2+4};paint.card(card,colors,16);
        for(unsigned i=0;i<2;++i){
            const int yy=y+int(i)*line_h;paint.circle(card.x+17,yy+line_h/2,4,i?0xe7756aff:0x44a285ff);
            paint.text({card.x+33,yy+(line_h-20)/2,card.w-82,24},i?(end?end->name:"选择目的地"):(start?start->name:"选择起点"),i?(end?colors.ink:colors.muted):(start?colors.ink:colors.muted),14);
            paint.hit({card.x,yy,card.w-46,line_h},i?SelectEnd:SelectStart);
        }
        paint.line(card.x+32,y+line_h,card.x+card.w-49,y+line_h,colors.line);
        paint.icon("swap",card.x+card.w-24,y+(c?line_h*3/2:card.h/2),colors.muted);paint.hit({card.x+card.w-44,c?y+line_h:y,44,c?line_h:card.h},Swap);
        Rect primary{safe.x+16,safe.y+safe.h-46,safe.w-32,36};
        if(!c){
            Rect locate{safe.x+16,y+card.h+16,safe.w-32,40};paint.button(locate,"使用当前位置出发",colors,Locate,false,idle.get());
            paint.paragraph({safe.x+20,locate.y+54,safe.w-40,74},"选好起点和目的地，即可查看路线。导航会根据有效位置提示下一次转向。",colors.muted,13,21);
        }else{
            // The small locating control remains reachable without scrolling.
            paint.icon("locate",card.x+card.w-24,y+line_h/2,colors.primary,16);paint.hit({card.x+card.w-44,y,44,line_h},Locate);
        }
        const bool ready=start&&end;
        paint.button(primary,!idle.get()?"正在规划…":active_key.empty()?"连接地图服务":!start?"请选择起点":!end?"请选择目的地":"查看路线",colors,Start,true,idle.get()&&(active_key.empty()||ready));
    }
    void search_draw() {
        using namespace navui;
        title(search_start?"选择起点":"搜索目的地");
        paint.icon_button({safe.x+safe.w-46,safe.y+10,36,36},"settings",colors,Settings);
        Rect field{safe.x+16,safe.y+52,safe.w-32,40};paint.card(field,colors,14);paint.icon("search",field.x+20,field.y+20,colors.primary,18);
        paint.icon("close",field.x+field.w-61,field.y+20,colors.muted,16);paint.hit({field.x+field.w-78,field.y,34,40},Clear);
        paint.text({field.x+field.w-39,field.y+12,32,20},"搜索",colors.primary,12,1);if(idle.get())paint.hit({field.x+field.w-44,field.y,44,40},Submit);
        if(results.empty()){
            const int center=safe.y+std::max(search_attempted?122:142,(safe.h+40)/2);
            if(!layout.compact){paint.circle(safe.x+safe.w/2,center-35,28,mix(colors.background,colors.primary,16));paint.icon("pin",safe.x+safe.w/2,center-35,colors.primary,26);}
            const auto heading=!idle.get()?"正在搜索…":search_failed?"搜索暂不可用":search_attempted?"未找到相关地点":"搜索地点或地址";
            paint.text({safe.x+18,center,safe.w-36,24},heading,colors.ink,16,1);
            const auto hint=search_failed?(active_key.empty()?"请在设置中连接高德地图服务":"检查网络和 Key 后重试"):search_attempted?"试试更短的关键词，或更换搜索城市":active_city.empty()?"也可输入经度,纬度（GCJ-02）":"搜索城市："+active_city;
            paint.text({safe.x+18,center+29,safe.w-36,20},hint,colors.muted,11,1);
            if(idle.get()&&!search_attempted){
                const int chip=(safe.w-48)/3;
                for(int i=0;i<3;++i)paint.button({safe.x+16+i*(chip+8),safe.y+104,chip,32},i==0?"餐饮":i==1?"加油站":"停车场",colors,i==0?Food:i==1?Fuel:Parking,false);
            }
            if(idle.get()&&search_failed&&!layout.compact)paint.button({safe.x+safe.w/2-60,center+64,120,36},active_key.empty()?"连接地图服务":"重新搜索",colors,active_key.empty()?Settings:Submit);
        }
        // The result list uses native controls for key focus and scrolling.
    }
    void settings_draw() {
        using namespace navui;
        title("偏好设置");const int x=safe.x+16,w=safe.w-32;
        paint.icon_button({safe.x+safe.w-46,safe.y+10,36,36},"compass",colors,OpenCompass);
        const unsigned tabs[]={MapSettings,LocationSettings,CitySettings};
        const char* labels[]={"地图服务","定位来源","搜索城市"};
        for(unsigned i=0;i<3;++i){Rect r{x+int(i)*w/3,safe.y+55,w/3,31};
            if(settings_tab==i)paint.rect(r,mix(colors.surface,colors.primary,24),12);
            paint.text({r.x,r.y+6,r.w,22},labels[i],settings_tab==i?colors.primary:colors.muted,12,1);paint.hit(r,tabs[i]);
        }
        if(settings_tab!=1){
            paint.text({x,safe.y+(layout.compact?88:92),w,17},settings_tab==0?"高德 Web 服务 Key":"优先搜索的城市（选填）",colors.muted,11);
            Rect clear{x+w-42,safe.y+(layout.compact?85:89),42,23};
            paint.text(clear,"清空",colors.primary,11,2);paint.hit(clear,ClearField);
            paint.rect({x,safe.y+(layout.compact?104:111),w,layout.compact?34:38},colors.surface,12);
            if(!layout.compact)paint.paragraph({x+2,safe.y+180,w-4,100},settings_tab==0?"密钥仅保存在这台设备的应用设置中。地图与路线查询的可用额度，以你的高德账号为准。":"输入城市名称，有助于找到同名地点。留空时搜索所有城市，请在结果中确认具体地址。",colors.muted,13,22);
            else if(safe.h>=232)paint.text({x,safe.y+153,w,22},settings_tab==0?"密钥仅保存在本机":"留空可搜索所有城市",colors.muted,11);
        }else{
            for(unsigned i=0;i<2;++i){Rect r{x+int(i)*(w/2+4),safe.y+94,w/2-4,27};paint.button(r,i?"网络转发":"免费 Wi-Fi",colors,i?ForwardSource:WifiSource,source==i);}
            if(source==1){
                paint.rect({x,safe.y+125,w,38},colors.surface,12);
                if(!layout.compact||safe.h>=232)paint.paragraph({x,safe.y+172,w,layout.compact?25:100},"填入位置服务地址，可接收手机或 GPS 适配器提供的位置。",colors.muted,layout.compact?11:13,20);
            }else paint.paragraph({x,safe.y+132,w,layout.compact?std::max(16,safe.h-178):160},"通过附近 Wi-Fi 估算位置。需要设备扫描权限，覆盖与精度因地区而异。",colors.muted,layout.compact?12:14,layout.compact?18:24);
        }
        const int button_y=safe.y+safe.h-(layout.compact?40:48);
        if(layout.compact){
            paint.button({x,button_y,w,32},!idle.get()?"正在保存…":"保存设置",colors,Save,true,idle.get());
        }else paint.button({x,button_y,w,38},!idle.get()?"正在保存…":"保存设置",colors,Save,true,idle.get());
    }
    void book_draw() {
        using namespace navui;
        title("行程路书",route.points.empty()?"规划路线后，在这里查看每一段行程":nav::summary(route,mode));
        if(route.steps.empty()){
            paint.icon("route",safe.x+safe.w/2,safe.y+safe.h/2,colors.primary,30);
            paint.button({safe.x+24,safe.y+safe.h-56,safe.w-48,38},"规划一条路线",colors,Plan);return;
        }
        const int top=safe.y+87,available=safe.h-145,count=std::max(1,available/81);
        for(int i=0;i<count&&step_index+i<route.steps.size();++i){
            const auto& s=route.steps[step_index+i];Rect r{safe.x+16,top+i*81,safe.w-32,73};paint.card(r,colors,14);
            paint.circle(r.x+21,r.y+25,12,i==0?colors.primary:mix(colors.surface,colors.muted,28));
            paint.text({r.x+10,r.y+17,22,18},nav::unsigned_text(step_index+i+1),i==0?colors.on_primary:colors.muted,11,1);
            paint.text({r.x+43,r.y+10,r.w-56,23},s.instruction,colors.ink,14);
            paint.text({r.x+43,r.y+43,r.w-56,19},s.road+" · "+nav::distance(s.distance),colors.muted,12);
        }
        const int y=safe.y+safe.h-46;
        paint.button({safe.x+16,y,62,34},"上一段",colors,Previous,false,step_index>0);
        paint.text({safe.x+84,y+8,safe.w-168,22},nav::unsigned_text(step_index+1)+" / "+nav::unsigned_text(route.steps.size()),colors.muted,12,1);
        paint.button({safe.x+safe.w-78,y,62,34},"下一段",colors,Next,false,step_index+1<route.steps.size());
    }
    void draw() {
        compass_only_dirty=false;
        ui_dirty=true;
        if(ctx&&!in_tick)clock();
    }
    struct MapFrame {
        int width,height,screen,zoom,camera_w,camera_h;
        double lon,lat;
        navui::Rect safe;
        std::uint64_t image,toast;
        unsigned density,route_revision,notice_revision;
        bool live,demo,idle,map_pending,map_failed,compass_available;
        bool operator==(const MapFrame&)const=default;
    };
    std::optional<MapFrame> previous_map_frame;
    std::optional<nav::Point> previous_fix_pixel;
    int previous_pan_x=0,previous_pan_y=0;
    void flush_draw() {
        if(!canvas_node)return;
        resize_map();sync_inputs();paint.begin(width,height,env.display().get().density_q16);
        switch(screen){case 0:case 1:map_page_draw();break;case 2:book_draw();break;case 3:settings_draw();break;case 4:search_draw();break;case 5:plan_draw();break;case 6:compass_draw();break;case 7:place_draw();break;case 8:exit_draw();break;}
        if(toast_until&&now_ms()<toast_until&&!notice.get().empty()){
            const int map_y=layout.map_h-70;
            const bool over_map=map_screen()&&map_y>=safe.y+64;
            const int y=over_map?map_y:safe.y+8;
            navui::Rect toast{safe.x+(over_map?14:60),y,safe.w-(over_map?28:76),38};
            paint.rect(toast,0x253b4bff,12);
            paint.paragraph({toast.x+12,toast.y+3,toast.w-24,34},notice.get(),0xffffffff,11,16);
        }
        if(!idle.get()){
            const int x=safe.x+safe.w/2,y=safe.y+4;
            const auto angle=unsigned(now_ms()/4)%360;
            pxa_canvas_arc(&paint.frame,x,y,4,angle,angle+220,colors.primary,2);
        }
        const MapFrame view{width,height,int(screen),camera.zoom,camera.width,camera.height,camera.center.lon,camera.center.lat,safe,
            image?image->handle():0,toast_until,env.display().get().density_q16,route_revision,notice_revision,live,demo,idle.get(),map_pending,map_failed,compass_available()};
        std::array<pxa_canvas_dirty_rect_t,4> regions{};std::uint8_t count=0;
        auto dirty=[&](navui::Rect r){
            const int x=std::max(0,r.x),y=std::max(0,r.y),right=std::min(width,r.x+r.w),bottom=std::min(height,r.y+r.h);
            if(right>x&&bottom>y&&count<regions.size())regions[count++]={x,y,right-x,bottom-y};
        };
        const auto position=fix?std::optional(screen_point(fix->point)):std::nullopt;
        if(!force_frame&&map_screen()&&idle.get()&&previous_map_frame&&*previous_map_frame==view){
            if(compass_only_dirty)dirty(compass_badge());
            else if(previous_pan_x!=pan_x||previous_pan_y!=pan_y)dirty({0,0,width,layout.map_h});
            else if(live){
                dirty({safe.x+8,safe.y+8,safe.w-16,layout.compact?64:85});
                if(position||previous_fix_pixel){
                    auto a=position?*position:*previous_fix_pixel,b=previous_fix_pixel?*previous_fix_pixel:a;
                    const int x=int(std::min(a.lon,b.lon))-21,y=int(std::min(a.lat,b.lat))-21;
                    dirty({x,y,int(std::max(a.lon,b.lon))-x+22,int(std::max(a.lat,b.lat))-y+22});
                }
            }
            if(live&&!compass_only_dirty)dirty({layout.deck.x-2,layout.deck.y-2,layout.deck.w+4,layout.deck.h+7});
        }
        sync_key_targets();sync_results();
        if(!paint.present(canvas_node,regions.data(),count,force_frame))(void)ctx->log().write(LogLevel::warning,"Navigation: map frame rejected");
        else{previous_map_frame=view;previous_fix_pixel=position;previous_pan_x=pan_x;previous_pan_y=pan_y;force_frame=false;}
        ui_dirty=false;
        compass_only_dirty=false;
    }
    struct ScreenCanvas:ViewModifiers {
        static constexpr Capacity capacity{2,0,1};
        NavigationApp* app;
        explicit ScreenCanvas(NavigationApp* p):app(p){}
        void operator()(const CanvasPointer& p){app->pointer(p);}
        template<class Owner> bool render(Owner& owner,std::uint32_t parent){
            auto node=owner.create(parent,protocol::canvas);app->canvas_node=node;
            // A scroll container receives gestures without becoming a blank
            // focus stop in Gen4's native navigation. It has no scroll content.
            auto pointer=owner.create(parent,3);auto& tx=owner.transaction();
            return node&&pointer&&tx.fill(node,protocol::width)&&tx.fill(node,protocol::height)&&
                tx.u8(pointer,protocol::position,1)&&tx.fill(pointer,protocol::width)&&tx.fill(pointer,protocol::height)&&
                tx.u8(pointer,protocol::scroll_axis,0)&&tx.u8(pointer,protocol::scrollbar,0)&&
                tx.u64(pointer,protocol::event_mask,1ull<<6)&&owner.on_pointer(pointer,*this);
        }
    };
    auto view() {
        return Stack(ScreenCanvas(this),
            TextInput(query,query_editor).single_line().max_bytes(72).box(query_box).on_submit([this]{action(navui::Submit);}).background(colors.surface).border_width(0_dp).enabled(idle),
            TextInput(key,key_editor).single_line().max_bytes(32).box(key_box).password().background(colors.surface).border_width(0_dp).enabled(idle),
            TextInput(city,city_editor).single_line().max_bytes(32).box(city_box).background(colors.surface).border_width(0_dp).enabled(idle),
            TextInput(endpoint,endpoint_editor).single_line().max_bytes(256).box(endpoint_box).background(colors.surface).border_width(0_dp).enabled(idle),
            KeyControls(this),ResultControls(this),KeyBackControls(this)
        ).fill().fill_height();
    }
    Result<void> on_start(Context& context,std::span<const std::byte> config) {
        ctx=&context;auto r=env.initialize(config);if(!r)return r;
        (void)ctx->window().fullscreen(WindowBarMode::hidden,WindowBarMode::hidden);
        jobs.on_error(this,[](void* p,Error) noexcept {auto* app=static_cast<NavigationApp*>(p);app->done();app->message("操作暂不可用，请重试");});
        location_jobs.on_error(this,[](void* p,Error) noexcept {auto* app=static_cast<NavigationApp*>(p);app->location_pending=false;app->message("位置更新失败，稍后重试");});
        map_jobs.on_error(this,[](void* p,Error) noexcept {auto* app=static_cast<NavigationApp*>(p);app->map_pending=false;app->map_failed=true;app->draw();});
        settings_jobs.on_error(this,[](void* p,Error) noexcept {auto* app=static_cast<NavigationApp*>(p);app->bookmark_pending=false;app->bookmark_dirty=true;app->bookmark_due=now_ms()+10000;});
        compass_jobs.on_error(this,[](void* pointer,Error error) noexcept {
            auto* app=static_cast<NavigationApp*>(pointer);
            app->compass_saving=false;app->compass_connection_failed(error);
        });
        resize_map();clock();
        return ctx->tasks().start(load());
    }
    Result<bool> on_event(Context&,const Event& event) {
        if(event.service==4&&event.opcode==0x8001){tick();return true;}
        if(compass_event(event))return true;
        auto r=env.update(event);
        if(r&&*r){resize_map();ui_dirty=true;draw();}
        return r;
    }
    void on_window(Context&,const WindowMetrics& m){window_metrics=m;force_frame=true;resize_map();ui_dirty=true;draw();}
    void on_foreground(Context&) {force_frame=true;ui_dirty=true;flush_draw();clock();}
    BackAction on_back() {
        if(initialized&&!idle.get()&&(screen==4||screen==5)){
            jobs.cancel();done();toast_until=0;
        }
        if(screen==6){
            if(compass_saving)return BackAction::stay;
            heading.model.capture.collecting=false;heading.model.capture.candidate={};
            show(compass_return);return BackAction::stay;
        }
        if(screen==3){show(settings_return_screen==3?0:settings_return_screen);return BackAction::stay;}
        if(screen==4){show(search_return_screen==4?0:search_return_screen);return BackAction::stay;}
        if(screen==7){
            results=detail_results;query.set(detail_query);search_return_screen=detail_search_return;
            search_start=false;search_attempted=true;search_failed=false;show(4);request_map(detail_previous_camera);return BackAction::stay;
        }
        if(screen==8){show(1);return BackAction::stay;}
        if(screen==5){show(plan_return_screen==5?0:plan_return_screen);return BackAction::stay;}
        if(screen==2){show(route.points.empty()?0:1);return BackAction::stay;}
        if((screen==0||screen==1)&&live){show(8);return BackAction::stay;}
        if(screen==1){invalidate_route();image.reset();show(0);if(!active_key.empty())request_map(camera);return BackAction::stay;}
        if(screen!=0){show(0);return BackAction::stay;}return BackAction::close;
    }
    void on_background(Context&) {close_compass();compass_probed=false;compass_settings_loaded=false;jobs.cancel();location_jobs.cancel();map_jobs.cancel();if(bookmark_pending)bookmark_dirty=true;settings_jobs.cancel();bookmark_pending=false;map_pending=false;requested_map.reset();++map_generation;location_pending=false;stop();pointer_down=false;dragging=false;pan_x=pan_y=0;if(initialized)done();}
    void on_stop(Context&) {close_compass();jobs.cancel();location_jobs.cancel();map_jobs.cancel();settings_jobs.cancel();}
    void on_error(Context&,Error) {done();message("操作失败，请检查网络或设置");}
