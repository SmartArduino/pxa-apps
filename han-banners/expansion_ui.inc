    static constexpr int rosterSize=5;
    int rosterCount() const {return campaign.guest?commanderSlots:officerCount;}
    int rosterStart() const {return officerSlot/rosterSize*rosterSize;}
    bool recruitingAllowed() const {return rosterFrom!=Page::deploy;}
    bool eligibleOfficer(int slot) const {
        if (!campaign.unlocked(slot)) return false;
        if (rosterFrom==Page::dispatch) return campaign.available(slot);
        if (rosterFrom==Page::deploy) return campaign.available(slot,departure.to);
        return !campaign.inField(slot);
    }
    int plannedLeader() const {
        if (battle.defending) return defenseLeader;
        return arrivingColumn>=0?campaign.columns[arrivingColumn].leader:-1;
    }

    void drawCityDesk(pxa::game::Frame& f) {
        const auto& c=campaign.cities[selected];
        const int mayor=campaign.governor(selected);
        fillBackground(f,art::cityCourtyard,.88f);
        header(f,"城池内政",cityName(selected));
        skinFrame(f,{10,43,172,84});
        if (mayor>=0) portrait(f,campaign.leader(mayor).portrait,{19,50,34,34});
        text(f,mayor>=0?campaign.leader(mayor).name:"暂无太守",mayor>=0?61:20,52,15,gold);
        char stats[60];
        std::snprintf(stats,sizeof stats,"守军 %d",c.troops);
        text(f,stats,mayor>=0?61:20,72,13,ivory);
        std::snprintf(stats,sizeof stats,"民心%d  城防%d",c.order,c.fort);
        text(f,stats,20,89,12,ivory);
        std::snprintf(stats,sizeof stats,"田产%d  市集%d",c.fields,c.market);
        text(f,stats,20,106,12,ivory);

        constexpr const char* choices[]={"征募","屯田","筑城","兴市","安民","巡访"};
        constexpr const char* price[]={"80金 50粮","95金","105金","125金","25金 65粮","35金"};
        constexpr UiIcon icons[]={recruitIcon,farmIcon,wallsIcon,marketIcon,pacifyIcon,surveyIcon};
        for (int i=0;i<6;++i) {
            const Rect tileBox{10+122*(i%3),132+47*(i/3),118,43};
            const bool enabled=i==5?campaign.canSurvey(selected):campaign.canWork(selected,Work(i));
            const bool focused=registerControl(tileBox,i==5?Command::survey:Command::work,i,enabled);
            plaque(f,tileBox,enabled,focused);
            contain(f,art::commandIcons[icons[i]],{tileBox.x+7,tileBox.y+7,29,29},true);
            text(f,choices[i],tileBox.cx()+16,tileBox.y+5,15,enabled?ivory:dim,1);
            text(f,price[i],tileBox.cx()+16,tileBox.y+23,12,enabled?gold:dim,1);
        }
        if (surveyCity==selected) {
            switch (discovery.kind) {
                case moneyFound:std::snprintf(stats,sizeof stats,"巡访得金 %d",discovery.value);break;
                case grainFound:std::snprintf(stats,sizeof stats,"巡访得粮 %d",discovery.value);break;
                case gearFound:std::snprintf(stats,sizeof stats,"找到 %s",gearNames[discovery.value]);break;
                case loreFound:std::snprintf(stats,sizeof stats,"将领获历练 %d",discovery.value);break;
            }
            plaque(f,{103,231,120,29});
            text(f,stats,163,239,12,mint,1);
        }
        button(f,{10,235,91,24},"返回",Command::back);
        button(f,{228,233,146,27},"太守任免",Command::roster,0,true,true);
    }

    void drawDispatch(pxa::game::Frame& f) {
        fillBackground(f,art::warHall);
        const char* goal=cityName(departure.to);
        char stats[70];
        std::snprintf(stats,sizeof stats,"%s → %s",cityName(departure.from),goal);
        header(f,"军队开拔",stats);
        skinFrame(f,{10,43,364,60},panel);
        if (dispatchLead>=0) portrait(f,campaign.leader(dispatchLead).portrait,{18,50,46,46});
        text(f,dispatchLead>=0?campaign.leader(dispatchLead).name:"无闲将",76,52,16,gold);
        std::snprintf(stats,sizeof stats,"目的地守军 %d",campaign.cities[departure.to].troops);
        text(f,stats,76,78,13,dim);
        button(f,{252,60,109,29},"选择领军",Command::roster);

        for (int i=0;i<3;++i) {
            Rect r{10+122*i,112,118,52};
            bool hi=registerControl(r,Command::chooseSize,i);
            plaque(f,r,true,hi||dispatchSize==i);
            text(f,dispatchNames[i],r.cx(),r.y+5,14,ivory,1);
            number(f,campaign.dispatch(departure.from,DispatchSize(i)),r.cx(),r.y+28,16,mint,1);
        }
        const bool enemy=!campaign.own(departure.to);
        button(f,{11,172,115,28},"急行抵达",Command::approach,0,true,!stageSiege,horseIcon);
        button(f,{135,172,115,28},"扎营围城",Command::approach,1,enemy,stageSiege,wallsIcon);
        plaque(f,{10,204,364,22});
        text(f,stageSiege?"次季抵达，城防减二":"抵达后可立即布阵",18,209,12,ivory);
        if (dispatchLead>=0) {
            std::snprintf(stats,sizeof stats,"耗粮 %d",campaign.supplyCost(
                departure.from,departure.to,dispatchSize,dispatchLead,stageSiege));
            text(f,stats,365,209,12,gold,2);
        }
        button(f,{10,231,91,25},"返回",Command::back);
        button(f,{244,229,130,28},"下令开拔",Command::send,0,campaign.canSend(
            departure.from,departure.to,dispatchSize,dispatchLead,stageSiege),true);
    }

    void drawForces(pxa::game::Frame& f) {
        fillBackground(f,art::warHall);
        header(f,"军情与军列",campaign.story().name);
        const auto& objective=campaign.objective();
        skinFrame(f,{10,43,364,53});
        char buf[84];
        std::snprintf(buf,sizeof buf,"%s · %s",objective.name,campaign.objectivePaid?"已达成":"夺城悬赏");
        text(f,buf,18,49,15,gold);
        std::snprintf(buf,sizeof buf,"%s %s   %s %s",
            cityName(objective.a),campaign.own(objective.a)?"已占":"未占",
            cityName(objective.b),campaign.own(objective.b)?"已占":"未占");
        text(f,buf,18,74,12,dim);
        contain(f,art::equipment[objective.reward],{320,47,44,44},true);
        bool occupied=false;
        for (int i=0;i<columnCount;++i) if (campaign.columns[i]) {
            occupied=true;
            const auto& unit=campaign.columns[i];
            int y=103+i*39;
            plaque(f,{10,y,364,36});
            text(f,campaign.leader(unit.leader).name,18,y+9,15,mint);
            std::snprintf(buf,sizeof buf,"%s → %s   %d兵%s",cityName(unit.road.from),
                          cityName(unit.road.to),unit.troops,unit.eta?" 扎营":"");
            text(f,buf,81,y+12,12,ivory);
            button(f,{300,y+4,66,28},"撤营",Command::recall,i);
        }
        if (!occupied) {
            contain(f,art::mapProps[mapScroll],{155,110,74,57},true);
            plaque(f,{112,171,160,27});
            text(f,"当前无出征军列",192,176,16,ivory,1);
        }
        button(f,{10,234,98,25},"返回",Command::back);
    }

    void drawGear(pxa::game::Frame& f) {
        fillBackground(f,art::warHall);
        const auto& lead=campaign.leader(officerSlot);
        const auto& veteran=campaign.generals[officerSlot];
        header(f,"军械与操练",lead.name);
        for (int i=0;i<noGear;++i) {
            Rect r{9+93*i,45,87,104};
            const bool equipped=veteran.gear==i;
            const bool hi=registerControl(r,Command::pickGear,i);
            skinFrame(f,r,hi||gearCursor==i?rgb(53,49,35):panel);
            contain(f,art::equipment[i],{r.x+15,r.y+3,57,58},true);
            text(f,gearNames[i],r.cx(),r.y+64,13,ivory,1);
            char countText[32];
            std::snprintf(countText,sizeof countText,"存 %d",campaign.stock[i]);
            text(f,equipped?"已佩戴":countText,r.cx(),r.y+84,12,equipped?mint:dim,1);
        }
        plaque(f,{10,153,364,56});
        text(f,gearHints[gearCursor],192,161,15,gold,1);
        char stats[70];
        std::snprintf(stats,sizeof stats,"等级 %d  历练 %d  疲劳 %d",veteran.level(),veteran.xp,veteran.fatigue);
        text(f,stats,192,183,14,ivory,1);
        button(f,{9,229,61,27},"返回",Command::back);
        button(f,{77,229,84,27},"操练85金",Command::drill,officerSlot,
               !campaign.inField(officerSlot)&&campaign.actions&&campaign.gold>=85&&
               (veteran.level()<5||veteran.fatigue));
        button(f,{168,229,77,27},"卸下",Command::equip,noGear,
               !campaign.inField(officerSlot)&&veteran.gear!=noGear);
        button(f,{252,229,121,27},"佩戴",Command::equip,gearCursor,
               !campaign.inField(officerSlot)&&veteran.gear!=gearCursor&&campaign.stock[gearCursor],true);
    }

    void drawCaptive(pxa::game::Frame& f) {
        const auto& person=campaign.prisoner;
        const auto& actor=officers[person.side][person.officer];
        fillBackground(f,art::warHall);
        header(f,"帐下俘将",cityName(person.city));
        skinFrame(f,{14,46,356,132});
        portrait(f,actor.portrait,{25,58,106,106});
        text(f,actor.name,150,65,22,gold);
        text(f,factions[person.side].title,150,96,14,colors[person.side]);
        text(f,actor.skill,150,123,15,ivory);
        text(f,campaign.guest?"客将已有席位":"花费220金可收为客将",150,146,12,dim);
        plaque(f,{66,184,252,27});
        text(f,"放归安民，索赎得110金",192,191,13,ivory,1);
        button(f,{10,226,103,31},"放归",Command::captiveChoice,0);
        button(f,{120,226,108,31},"索赎",Command::captiveChoice,1);
        button(f,{235,226,139,31},"招降俘将",Command::captiveChoice,2,campaign.mayAdmit(),true);
    }

    void drawRoster(pxa::game::Frame& f) {
        fillBackground(f,art::warHall);
        header(f,"武将名册",factions[campaign.player].title);
        int first=rosterStart(),last=std::min(rosterCount(),first+rosterSize);
        for (int i=first;i<last;++i) {
            Rect box{9+74*(i-first),46,70,108};
            const auto& actor=campaign.leader(i);
            bool focused=registerControl(box,Command::officer,i);
            skinFrame(f,box,i==officerSlot||focused?rgb(70,50,30):ink);
            portrait(f,actor.portrait,{box.x+6,box.y+5,58,58});
            text(f,actor.name,box.cx(),box.y+65,14,campaign.unlocked(i)?ivory:dim,1);
            const auto& veteran=campaign.generals[i];
            const char* job=!campaign.unlocked(i)?"待招揽":campaign.inField(i)?"出征中":
                veteran.station>=0?"太守":i==campaign.commander?"主将":"待命";
            text(f,job,box.cx(),box.y+87,12,i==campaign.commander?mint:dim,1);
        }
        const auto& actor=campaign.leader(officerSlot);
        const auto& veteran=campaign.generals[officerSlot];
        plaque(f,{10,157,364,66});
        text(f,actor.skill,20,163,17,gold);
        text(f,actor.hint,20,185,13,ivory);
        char desc[68];
        std::snprintf(desc,sizeof desc,"武%d  智%d  等级%d",actor.valor,actor.counsel,veteran.level());
        text(f,desc,364,165,13,gold,2);
        if (!campaign.unlocked(officerSlot)) {
            std::snprintf(desc,sizeof desc,"克%d城  金%d招揽",campaign.hireRenown(officerSlot),
                          campaign.hireCost(officerSlot));
        } else if (veteran.station>=0) {
            std::snprintf(desc,sizeof desc,"%s太守",cityName(veteran.station));
        } else {
            std::snprintf(desc,sizeof desc,"疲劳%d  %s",veteran.fatigue,
                          veteran.gear==noGear?"无军械":gearNames[veteran.gear]);
        }
        text(f,desc,364,205,12,dim,2);
        button(f,{9,229,63,27},"返回",Command::back);
        button(f,{77,229,37,27},"<",Command::rosterPage,-1,first>0);
        button(f,{119,229,37,27},">",Command::rosterPage,1,last<rosterCount());
        button(f,{161,229,66,27},"军械",Command::gear,0,campaign.unlocked(officerSlot));
        if (campaign.unlocked(officerSlot)) {
            const char* label=rosterFrom==Page::cityDesk?
                (veteran.station==selected?"解除太守":"任命太守"):
                rosterFrom==Page::dispatch||rosterFrom==Page::deploy?"选为领军":"任命主将";
            button(f,{231,229,143,27},label,Command::chooseOfficer,officerSlot,eligibleOfficer(officerSlot),true);
        } else {
            button(f,{231,229,143,27},"招揽入帐",Command::hire,officerSlot,
                   recruitingAllowed()&&campaign.mayHire(officerSlot),true);
        }
    }

    void drawOrders(pxa::game::Frame& f) {
        count=0;
        skinFrame(f,{39,61,306,142});
        text(f,"全军号令",192,69,20,gold,1);
        text(f,"战术耗18令，持续两合",192,93,13,dim,1);
        for (int i=press;i<=flank;++i)
            button(f,{50+95*(i-1),115,92,30},maneuvers[i],Command::maneuver,i,
                   battle.mayManeuver(Maneuver(i)),battle.maneuver==i);
        button(f,{50,156,112,30},"阵前单挑",Command::duel,0,battle.mayDuel(),true);
        button(f,{170,156,75,30},"暂停",Command::pause);
        button(f,{254,156,79,30},"返回",Command::orders);
    }

    void drawDuel(pxa::game::Frame& f) {
        fillBackground(f,art::siege);
        battleHud(f);
        portrait(f,battle.officer().portrait,{9,8,36,36});
        if (auto* foe=battle.opponent()) portrait(f,foe->portrait,{339,8,36,36});
        text(f,battle.officer().name,51,10,14,mint);
        text(f,battle.opponent()?battle.opponent()->name:"守阵校尉",332,10,14,coral,2);
        text(f,"阵前斗将",192,16,16,gold,1);
        healthBar(f,{51,32,88,10},battle.duel.health[0],100);
        healthBar(f,{245,32,88,10},battle.duel.health[1],100,true);
        int tick=battle.duel.poseTicks;
        for (int i=0;i<2;++i) {
            int pose=0;
            if (!battle.duel.health[i]) pose=5;
            else if (tick>1) pose=battle.duel.recoil==i?4:1+battle.duel.last[i];
            int x=i==0?105:279;
            if (tick && pose==1) x+=(i==0?1:-1)*std::min(25,tick*2);
            block(f,{x-37,171,74,4},rgb(48,40,30));
            contain(f,art::duelFrames[i][pose],{x-53,70,106,106},true);
        }
        char line[72];
        if (!battle.duel.active)
            std::snprintf(line,sizeof line,"%s",battle.duel.result>0?"敌将败阵，敌方军心下降":
                          battle.duel.result<0?"我将失手，我方军心下降":"两将战平，双方军心受挫");
        else if (!battle.duel.round)
            std::snprintf(line,sizeof line,"突刺克佯攻 · 格挡克突刺");
        else
            std::snprintf(line,sizeof line,"第%d合  敌势 %s",battle.duel.round+1,duelMoves[battle.duelIntent()]);
        plaque(f,{12,188,360,31});
        text(f,line,192,196,13,gold,1);
        skinFrame(f,{4,222,376,41},rgb(39,26,19),true);
        for (int i=0;i<3;++i)
            button(f,{9+103*i,229,97,28},duelMoves[i],Command::duelMove,i,battle.duel.ready());
        button(f,{322,229,54,28},"暂停",Command::pause);
    }
