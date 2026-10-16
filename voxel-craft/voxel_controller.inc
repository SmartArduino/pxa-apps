    // Touch and controller share actions, while each held mine has one owner.
    static constexpr int controller_mine=-2;
    void begin_mining(int owner)noexcept {
        if(mine_pointer!=-1)return;
        mine_pointer=owner;mining=true;mine_timer=0;mine_long_pressed=false;digging.reset();
    }
    void finish_mining(bool use)noexcept {
        use=use&&!mine_long_pressed;
        mining=false;mine_pointer=-1;mine_timer=0;mine_long_pressed=false;digging.reset();
        if(use){const auto hit=target();if(hit.hit&&world.at(hit.x,hit.y,hit.z)==voxel::kTable)show(voxel::Screen::workbench);}
    }
    void controller_back(pxa::Context& context) {
        if(on_back()==pxa::BackAction::close)(void)context.window().request_back();
    }
    void on_controller(pxa::Context& context,const pxa::input::ControllerState& state) {
        using namespace pxa::input;
        using enum voxel::Screen;
        if(!controller.update(state))return;
        controller_blocked&=controller.buttons();
        if(!state.connected){
            controller_yaw.reset();controller_pitch.reset();place_repeat_seconds=0;
            if(mine_pointer==controller_mine)finish_mining(false);
            controller_wait_release=true;controller_navigation=false;menu_repeat_us=0;ui_dirty=true;return;
        }
        if(controller_wait_release){controller_blocked=controller.buttons();controller_wait_release=false;}
        if(busy||initializing){controller_blocked|=controller.buttons();return;}
        const auto buttons=controller.buttons()&~controller_blocked;
        if(!(buttons&controller_b)||(buttons&controller_select))place_repeat_seconds=0;
        auto pressed=[&](std::uint32_t mask){return controller.pressed(mask&~controller_blocked);};
        if(screen!=game){
            if(!buttons)menu_repeat_us=0;
            else if(!controller_navigation){controller_navigation=true;ui_dirty=true;}
            if(pressed(controller_b)||(screen==pause&&pressed(controller_start))||
               ((screen==inventory||screen==workbench)&&pressed(controller_y))){controller_back(context);return;}
            if(!menu_hit_count)return;
            if(menu_focus<0||unsigned(menu_focus)>=menu_hit_count)menu_focus=0;
            constexpr auto directions=controller_up|controller_down|controller_left|controller_right;
            const auto held=buttons&directions;
            if(held&&(pressed(directions)||!menu_repeat_us||state.timestamp_us>=menu_repeat_us)){
                const int dx=int(bool(held&controller_right))-int(bool(held&controller_left));
                const int dy=int(bool(held&controller_down))-int(bool(held&controller_up));
                const int next=voxel::menu_neighbor(std::span{menu_hits}.first(menu_hit_count),menu_focus,dx,dy);
                if(next!=menu_focus){menu_focus=next;ui_dirty=true;}
                menu_repeat_us=state.timestamp_us+(pressed(directions)?350000u:100000u);
            }else if(!held)menu_repeat_us=0;
            if(pressed(controller_l|controller_r)){
                const int delta=int(pressed(controller_r))-int(pressed(controller_l));
                menu_focus=(menu_focus+delta+int(menu_hit_count))%int(menu_hit_count);ui_dirty=true;
            }
            if(pressed(controller_a)){
                const auto hit=menu_hits[menu_focus];menu_action(hit.action,hit.argument);
            }else if(pressed(controller_x)&&(screen==inventory||screen==workbench)){
                inventory_source=-1;ui_dirty=true;
                const auto hit=menu_hits[menu_focus];
                if(hit.action==voxel::UiAction::craft_cell)menu_action(hit.action,hit.argument);
            }
            return;
        }
        if(pressed(controller_start)){controller_navigation=true;show(pause);return;}
        if(buttons&controller_select){
            if(pressed(controller_a|controller_b)){
                cycle_hotbar(int(pressed(controller_b))-int(pressed(controller_a)));
                controller_blocked|=buttons&(controller_a|controller_b);dirty_save=true;
            }
        }else{
            if(pressed(controller_y)){controller_navigation=true;show(inventory);return;}
            if(pressed(controller_a)&&!flying&&on_ground){velocity_y=kJumpSpeed;on_ground=false;}
            if(pressed(controller_b)){(void)place();place_repeat_seconds=.35f;}
            if(pressed(controller_x))begin_mining(controller_mine);
        }
        if(controller.released(controller_x)&&mine_pointer==controller_mine)finish_mining(true);
    }
