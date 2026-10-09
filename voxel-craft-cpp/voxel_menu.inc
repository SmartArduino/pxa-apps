    void draw_menu(pxa::game::Renderer& target_renderer)noexcept {
        using enum voxel::Screen;using A=voxel::UiAction;
        auto frame=target_renderer.frame(commands);frame.clear({0x10c3});
        const float sx=float(target_renderer.info().render_width)/hud.width*16.f;
        const float sy=float(target_renderer.info().render_height)/hud.height*16.f;
        auto qx=[sx](float x){return std::int16_t(std::lround(x*sx));};
        auto qy=[sy](float y){return std::int16_t(std::lround(y*sy));};
        auto rectangle=[&](voxel::Rect r,std::uint16_t color){frame.quad({qx(r.x),qy(r.y),qx(r.x+r.w),qy(r.y),qx(r.x+r.w),qy(r.y+r.h),qx(r.x),qy(r.y+r.h)},{color});};
        auto text_width=[](std::string_view text,float scale){int w=0;for(std::size_t at=0;at<text.size();){auto c=voxel::font_next(text,at);w+=c<128?8:12;}return float(w)*scale;};
        auto text=[&](std::string_view message,float x,float y,float scale){
            // Fixed small batches use 16 bytes per glyph; INDEX8 is interpreted
            // as grayscale coverage, blended against the actual background.
            std::array<pxa::game::Sprite,8> batch{};unsigned count=0;
            auto flush=[&]{if(count){frame.sprites({46},std::span{batch}.first(count),{.transparent_index0=true,.solid_color=true,.texel_alpha=true,.color={0xffff}});count=0;}};
            const int cell=font_size;
            for(std::size_t at=0;at<message.size();){auto c=voxel::font_next(message,at);auto index=voxel::font_index(c);
                if(index>=0&&c!=' '){batch[count++]={
                    .x=std::int16_t(std::lround(x*sx/16)),.y=std::int16_t(std::lround(y*sy/16)),
                    .width=std::uint16_t(std::max(1l,std::lround(12*scale*sx/16))),.height=std::uint16_t(std::max(1l,std::lround(12*scale*sy/16))),
                    .source_x=std::uint16_t((index%11)*cell),.source_y=std::uint16_t((index/11)*cell),
                    .source_width=std::uint16_t(cell),.source_height=std::uint16_t(cell)};
                    if(count==batch.size())flush();
                }x+=(c<128?8:12)*scale;
            }flush();
        };
        auto centered=[&](std::string_view value,int y,float scale){
            auto span=controls.horizontal(y,std::max(1,int(std::ceil(12*scale))));
            const float available=std::max(1,span.second-span.first-8);
            const float wanted=text_width(value,scale);if(wanted>available)scale*=available/wanted;
            text(value,(hud.width-text_width(value,scale))/2.f,float(y),scale);
        };
        const int margin=std::max(4,int(7*controls.scale));
        const int top=int(display.safe.top)+margin,bottom=hud.height-int(display.safe.bottom)-margin;
        const float font=std::min(controls.text_scale,controls.scale*1.18f);
        const int button_h=std::max(20,int(29*controls.scale));
        const int gap=std::max(3,int(6*controls.scale));
        const int wanted=std::min(int(190*controls.scale),hud.width-2*margin);
        unsigned previous_hits=menu_hit_count;menu_hit_count=0;(void)previous_hits;
        auto button=[&](voxel::Rect r,std::string_view label,A action,int arg=0,bool enabled=true){
            enabled=enabled&&!busy&&!initializing;
            bool pressed=enabled&&ui_pressed>=0&&unsigned(ui_pressed)==menu_hit_count;
            rectangle({r.x+2,r.y+2,r.w,r.h},0x0821);
            rectangle(r,enabled?(pressed?0x3b88:0x2266):0x18c3);
            rectangle({r.x,r.y,r.w,2},enabled?0x5db1:0x2965);
            text(label,r.x+(r.w-text_width(label,font))/2.f,r.y+(r.h-12*font)/2.f,font);
            if(enabled&&!busy&&menu_hit_count<menu_hits.size())menu_hits[menu_hit_count++]={r,action,arg};
        };
        auto heading=[&](std::string_view name){centered(name,top+int(8*controls.scale),font*1.35f);};
        if(screen==title){
            // Decorative voxel silhouette is bounded and only encoded when
            // the menu changes. No world projection, simulation or idle redraw.
            for(int i=0;i<8;++i){int w=hud.width/8+1;rectangle({i*w,0,w,std::max(1,int(display.safe.top)+int((i%3+1)*8*controls.scale))},0x1a24);}
            centered("VOXEL CRAFT",top+int(12*controls.scale),controls.scale*1.8f);
            centered("体素世界",top+int(40*controls.scale),font);
            int y=top+int(66*controls.scale);
            button(controls.row(y,button_h,wanted),"新游戏",A::new_game);y+=button_h+gap;
            button(controls.row(y,button_h,wanted),"加载存档",A::load_slots);y+=button_h+gap;
            button(controls.row(y,button_h,wanted),"设置",A::settings);
        }else if(screen==pause){
            heading("游戏菜单");int y=top+int(42*controls.scale);
            for(auto entry:std::array{std::pair{"继续",A::resume},std::pair{"保存游戏",A::save_slots},std::pair{"设置",A::settings},std::pair{"返回标题",A::title}}){button(controls.row(y,button_h,wanted),entry.first,entry.second);y+=button_h+gap;}
        }else if(screen==settings_page){
            heading("设置");int y=top+int(29*controls.scale),h=std::max(14,int(20*controls.scale));
            char sensitivity[64];std::snprintf(sensitivity,sizeof(sensitivity),"视角灵敏度 %s",settings.sensitivity==0?"低":settings.sensitivity==1?"中":"高");
            const std::array<std::pair<const char*,A>,6> choices{{{settings.flight?"飞行模式 开":"飞行模式 关",A::fly},{sensitivity,A::sensitivity},{settings.invert_y?"反转上下 开":"反转上下 关",A::invert},{settings.auto_distance?"自动视距 开":"自动视距 关",A::distance},{settings.auto_jump?"自动跳跃 开":"自动跳跃 关",A::auto_jump},{"恢复默认",A::defaults}}};
            for(auto choice:choices){button(controls.row(y,h,wanted),choice.first,choice.second);y+=h+std::max(2,gap/2);}
            button(controls.row(y+gap/2,button_h,wanted),"返回",A::back);
        }else if(screen==load_slots||screen==save_slots){
            heading(screen==load_slots?"加载存档":"保存游戏");int y=top+int(43*controls.scale);
            for(int i=0;i<voxel::kSaveSlots;++i){char label[80];std::snprintf(label,sizeof(label),"存档 %d  %s",i+1,slot_generations[i]?"已有世界":"空槽位");button(controls.row(y,button_h,wanted),label,A::slot,i,screen==save_slots||slot_generations[i]);y+=button_h+gap;}
            button(controls.row(y,button_h,wanted),"返回",A::back);
        }else if(screen==confirm_title||screen==confirm_overwrite){
            heading(screen==confirm_title?"未保存更改":"覆盖存档");
            centered(screen==confirm_title?"放弃更改返回标题":"确定覆盖此存档",top+int(60*controls.scale),font);
            int y=top+int(100*controls.scale);
            button(controls.row(y,button_h,wanted),"确定",A::confirm);y+=button_h+gap;
            button(controls.row(y,button_h,wanted),"取消",A::back);
        }else if(screen==inventory||screen==workbench){
            heading(screen==workbench?"工作台":"背包");
            // Compact touch crafting: choose a stack, tap ingredient cells,
            // then take the preview. No inventory cache or extra Surface.
            const int side=screen==workbench?3:2;
            int cell=std::max(8,int(20*controls.scale)),space=std::max(1,int(2*controls.scale));
            const int grid_y=top+int(36*controls.scale);
            cell=std::min(cell,std::max(8,(bottom-grid_y-int(12*controls.scale)-7*space)/7));
            auto icon=[&](voxel::ItemStack stack,voxel::Rect r,bool selected){
                rectangle(r,selected?0xffe0:0x8c51);
                rectangle({r.x+1,r.y+1,r.w-2,r.h-2},0x2104);
                if(stack.count){
                    if(stack.block>=voxel::kStick)voxel::draw_tool_icon(frame,stack.block,r.x+2,r.y+2,r.w-4,r.h-4);
                    else{
                    const auto texture=voxel::block_info(stack.block).texture[1];
                    std::array<pxa::game::Vertex,4> q{{
                        {.x_q4=qx(r.x+3),.y_q4=qy(r.y+3),.u_q4=0,.v_q4=0,.light=0,.depth_q8=1},
                        {.x_q4=qx(r.x+r.w-3),.y_q4=qy(r.y+3),.u_q4=256,.v_q4=0,.light=0,.depth_q8=1},
                        {.x_q4=qx(r.x+r.w-3),.y_q4=qy(r.y+r.h-3),.u_q4=256,.v_q4=256,.light=0,.depth_q8=1},
                        {.x_q4=qx(r.x+3),.y_q4=qy(r.y+r.h-3),.u_q4=0,.v_q4=256,.light=0,.depth_q8=1}}};
                    frame.textured_quad({texture},q,{.affine_uv=true,.painter=true,.transparent_index0=true});
                    }
                    if(stack.wear){const int width=std::max(1,int((r.w-4)*unsigned(stack.wear)/voxel::tool_life(stack.block)));rectangle({r.x+2,r.y+r.h-3,r.w-4,1},0x4208);rectangle({r.x+2,r.y+r.h-3,width,1},0x87e0);}
                    if(stack.count>1){char count[4];std::snprintf(count,sizeof(count),"%u",unsigned(stack.count));float small=font*.68f;text(count,r.x+r.w-text_width(count,small)-2,r.y+r.h-12*small,small);}
                }
            };
            auto hit=[&](voxel::Rect r,A action,int index){if(menu_hit_count<menu_hits.size())menu_hits[menu_hit_count++]={r,action,index};};
            const int craft_x=(hud.width-(side+3)*(cell+space))/2;
            if(inventory_source>=0&&this->inventory.slots[inventory_source].block>=voxel::kStick)
                centered(voxel::item_name(this->inventory.slots[inventory_source].block),top+int(23*controls.scale),font*.8f);
            text("合成",craft_x,grid_y-12*font*.8f,font*.8f);
            for(int y=0;y<side;++y)for(int x=0;x<side;++x){
                voxel::Rect r{craft_x+x*(cell+space),grid_y+y*(cell+space),cell,cell};
                icon(crafting.ingredient(this->inventory,y*3+x),r,false);hit(r,A::craft_cell,y*3+x);
            }
            const int middle=grid_y+(side*(cell+space)-space-cell)/2;
            const int arrow_x=craft_x+side*(cell+space)+space;
            rectangle({arrow_x,middle+cell/2-1,cell-3,3},0xad55);
            rectangle({arrow_x+cell-7,middle+cell/2-4,3,9},0xad55);
            voxel::Rect output{craft_x+(side+2)*(cell+space),middle,cell,cell};
            text("产物",output.x,grid_y-12*font*.8f,font*.8f);
            icon(crafting.result(this->inventory,side),output,false);hit(output,A::craft,0);
            const int bag_y=grid_y+3*(cell+space)+int(10*controls.scale);
            const int bag_x=(hud.width-(9*cell+8*space))/2;
            for(int i=0;i<voxel::kBagCount;++i){
                const int index=i+kQuickCount;auto stack=this->inventory.slots[index];stack.count=crafting.available(this->inventory,index);
                voxel::Rect r{bag_x+(i%9)*(cell+space),bag_y+(i/9)*(cell+space),cell,cell};
                icon(stack,r,index==inventory_source);hit(r,A::item,index);
            }
            const int quick_y=bag_y+3*(cell+space)+space;
            const int quick_x=(hud.width-(kQuickCount*cell+(kQuickCount-1)*space))/2;
            for(int i=0;i<kQuickCount;++i){
                auto stack=this->inventory.slots[i];stack.count=crafting.available(this->inventory,i);
                voxel::Rect r{quick_x+i*(cell+space),quick_y,cell,cell};
                icon(stack,r,i==inventory_source||i==hotbar);hit(r,A::item,i);
            }
            // Close sits at the top safe edge, so it remains accessible on
            // short displays and never overlaps the bottom inventory row.
            auto span=controls.horizontal(top,std::max(16,int(20*controls.scale)));
            const int close_w=std::max(24,int(36*controls.scale)),close_h=std::max(16,int(20*controls.scale));
            button({span.second-close_w-4,top,close_w,close_h},"返回",A::back);
#if VOXEL_VALIDATE
            if(app_context){char msg[240];std::snprintf(msg,sizeof(msg),"VOXEL-CRAFT side=%d cell=%d space=%d grid=%d,%d bag=%d,%d quick=%d,%d output=%d,%d back=%d,%d",side,cell,space,craft_x,grid_y,bag_x,bag_y,quick_x,quick_y,output.cx(),output.cy(),span.second-close_w/2-4,top+close_h/2);(void)app_context->log().write(pxa::LogLevel::info,msg);}
#endif
        }
        if(busy)centered(ui_status,bottom-int(12*font),font*.85f);
        else if(*ui_status){
            // A short status is kept on screen, never hidden solely in a toast.
            centered(ui_status,bottom-int(10*font),font*.8f);
        }
        const auto submitted=frame.submit();ui_dirty=!submitted;
        if(!submitted&&app_context){char error[80];std::snprintf(error,sizeof(error),"Menu draw failed: %d bytes=%u",int(submitted.error()),unsigned(frame.bytes_used()));(void)app_context->log().write(pxa::LogLevel::error,error);}
#if VOXEL_VALIDATE
        if(app_context){char message[200];int n=std::snprintf(message,sizeof(message),"VOXEL-UI screen=%u busy=%u slots=%u fly=%u font=%d button=%d menu=%d,%d bag=%d,%d hits=%u audio=%u",unsigned(screen),unsigned(busy),unsigned(bool(slot_generations[0]))|(unsigned(bool(slot_generations[1]))<<1)|(unsigned(bool(slot_generations[2]))<<2),unsigned(flying),font_size,hud.button,controls.menu.cx(),controls.menu.cy(),controls.bag.cx(),controls.bag.cy(),menu_hit_count,unsigned(bool(audio)));if(n>0&&n<int(sizeof(message)))(void)app_context->log().write(pxa::LogLevel::info,std::string_view(message,n));}
#endif
    }
