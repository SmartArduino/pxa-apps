// SPDX-License-Identifier: MIT
// Native controls share the Canvas hit boxes. Gen4's ordinary focus group and
// system IME own key handling; the application does not capture volume keys.
    struct KeyTarget {
        navui::Rect box{};
        std::string_view label;
        bool visible=false,enabled=false;
        bool operator==(const KeyTarget&)const=default;
    };
    struct KeyBinding { unsigned action, screens; };
    static constexpr KeyBinding key_bindings[]={
        {navui::SelectStart,63},{navui::SelectEnd,63},{navui::Swap,63},{navui::Locate,1u<<5},
        {navui::Walk,63},{navui::Bike,63},{navui::Drive,63},{navui::Demo,63},{navui::Start,63},
        {navui::Book,63},{navui::Overview,63},{navui::Plan,63},{navui::Search,63},
        {navui::Plus,63},{navui::Minus,63},{navui::Locate,63u^(1u<<5)},
        {navui::RetryMap,63},{navui::Settings,63},{navui::Submit,63},
        {navui::MapSettings,63},{navui::LocationSettings,63},{navui::CitySettings,63},
        {navui::WifiSource,63},{navui::ForwardSource,63},{navui::ClearField,63},{navui::Save,63},
        {navui::Previous,63},{navui::Next,63},
        {navui::OpenCompass,63},{navui::CalibrateCompass,64},{navui::SaveCompassCalibration,64},
        {navui::CancelCompassCalibration,64},{navui::RetryCompass,64},
        {navui::FirstMagnetometer,64},{navui::SecondMagnetometer,64}
    };
    static constexpr unsigned key_count=sizeof(key_bindings)/sizeof(*key_bindings);
    struct KeyState:State<KeyTarget>{KeyState():State<KeyTarget>(KeyTarget{}){}};
    std::array<KeyState,key_count> key_targets{};
    std::array<KeyState,7> key_backs{};
    struct KeyClick {
        NavigationApp* app=nullptr;unsigned id=0;
        void operator()(){if(app)app->action(id);}
    };
    std::array<KeyClick,key_count> key_clicks{};
    std::array<KeyClick,7> key_back_clicks{};
    std::string_view key_label(unsigned id)const{
        using namespace navui;
        switch(id){
        case OpenCompass:return "指南针";case CalibrateCompass:return "六面校准";
        case SaveCompassCalibration:return "保存校准";case CancelCompassCalibration:return "取消校准";
        case RetryCompass:return "重新检测";case FirstMagnetometer:return "磁力计 1";case SecondMagnetometer:return "磁力计 2";
        case SelectStart:return "选择起点";case SelectEnd:return "选择目的地";
        case Locate:return "当前位置";case Swap:return "交换起终点";
        case Walk:return "步行";case Bike:return "骑行";case Drive:return "驾车";
        case Demo:return route.points.empty()?"模拟导航":"再走一次";
        case Start:return screen==5?(active_key.empty()?"连接地图服务":"查看路线"):live?"结束导航":"开始导航";
        case Book:return "行程路书";case Overview:return "路线全览";case Plan:return "路线规划";
        case Search:return "搜索地点";case Plus:return "放大地图";case Minus:return "缩小地图";
        case RetryMap:return "重试底图";case Settings:return "偏好设置";case Submit:return "搜索";
        case MapSettings:return "地图服务";case LocationSettings:return "定位来源";case CitySettings:return "搜索城市";
        case WifiSource:return "免费 Wi-Fi";case ForwardSource:return "网络转发";case ClearField:return "清空输入";
        case Save:return "保存设置";case Previous:return "上一段";case Next:return "下一段";
        default:return "返回";
        }
    }
    static bool write_key_target(Transaction<>& tx,std::uint32_t node,const KeyTarget& s){
        return tx.u8(node,protocol::visible,s.visible)&&tx.u8(node,protocol::enabled,s.enabled)&&
            tx.logical_px(node,protocol::x,s.box.x)&&tx.logical_px(node,protocol::y,s.box.y)&&
            tx.logical_px(node,protocol::width,std::max(1,s.box.w))&&
            tx.logical_px(node,protocol::height,std::max(1,s.box.h))&&tx.text(node,s.label);
    }
    void sync_key_targets(){
        auto target=[&](unsigned id,bool active){
            KeyTarget t;t.label=key_label(id);
            if(active)for(unsigned i=0;i<paint.hit_count;++i)if(paint.hits[i].action==id){t.box=paint.hits[i].box;t.visible=true;break;}
            t.enabled=t.visible&&(!compass_saving||screen!=6)&&(idle.get()||id==navui::Back||id==navui::Book||id==navui::Settings||
                id==navui::Previous||id==navui::Next||id==navui::MapSettings||id==navui::LocationSettings||id==navui::CitySettings);
            return t;
        };
        for(unsigned i=0;i<key_count;++i){const auto& binding=key_bindings[i];auto t=target(binding.action,(binding.screens&(1u<<screen))!=0);if(key_targets[i].get()!=t)key_targets[i].set(t);}
        for(unsigned i=0;i<key_backs.size();++i){auto t=target(navui::Back,screen==i);t.label=i==0?"退出地图":"返回";if(key_backs[i].get()!=t)key_backs[i].set(t);}
    }
    struct KeyControls:ViewModifiers {
        static constexpr Capacity capacity{key_count,key_count,key_count};
        NavigationApp* app;
        explicit KeyControls(NavigationApp* a):app(a){}
        template<class Owner> bool render(Owner& owner,std::uint32_t parent){
            for(unsigned i=0;i<key_count;++i){
                auto node=owner.create(parent,protocol::control,protocol::button);auto& tx=owner.transaction();
                app->key_clicks[i]={app,key_bindings[i].action};
                if(!node||!tx.u8(node,protocol::position,1)||!tx.rgba(node,protocol::background,0)||
                    !tx.rgba(node,protocol::foreground,0)||!tx.dp(node,protocol::border_width,0)||!tx.dp(node,protocol::radius,12)||
                    !tx.u64(node,protocol::event_mask,1)||!write_key_target(tx,node,app->key_targets[i].get())||
                    !owner.template bind<KeyTarget,write_key_target>(app->key_targets[i],node)||!owner.on_click(node,app->key_clicks[i]))return false;
            }
            return true;
        }
    };
    struct KeyBackControls:ViewModifiers {
        static constexpr Capacity capacity{7,7,7};NavigationApp* app;
        explicit KeyBackControls(NavigationApp* a):app(a){}
        template<class Owner> bool render(Owner& owner,std::uint32_t parent){
            for(unsigned i=0;i<7;++i){auto node=owner.create(parent,protocol::control,protocol::button);auto& tx=owner.transaction();app->key_back_clicks[i]={app,navui::Back};
                if(!node||!tx.u8(node,protocol::position,1)||!tx.rgba(node,protocol::background,0)||!tx.rgba(node,protocol::foreground,0)||
                    !tx.dp(node,protocol::border_width,0)||!tx.dp(node,protocol::radius,18)||!tx.u64(node,protocol::event_mask,1)||
                    !write_key_target(tx,node,app->key_backs[i].get())||!owner.template bind<KeyTarget,write_key_target>(app->key_backs[i],node)||
                    !owner.on_click(node,app->key_back_clicks[i]))return false;
            }return true;
        }
    };
    struct NativeResultRow { std::uint32_t button=0,title=0,address=0;std::string name,detail;bool operator==(const NativeResultRow&)const=default; };
    struct NativeResults {
        navui::Rect box{};bool visible=false;unsigned count=0;
        std::array<NativeResultRow,5> rows{};
        bool operator==(const NativeResults&)const=default;
    };
    State<NativeResults> native_results{NativeResults{}};
    struct ResultClick {NavigationApp* app=nullptr;unsigned index=0;void operator()(){if(app)app->choose(index);}};
    std::array<ResultClick,5> result_clicks{};
    static bool write_results(Transaction<>& tx,std::uint32_t root,const NativeResults& s){
        if(!tx.u8(root,protocol::visible,s.visible)||!tx.logical_px(root,protocol::x,s.box.x)||!tx.logical_px(root,protocol::y,s.box.y)||
            !tx.logical_px(root,protocol::width,std::max(1,s.box.w))||!tx.logical_px(root,protocol::height,std::max(1,s.box.h)))return false;
        for(unsigned i=0;i<s.rows.size();++i){const auto& row=s.rows[i];
            if(!tx.u8(row.button,protocol::visible,s.visible&&i<s.count)||!tx.text(row.title,row.name)||!tx.text(row.address,row.detail))return false;
        }return true;
    }
    void sync_results(){
        const bool visible=screen==4&&!results.empty();if(!visible&&!native_results.get().visible)return;
        auto state=native_results.get();state.visible=visible;state.count=std::min<std::size_t>(results.size(),state.rows.size());
        state.box={safe.x+12,safe.y+104,safe.w-24,std::max(1,safe.h-112)};
        if(visible)for(unsigned i=0;i<state.count;++i){state.rows[i].name=navui::Painter::shorten(results[i].name,state.box.w-32,18);state.rows[i].detail=navui::Painter::shorten(results[i].address,state.box.w-32,13);}
        if(state!=native_results.get())native_results.set(std::move(state));
    }
    struct ResultControls:ViewModifiers {
        static constexpr Capacity capacity{17,1,5};NavigationApp* app;
        explicit ResultControls(NavigationApp* a):app(a){}
        template<class Owner> bool render(Owner& owner,std::uint32_t parent){
            auto& tx=owner.transaction();auto root=owner.create(parent,3);auto content=owner.create(root,protocol::box);
            if(!root||!content||!tx.u8(root,protocol::position,1)||!tx.u8(root,protocol::scroll_axis,2)||!tx.u64(root,protocol::event_mask,8)||
                !owner.padding(root,3)||!tx.u8(content,protocol::layout,protocol::column)||!tx.dp(content,protocol::gap,7)||!tx.fill(content,protocol::width))return false;
            auto state=app->native_results.get();
            for(unsigned i=0;i<state.rows.size();++i){auto& row=state.rows[i];row.button=owner.create(content,protocol::control,protocol::button);
                row.title=owner.create(row.button,protocol::text);row.address=owner.create(row.button,protocol::text);app->result_clicks[i]={app,i};
                if(!row.button||!row.title||!row.address||!tx.fill(row.button,protocol::width)||!tx.logical_px(row.button,protocol::height,59)||
                    !tx.u8(row.button,protocol::layout,protocol::column)||!tx.dp(row.button,protocol::gap,3)||!owner.padding(row.button,7)||
                    !tx.rgba(row.button,protocol::background,app->colors.surface)||!tx.dp(row.button,protocol::radius,12)||
                    !tx.u64(row.button,protocol::event_mask,1)||!owner.on_click(row.button,app->result_clicks[i])||
                    !tx.text(row.title,"")||!tx.fill(row.title,protocol::width)||!tx.u16(row.title,protocol::font_role,std::uint16_t(Font::body))||
                    !tx.rgba(row.title,protocol::foreground,app->colors.ink)||!tx.text(row.address,"")||!tx.fill(row.address,protocol::width)||
                    !tx.u16(row.address,protocol::font_role,std::uint16_t(Font::caption))||!tx.rgba(row.address,protocol::foreground,app->colors.muted))return false;
            }
            app->native_results.set(state);
            return write_results(tx,root,state)&&owner.template bind<NativeResults,write_results>(app->native_results,root);
        }
    };
