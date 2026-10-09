    static constexpr int kQuickCount=voxel::kQuickCount;
    std::array<std::uint8_t,kQuickCount> quickbar{1,2,3,4,5,6,8,12};
    pxa::Context* app_context=nullptr;
    pxa::ui::DisplayMetrics display;
    voxel::ControlLayout controls=voxel::ControlLayout::make(display);
    voxel::Settings settings;
    voxel::Screen screen=(VOXEL_BENCH_SCENE||VOXEL_BENCH_DISTANCE)?voxel::Screen::game:voxel::Screen::title;
    voxel::Screen settings_return=voxel::Screen::title,slots_return=voxel::Screen::title;
    std::array<voxel::MenuHit,32> menu_hits{};
    unsigned menu_hit_count=0;
    int ui_pointer=-1,ui_pressed=-1,pending_slot=0;
    std::uint32_t world_seed=0x5ae1;
    std::array<std::uint32_t,voxel::kSaveSlots> slot_generations{};
    int font_size=10;
    bool mine_long_pressed=false;
    bool session_ready=false,ui_dirty=true,busy=false,dirty_save=false,catalog_ready=false;
    const char* ui_status="";

    void apply_display(pxa::ui::DisplayMetrics value)noexcept {
        display=value;controls=voxel::ControlLayout::make(value);
        hud.width=controls.width();hud.height=controls.height();hud.slot=controls.slot;hud.slot_gap=controls.gap;
        hud.hotbar_x=controls.hotbar.x;hud.hotbar_y=controls.hotbar.y;hud.button=controls.actions[0].w;
        hud.button_gap=std::max(2,int(6*controls.scale));hud.button_x=controls.actions[0].x;hud.button_y=controls.actions[0].y;
        cancel_controls();menu_hit_count=0;ui_dirty=true;
    }
    void cancel_controls()noexcept {
        stick_active=look_active=mining=up_held=down_held=false;
        stick_pointer=look_pointer=mine_pointer=up_pointer=down_pointer=-1;
        move_right=move_forward=0;mine_timer=0;mine_long_pressed=false;last_draw_tick_us=0;ui_pointer=ui_pressed=-1;
    }
    void show(voxel::Screen next)noexcept {
        cancel_controls();screen=next;ui_dirty=true;menu_hit_count=0;ui_status="";
        fps_window_frames=fps_window_us=0;
    }
    void new_game()noexcept {
        world_seed=next_random();world.generate(world_seed);respawn();
        flying=settings.flight;on_ground=false;quickbar={1,2,3,4,5,6,8,12};hotbar=0;
        for(auto& p:particles.items())p.life=0;
        session_ready=true;dirty_save=true;budget_max_distance=32;low_quality=false;show(voxel::Screen::game);
    }
    void begin(pxa::Task<void> task) {
        if(!app_context)return;
        busy=true;ui_dirty=true;auto started=app_context->tasks().start(std::move(task));
        if(!started){busy=false;ui_status="失败";ui_dirty=true;}
    }
    pxa::Task<void> read_catalog() {
        std::array<std::byte,8> cfg{};
        auto saved=co_await app_context->storage().get("voxel.settings.v1",cfg);
        if(saved&&*saved==cfg.size()&&(cfg[0]==std::byte{1}||cfg[0]==std::byte{2})&&std::to_integer<unsigned>(cfg[1])<=2&&std::to_integer<unsigned>(cfg[2])<=1&&std::to_integer<unsigned>(cfg[3])<=1&&std::to_integer<unsigned>(cfg[4])<=1&&std::to_integer<unsigned>(cfg[5])<=1)
            settings={std::to_integer<std::uint8_t>(cfg[1]),cfg[2]!=std::byte{},cfg[3]!=std::byte{},cfg[4]!=std::byte{},cfg[0]==std::byte{1}||cfg[5]!=std::byte{}};
        for(int i=0;i<voxel::kSaveSlots;++i){
            char key[24];std::snprintf(key,sizeof(key),"voxel.slot.%d",i);
            voxel::SaveHeader header{};auto got=co_await app_context->storage().get(key,header);
            if(got&&*got==header.size())if(auto state=voxel::decode_save(header))slot_generations[i]=state->generation;
        }
        catalog_ready=true;busy=false;ui_dirty=true;co_return pxa::Result<void>{};
    }
    pxa::Task<void> write_settings() {
        std::array<std::byte,8> cfg{};cfg[0]=std::byte{2};cfg[1]=std::byte(settings.sensitivity);
        cfg[2]=std::byte(settings.invert_y);cfg[3]=std::byte(settings.auto_distance);cfg[4]=std::byte(settings.flight);cfg[5]=std::byte(settings.auto_jump);
        auto result=co_await app_context->storage().set("voxel.settings.v1",cfg);
        busy=false;ui_status=result?"保存成功":"保存失败";ui_dirty=true;co_return pxa::Result<void>{};
    }
    static void save_path(char* out,std::size_t n,int slot,std::uint32_t generation)noexcept {
        std::snprintf(out,n,"voxel-slot-%d-%c.bin",slot,(generation&1)?'b':'a');
    }
    static pxa::Result<void> write_all(pxa::File& file,std::span<const std::byte> bytes)noexcept {
        while(!bytes.empty()){auto n=file.write(bytes);if(!n)return std::unexpected(n.error());if(!*n)return std::unexpected(pxa::Error::internal);bytes=bytes.subspan(*n);}return {};
    }
    static pxa::Result<void> read_all(pxa::File& file,std::span<std::byte> bytes)noexcept {
        while(!bytes.empty()){auto n=file.read(bytes);if(!n)return std::unexpected(n.error());if(!*n)return std::unexpected(pxa::Error::protocol_error);bytes=bytes.subspan(*n);}return {};
    }
    pxa::Task<void> save_slot(int slot) {
        if(slot<0||slot>=voxel::kSaveSlots||!session_ready){busy=false;co_return std::unexpected(pxa::Error::bad_state);}
        std::uint32_t generation=slot_generations[slot]+1;if(!generation)generation=2;
        voxel::SaveState state{camera,velocity_y,world_seed,generation,voxel::checksum(world.saved_blocks()),quickbar,std::uint8_t(hotbar),flying};
        auto header=voxel::encode_save(state);char path[32],key[24];save_path(path,sizeof(path),slot,generation);
        std::snprintf(key,sizeof(key),"voxel.slot.%d",slot);
        bool success=false;
        auto file=co_await app_context->fs().open(path,pxa::OpenMode::write|pxa::OpenMode::create|pxa::OpenMode::truncate);
        if(file){
            auto written=write_all(*file,header);
            auto blocks=world.saved_blocks();
            for(std::size_t at=0;written&&at<blocks.size();at+=1024){
                written=write_all(*file,blocks.subspan(at,std::min<std::size_t>(1024,blocks.size()-at)));
                {auto yielded=co_await app_context->clock().now();if(!yielded){written=std::unexpected(yielded.error());break;}}
            }
            file->close();
            if(written){
                // Verify the inactive file before atomically publishing its
                // generation in Storage. Existing committed data stays intact.
                auto verify=co_await app_context->fs().open(path);
                if(verify){
                    auto temp=world.load_staging();voxel::SaveHeader observed{};
                    auto read=read_all(*verify,observed);
                    for(std::size_t at=0;read&&at<temp.size();at+=1024){
                        read=read_all(*verify,temp.subspan(at,std::min<std::size_t>(1024,temp.size()-at)));
                        {auto yielded=co_await app_context->clock().now();if(!yielded){read=std::unexpected(yielded.error());break;}}
                    }
                    success=read&&observed==header&&voxel::checksum(temp)==state.blocks_hash;
                    world.finish_load(false);verify->close();
                }
                if(success){auto committed=co_await app_context->storage().set(key,header);success=bool(committed);}
            }
        }
        if(success){slot_generations[slot]=generation;dirty_save=false;}
        busy=false;ui_dirty=true;ui_status=success?"保存成功":"保存失败";
        if(app_context){char msg[80];std::snprintf(msg,sizeof(msg),"VOXEL-SAVE slot=%d generation=%u ok=%u",slot,generation,unsigned(success));(void)app_context->log().write(pxa::LogLevel::info,msg);}
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> load_slot(int slot) {
        bool success=false;
        if(slot>=0&&slot<voxel::kSaveSlots&&slot_generations[slot]){
            char path[32],key[24];save_path(path,sizeof(path),slot,slot_generations[slot]);std::snprintf(key,sizeof(key),"voxel.slot.%d",slot);
            voxel::SaveHeader committed{},header{};auto record=co_await app_context->storage().get(key,committed);
            auto file=co_await app_context->fs().open(path);
            if(record&&*record==committed.size()&&file){
                auto staged=world.load_staging();auto read=read_all(*file,header);
                auto state=voxel::decode_save(header);
                for(std::size_t at=0;read&&state&&header==committed&&at<staged.size();at+=1024){
                    read=read_all(*file,staged.subspan(at,std::min<std::size_t>(1024,staged.size()-at)));
                    {auto yielded=co_await app_context->clock().now();if(!yielded){read=std::unexpected(yielded.error());break;}}
                }
                std::byte extra{};auto eof=file->read({&extra,1});
                success=read&&state&&header==committed&&eof&&!*eof&&voxel::valid_saved_blocks(staged)&&voxel::checksum(staged)==state->blocks_hash;
                world.finish_load(success);file->close();
                if(success){camera=state->camera;velocity_y=state->velocity;world_seed=state->seed;quickbar=state->quick;hotbar=state->selected;flying=state->flying;settings.flight=flying;on_ground=false;session_ready=true;dirty_save=false;for(auto& p:particles.items())p.life=0;show(voxel::Screen::game);}
            }
        }
        busy=false;ui_dirty=true;ui_status=success?"":"存档损坏或读取失败";
        if(app_context){char msg[64];std::snprintf(msg,sizeof(msg),"VOXEL-LOAD slot=%d ok=%u",slot,unsigned(success));(void)app_context->log().write(pxa::LogLevel::info,msg);}
        co_return pxa::Result<void>{};
    }
    void menu_action(voxel::UiAction action,int argument=0) {
        using enum voxel::Screen;using A=voxel::UiAction;
        if(busy||initializing)return;
        switch(action){
        case A::new_game:new_game();break;
        case A::load_slots:slots_return=screen;show(load_slots);break;
        case A::save_slots:slots_return=pause;show(save_slots);break;
        case A::settings:settings_return=screen;show(settings_page);break;
        case A::resume:show(game);break;
        case A::title:show(dirty_save?confirm_title:title);break;
        case A::slot:
            if(screen==load_slots){if(slot_generations[argument]){ui_status="正在读取";begin(load_slot(argument));}}
            else{pending_slot=argument;if(slot_generations[argument])show(confirm_overwrite);else{ui_status="正在保存";begin(save_slot(argument));}}break;
        case A::confirm:
            if(screen==confirm_title){session_ready=false;show(title);}
            else{show(save_slots);ui_status="正在保存";begin(save_slot(pending_slot));}break;
        case A::back:
            if(screen==settings_page){show(settings_return);begin(write_settings());}
            else if(screen==load_slots||screen==save_slots)show(slots_return);
            else if(screen==confirm_overwrite)show(save_slots);
            else if(screen==confirm_title)show(pause);
            else show(game);break;
        case A::fly:settings.flight=!settings.flight;if(session_ready){flying=settings.flight;velocity_y=0;dirty_save=true;}ui_dirty=true;break;
        case A::sensitivity:settings.sensitivity=(settings.sensitivity+1)%3;ui_dirty=true;break;
        case A::invert:settings.invert_y=!settings.invert_y;ui_dirty=true;break;
        case A::distance:settings.auto_distance=!settings.auto_distance;budget_max_distance=32;low_quality=false;ui_dirty=true;break;
        case A::auto_jump:settings.auto_jump=!settings.auto_jump;ui_dirty=true;break;
        case A::defaults:settings={};if(session_ready){flying=false;velocity_y=0;}budget_max_distance=32;ui_dirty=true;break;
        case A::quick_slot:hotbar=argument;ui_dirty=true;break;
        case A::item:quickbar[hotbar]=std::uint8_t(argument);dirty_save=true;ui_status="已放入快捷栏";ui_dirty=true;break;
        default:break;
        }
    }
    pxa::BackAction on_back() {
        using enum voxel::Screen;
        if(busy)return pxa::BackAction::stay;
        if(screen==title)return pxa::BackAction::close;
        if(screen==game)show(pause);
        else if(screen==pause)show(game);
        else menu_action(voxel::UiAction::back);
        return pxa::BackAction::stay;
    }
    pxa::Result<void> on_start(pxa::Context& ctx,std::span<const std::byte> config) {
        app_context=&ctx;if(auto m=pxa::ui::decode_start_display(config))apply_display(*m);else apply_display(display);
        return {};
    }
    pxa::Result<bool> on_event(pxa::Context&,const pxa::Event& event) {
        if(event.service==3&&event.opcode==0x8002&&event.token==0){
            auto metrics=pxa::ui::decode_display_metrics(event.payload);
            if(!metrics)return std::unexpected(metrics.error());apply_display(*metrics);return true;
        }return false;
    }
    void on_window_changed(const pxa::WindowMetrics& window)noexcept {
        auto updated=display;updated.width=window.logical_width;updated.height=window.logical_height;
        updated.safe.left=std::max(updated.safe.left,window.safe_insets.left);
        updated.safe.top=std::max(updated.safe.top,window.safe_insets.top);
        updated.safe.right=std::max(updated.safe.right,window.safe_insets.right);
        updated.safe.bottom=std::max(updated.safe.bottom,window.safe_insets.bottom);apply_display(updated);
    }
