    static constexpr int rosterSize=5;
    int rosterCount() const {
        int count=officerCount;
        for (int slot=officerCount;slot<commanderSlots;++slot) count+=campaign.unlocked(slot);
        return count;
    }
    int rosterSlot(int index) const {
        if (index<officerCount) return index;
        for (int slot=officerCount;slot<commanderSlots;++slot)
            if (campaign.unlocked(slot) && index--==officerCount) return slot;
        return campaign.commander;
    }
    int rosterPosition() const {
        if (officerSlot<officerCount) return officerSlot;
        int index=officerCount;
        for (int slot=officerCount;slot<officerSlot;++slot) index+=campaign.unlocked(slot);
        return index;
    }
    int rosterStart() const {return rosterPosition()/rosterSize*rosterSize;}
    bool recruitingAllowed() const {return rosterFrom!=Page::deploy && rosterFrom!=Page::search;}
    bool eligibleOfficer(int slot) const {
        if (!campaign.unlocked(slot)) return false;
        if (rosterFrom==Page::dispatch) return campaign.available(slot);
        if (rosterFrom==Page::deploy) return campaign.available(slot,departure.to);
        if (rosterFrom==Page::search) return campaign.available(slot,selected);
        return !campaign.inField(slot);
    }
    int plannedLeader() const {
        if (battle.defending) return defenseLeader;
        return arrivingColumn>=0?campaign.columns[arrivingColumn].leader:-1;
    }

    void drawSoundSettings(pxa::game::Frame& f) {
        fillBackground(f,art::warHall);
        header(f,"音频设置");
        constexpr const char* names[]={"背景音乐","游戏音效","战场语音"};
        constexpr unsigned bits[]={GameAudio::musicBit,GameAudio::effectsBit,GameAudio::voiceBit};
        for (int i=0;i<3;++i) {
            const int y=48+i*54;
            const bool enabled=audio.enabled(bits[i]);
            skinFrame(f,{10,y,364,46});
            text(f,names[i],23,y+14,16,gold);
            button(f,{258,y+8,105,30},enabled?"开启":"关闭",Command::audioToggle,
                   bits[i],audio.loaded,enabled);
        }
        if (audio.loaded && audio.options && !audio.opening && !audio.bank.session)
            text(f,"音频播放权限未授予",21,215,12,dim);
        button(f,{10,234,96,25},"返回",Command::back);
    }

    void drawSeasonCouncil(pxa::game::Frame& f) {
        constexpr const char* plans[]={"照常","抚民","整训"};
        const auto accounts=campaign.seasonAccounts(seasonPlan);
        const int next=(campaign.season()+1)%4;
        char line[80];
        std::snprintf(line,sizeof line,"下一季  %d年 %s",campaign.year()+(next==0),seasons[next]);
        fillBackground(f,art::warHall);
        header(f,"季末军议",line);
        skinFrame(f,{10,41,364,45});
        std::snprintf(line,sizeof line,"季末入账   本季粮产 %d%%",grainBySeason[campaign.season()]);
        boxText(f,line,{21,44,341,17},13,gold);
        const int goldAfter=std::min(9999,campaign.gold+accounts.gold-accounts.goldCost);
        const int grainAfter=std::min(9999,campaign.grain+accounts.grain-accounts.grainCost);
        std::snprintf(line,sizeof line,"金 %+d → %d",goldAfter-campaign.gold,goldAfter);
        boxText(f,line,{21,63,170,17},13,ivory);
        std::snprintf(line,sizeof line,"粮 %+d → %d",grainAfter-campaign.grain,grainAfter);
        boxText(f,line,{206,63,156,17},13,ivory);

        for (int i=0;i<int(std::size(plans));++i) {
            const auto plan=SeasonPlan(i);
            const bool enabled=campaign.canAdvance(plan);
            const Rect row{10,89+i*26,364,23};
            const bool focused=registerControl(row,Command::choosePlan,i,enabled);
            const auto terms=campaign.seasonAccounts(plan);
            plaque(f,row,enabled,focused||seasonPlan==plan);
            if (seasonPlan==plan) block(f,{15,row.y+5,2,13},gold);
            boxText(f,plans[i],{23,row.y+2,40,19},15,enabled?gold:dim);
            if (plan==SeasonPlan::relief) {
                std::snprintf(line,sizeof line,"%d金 · 各城民心 +%d",terms.goldCost,reliefOrder);
            } else if (plan==SeasonPlan::drills) {
                std::snprintf(line,sizeof line,"%d粮 · 留守将历练 +%d，疲劳 -%d",
                              terms.grainCost,drillExperience,drillRest);
            } else std::snprintf(line,sizeof line,"依本季产量收取钱粮");
            boxText(f,line,{72,row.y+2,290,19},12,enabled?ivory:dim);
        }

        const auto forecast=campaign.forecastAttack(seasonPlan);
        battlePanel(f,{10,169,364,59},forecast.road?vermilionPlaque:bronzePlaque);
        if (forecast.road) {
            std::snprintf(line,sizeof line,"敌袭预报   %s → %s",
                          cityName(forecast.road.from),cityName(forecast.road.to));
            boxText(f,line,{21,172,237,18},13,coral);
            std::snprintf(line,sizeof line,"敌兵约 %d",forecast.attackers);
            boxText(f,line,{21,190,105,17},12,coral);
            std::snprintf(line,sizeof line,"守军约 %d",forecast.defenders);
            boxText(f,line,{129,190,110,17},12,mint);
            button(f,{257,182,105,29},"查看前线",Command::inspectThreat,0,true,true,ordersIcon);
        } else boxText(f,"暂无敌袭预报",{21,178,340,20},13,dim);
        int sieges=0;
        for (const auto& unit:campaign.columns) if (unit && unit.eta) ++sieges;
        if (sieges) {
            std::snprintf(line,sizeof line,"下季有 %d 路围城军抵达",sieges);
            boxText(f,line,{21,208,228,17},12,gold);
        }
        button(f,{10,234,96,25},"返回",Command::back);
        button(f,{250,232,124,27},"推进一季",Command::advanceSeason,0,
               campaign.canAdvance(seasonPlan),true);
    }

    void drawRegions(pxa::game::Frame& f) {
        char line[84];
        int paid=0;
        for (int i=0;i<regionalGoalCount;++i) paid+=campaign.regionalPaid(i);
        std::snprintf(line,sizeof line,"已领 %d/%d",paid,regionalGoalCount);
        fillBackground(f,art::warHall);
        header(f,"州郡军略",line);
        for (int i=0;i<regionalGoalCount;++i) {
            const Rect row{10+(i%2)*184,42+(i/2)*29,180,26};
            const bool focused=registerControl(row,Command::chooseRegion,i);
            plaque(f,row,true,focused||i==regionalChoice);
            boxText(f,regionalGoals[i].name,{row.x+10,row.y+3,113,20},15,ivory);
            const bool done=campaign.regionalPaid(i), ready=campaign.canClaimRegional(i);
            if (done || ready) std::snprintf(line,sizeof line,"%s",done?"已领":"可领");
            else std::snprintf(line,sizeof line,"%d/3",campaign.regionalProgress(i));
            boxText(f,line,{row.x+128,row.y+3,41,20},12,ready?gold:done?mint:dim,2);
        }

        const auto& goal=regionalGoals[regionalChoice];
        skinFrame(f,{10,159,364,68});
        if (goal.minOrder)
            std::snprintf(line,sizeof line,"占领三城，民心各到 %d",goal.minOrder);
        else std::snprintf(line,sizeof line,"占领下列三城");
        text(f,line,20,164,12,gold);
        for (int i=0;i<int(std::size(goal.cities));++i) {
            const int city=goal.cities[i];
            const Rect chip{19+i*116,181,113,23};
            const bool focus=registerControl(chip,Command::locateRegionCity,city);
            plaque(f,chip,true,focus);
            boxText(f,cityName(city),{chip.x+8,chip.y+2,57,19},13,ivory);
            const bool ready=campaign.regionalCityReady(regionalChoice,city);
            if (campaign.own(city) && goal.minOrder)
                std::snprintf(line,sizeof line,"%d",campaign.cities[city].order);
            else std::snprintf(line,sizeof line,"%s",ready?"已占":"未占");
            boxText(f,line,{chip.x+67,chip.y+2,37,19},12,ready?mint:dim,2);
        }
        std::snprintf(line,sizeof line,"赏金 %d  粮 %d  %s",goal.gold,goal.grain,gearNames[goal.gear]);
        text(f,line,20,208,12,ivory);
        button(f,{10,234,96,25},"返回",Command::back);
        button(f,{250,232,124,27},campaign.regionalPaid(regionalChoice)?"已领悬赏":"领取悬赏",
               Command::claimRegion,regionalChoice,campaign.canClaimRegional(regionalChoice),true);
    }

    void drawGazetteer(pxa::game::Frame& f) {
        const auto& place=placeHistory[gazetteerCity];
        const auto& start=campaign.story();
        const auto* official=recordedOfficial(gazetteerCity,campaign.chapter);
        char location[85], period[64];
        std::snprintf(location,sizeof location,"%s · %s · %s",cityName(gazetteerCity),
                      cityDistrict(gazetteerCity,campaign.year()),siteKinds[sites[gazetteerCity].kind]);
        std::snprintf(period,sizeof period,"%d年 · %s",start.year,start.name);
        fillBackground(f,art::warHall);
        header(f,"城池志",location);
        skinFrame(f,{10,41,364,81});
        text(f,"古城地望",23,49,13,gold);
        boxText(f,place.anchor,{23,62,337,20},16);
        bookText(f,place.context,{23,83,337,35},dim);

        skinFrame(f,{10,127,364,102});
        boxText(f,"史载官员与守将",{23,132,158,20},13,gold);
        boxText(f,period,{185,132,175,20},12,dim,2);
        boxText(f,official?official->title:"开局时当地长吏未详",{23,151,337,20},16);
        bookText(f,official?official->details:"城池内政处可查看和任免本局太守。",{23,172,337,35},dim);
        if (official) boxText(f,official->source,{23,207,337,17},12,gold,2);

        button(f,{10,235,91,24},"返回",Command::back);
        button(f,{151,235,104,24},"上一城",Command::browseCity,-1);
        button(f,{270,235,104,24},"下一城",Command::browseCity,1);
    }

    void drawCityDesk(pxa::game::Frame& f) {
        const auto& c=campaign.cities[selected];
        const int mayor=campaign.governor(selected);
        fillBackground(f,art::cityCourtyard,.88f);
        header(f,"城池内政",cityName(selected));
        skinFrame(f,{10,43,172,84});
        if (mayor>=0) portrait(f,campaign.leader(mayor).portrait,{19,50,34,34});
        text(f,mayor>=0?campaign.leader(mayor).name:"本局未任太守",mayor>=0?61:20,52,15,gold);
        char stats[60];
        std::snprintf(stats,sizeof stats,"守军 %d",c.troops);
        text(f,stats,mayor>=0?61:20,72,13,ivory);
        std::snprintf(stats,sizeof stats,"民心%d  城防%d",c.order,c.fort);
        text(f,stats,20,89,12,ivory);
        std::snprintf(stats,sizeof stats,"田产%d  市集%d",c.fields,c.market);
        text(f,stats,20,106,12,ivory);

        constexpr const char* choices[]={"征募","屯田","筑城","兴市","安民","搜寻"};
        constexpr const char* price[]={"80金 50粮","95金","105金","125金","25金 65粮","人才物资"};
        constexpr UiIcon icons[]={recruitIcon,farmIcon,wallsIcon,marketIcon,pacifyIcon,surveyIcon};
        for (int i=0;i<6;++i) {
            const Rect tileBox{10+122*(i%3),132+47*(i/3),118,43};
            const bool enabled=i==5 || campaign.canWork(selected,Work(i));
            const bool focused=registerControl(tileBox,i==5?Command::openSearch:Command::work,i,enabled);
            plaque(f,tileBox,enabled,focused);
            contain(f,art::commandIcons[icons[i]],{tileBox.x+7,tileBox.y+7,29,29},true);
            boxText(f,choices[i],{tileBox.x+39,tileBox.y+3,72,19},15,enabled?ivory:dim,1);
            boxText(f,price[i],{tileBox.x+39,tileBox.y+22,72,18},12,enabled?gold:dim,1);
        }
        button(f,{10,235,91,24},"返回",Command::back);
        button(f,{228,233,146,27},"太守任免",Command::roster,0,true,true);
    }

    void drawSearch(pxa::game::Frame& f) {
        const int candidate=campaign.localTalent(selected);
        const bool discovered=campaign.knowsTalent(candidate);
        const bool hired=discovered && campaign.unlocked(talentFirst+candidate);
        char line[92];
        fillBackground(f,art::cityCourtyard,.88f);
        header(f,"城中搜寻",cityName(selected));
        skinFrame(f,{10,41,364,62});
        if (searchingOfficer>=0) {
            const auto& officer=campaign.leader(searchingOfficer);
            portrait(f,officer.portrait,{18,47,49,49});
            text(f,officer.name,77,51,16,gold);
            std::snprintf(line,sizeof line,"智%d  疲劳%d",officer.counsel,campaign.generals[searchingOfficer].fatigue);
            text(f,line,77,76,12,dim);
        } else text(f,"无人可出访",21,62,15,dim);
        button(f,{272,56,91,29},"换访将",Command::searchOfficer);

        switch (discovery.kind) {
            case talentFound:std::snprintf(line,sizeof line,"寻得在野人才 %s",talents[discovery.value].officer.name);break;
            case moneyFound:std::snprintf(line,sizeof line,"搜得 %d 金",discovery.value);break;
            case grainFound:std::snprintf(line,sizeof line,"搜得 %d 粮",discovery.value);break;
            case gearFound:std::snprintf(line,sizeof line,"搜得军械 %s",gearNames[discovery.value]);break;
            case loreFound:std::snprintf(line,sizeof line,"访得兵书，额外历练 +%d",discovery.value);break;
            case noFind:std::snprintf(line,sizeof line,"每城每季可搜寻一次，消耗1政令");break;
        }
        if (hired && (discovery.kind==talentFound || discovery.kind==noFind))
            std::snprintf(line,sizeof line,"%s已入帐，可在武将名册任用",talents[candidate].officer.name);
        plaque(f,{10,108,364,23});
        boxText(f,line,{21,109,342,21},12,discovery.kind==noFind && !hired?ivory:mint,1);

        skinFrame(f,{10,136,364,90});
        if (discovered) {
            const auto& talent=talents[candidate];
            portrait(f,talent.officer.portrait,{19,147,64,64});
            boxText(f,talent.officer.name,{95,142,94,22},16,gold);
            std::snprintf(line,sizeof line,"武%d  智%d  %s",talent.officer.valor,talent.officer.counsel,
                          troopNames[talent.officer.favored]);
            boxText(f,line,{194,142,168,22},12,ivory,2);
            std::snprintf(line,sizeof line,"%s · %s",talent.officer.skill,talent.officer.hint);
            boxText(f,line,{95,165,267,22},12,ivory);
            boxText(f,talent.service,{95,190,267,22},12,mint);
        } else {
            text(f,candidate>=0?"乡里有贤才的消息":"走访乡里，搜集补给",192,154,16,gold,1);
            text(f,"智力高的武将更容易找到军械",192,187,12,ivory,1);
        }
        button(f,{10,234,73,25},"返回",Command::back);
        button(f,{90,234,127,25},campaign.alreadySearched(selected)?"本季已搜":"搜寻35金",
            Command::survey,0,campaign.canSearch(selected,searchingOfficer),!discovered);
        if (discovered && !hired) {
            std::snprintf(line,sizeof line,"延揽%d金",talents[candidate].fee);
            button(f,{225,234,149,25},line,Command::welcomeTalent,candidate,
                   campaign.canWelcome(selected,candidate),true);
        }
    }

    void drawDispatch(pxa::game::Frame& f) {
        fillBackground(f,art::warHall);
        const char* goal=cityName(departure.to);
        char stats[70];
        std::snprintf(stats,sizeof stats,"%s → %s",cityName(departure.from),goal);
        header(f,"军队开拔",stats);
        skinFrame(f,{10,43,364,60});
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
        boxText(f,stageSiege?"次季抵达，城防减二":"抵达后可立即布阵",{18,207,232,16},12,ivory);
        if (dispatchLead>=0) {
            std::snprintf(stats,sizeof stats,"耗粮 %d",campaign.supplyCost(
                departure.from,departure.to,dispatchSize,dispatchLead,stageSiege));
            boxText(f,stats,{253,207,112,16},12,gold,2);
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
            boxText(f,campaign.leader(unit.leader).name,{18,y+4,57,28},15,mint);
            std::snprintf(buf,sizeof buf,"%s → %s   %d兵%s",cityName(unit.road.from),
                          cityName(unit.road.to),unit.troops,unit.eta?" 扎营":"");
            boxText(f,buf,{80,y+4,211,28},12,ivory);
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
            skinFrame(f,r,hi||gearCursor==i);
            contain(f,art::equipment[i],{r.x+15,r.y+3,57,58},true);
            boxText(f,gearNames[i],{r.x+7,r.y+61,r.w-14,21},13,ivory,1);
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
        for (int index=first;index<last;++index) {
            const int i=rosterSlot(index);
            Rect box{9+74*(index-first),46,70,108};
            const auto& actor=campaign.leader(i);
            bool focused=registerControl(box,Command::officer,i);
            skinFrame(f,box,i==officerSlot||focused);
            portrait(f,actor.portrait,{box.x+6,box.y+5,58,58});
            boxText(f,actor.name,{box.x+5,box.y+64,box.w-10,20},14,campaign.unlocked(i)?ivory:dim,1);
            const auto& veteran=campaign.generals[i];
            const char* job=!campaign.unlocked(i)?"待招揽":campaign.inField(i)?"出征中":
                veteran.station>=0?"太守":i==campaign.commander?"主将":"待命";
            boxText(f,job,{box.x+5,box.y+85,box.w-10,19},12,i==campaign.commander?mint:dim,1);
        }
        const auto& actor=campaign.leader(officerSlot);
        const auto& veteran=campaign.generals[officerSlot];
        plaque(f,{10,157,364,66});
        boxText(f,actor.skill,{20,160,165,22},17,gold);
        const auto* talent=talentAtSlot(officerSlot);
        boxText(f,actor.hint,{20,181,344,19},12,ivory);
        char desc[68];
        std::snprintf(desc,sizeof desc,"武%d  智%d  等级%d",actor.valor,actor.counsel,veteran.level());
        boxText(f,desc,{190,160,174,22},13,gold,2);
        if (talent) {
            std::snprintf(desc,sizeof desc,"%s",talent->service);
        } else if (!campaign.unlocked(officerSlot)) {
            std::snprintf(desc,sizeof desc,"克%d城  金%d招揽",campaign.hireRenown(officerSlot),
                          campaign.hireCost(officerSlot));
        } else if (veteran.station>=0) {
            std::snprintf(desc,sizeof desc,"%s太守",cityName(veteran.station));
        } else {
            std::snprintf(desc,sizeof desc,"疲劳%d  %s",veteran.fatigue,
                          veteran.gear==noGear?"无军械":gearNames[veteran.gear]);
        }
        boxText(f,desc,{20,200,344,19},12,dim,2);
        button(f,{9,229,63,27},"返回",Command::back);
        button(f,{77,229,37,27},"<",Command::rosterPage,-1,first>0);
        button(f,{119,229,37,27},">",Command::rosterPage,1,last<rosterCount());
        button(f,{161,229,66,27},"军械",Command::gear,0,campaign.unlocked(officerSlot));
        if (campaign.unlocked(officerSlot)) {
            const char* label=rosterFrom==Page::cityDesk?
                (veteran.station==selected?"解除太守":"任命太守"):
                rosterFrom==Page::search?"选为访将":
                rosterFrom==Page::dispatch||rosterFrom==Page::deploy?"选为领军":"任命主将";
            button(f,{231,229,143,27},label,Command::chooseOfficer,officerSlot,eligibleOfficer(officerSlot),true);
        } else {
            button(f,{231,229,143,27},"招揽入帐",Command::hire,officerSlot,
                   recruitingAllowed()&&campaign.mayHire(officerSlot),true);
        }
    }

    void drawOrders(pxa::game::Frame& f) {
        count=0;
        battlePanel(f,{39,61,306,165});
        const int lane=tacticsRank;
        const bool scoped=lane>=0;
        char label[84];
        if (scoped) {
            std::snprintf(label,sizeof label,"%s  我军 %d · 敌军 %d",rankNames[lane],
                          battle.ranks[0][lane],battle.ranks[1][lane]);
        } else std::snprintf(label,sizeof label,"全军号令");
        boxText(f,label,{51,64,282,23},16,gold,1);
        for (int i=-1;i<Battle::rankCount;++i)
            button(f,{51+72*(i+1),91,66,21},i<0?"全军":rankNames[i],
                   Command::battleRank,i,true,lane==i);
        if (scoped) {
            const auto order=battle.rankOrders[lane];
            const auto support=battle.supportSize(lane);
            const char* source=support?rankNames[battle.supportSource(lane)]:"邻路";
            if (order.rounds)
                std::snprintf(label,sizeof label,"%s%d合 · %s可拨%d",maneuvers[order.maneuver],
                              order.rounds,source,support);
            else std::snprintf(label,sizeof label,"分路耗9令 · %s可拨%d",source,support);
        } else std::snprintf(label,sizeof label,"全军耗18令，持续两合");
        boxText(f,label,{51,113,282,17},12,dim,1);
        constexpr UiIcon icons[]={wedgeIcon,squareIcon,flankIcon,withdrawIcon};
        for (int i=press;i<=fallBack;++i)
            button(f,{51+72*(i-press),133,66,26},maneuvers[i],Command::maneuver,i,
                   scoped?battle.mayDirectRank(lane,Maneuver(i)):battle.mayManeuver(Maneuver(i)),
                   (scoped?battle.stance(0,lane):battle.maneuver)==i,icons[i-press]);
        const auto forecast=battle.forecastClash();
        const auto losses=[&](int side) {
            const auto& ranks=forecast[side];
            return scoped?ranks[lane]:ranks[0]+ranks[1]+ranks[2];
        };
        const int ours=losses(0), theirs=losses(1);
        std::snprintf(label,sizeof label,"令%d · 下合预计  我损%d · 敌损%d",battle.signal,ours,theirs);
        boxText(f,label,{51,198,282,20},13,
                ours>theirs?coral:ours<theirs?mint:dim,1);
        if (!scoped) {
            button(f,{51,167,112,27},"阵前单挑",Command::duel,0,battle.mayDuel(),true,duelIcon);
            button(f,{171,167,75,27},"暂停",Command::pause,0,true,false,pauseIcon);
            button(f,{254,167,79,27},"返回",Command::orders,0,true,false,withdrawIcon);
            return;
        }
        const bool hasGeneral=battle.canMoveGeneral() && battle.ranks[0][lane]>0;
        const bool sameLane=battle.generalLane==lane;
        button(f,{51,167,53,27},"增援",Command::rankSupport,0,battle.supportSize(lane)>0,true);
        button(f,{108,167,53,27},"阵前",Command::moveGeneral,2*lane+1,
               hasGeneral&&(!sameLane||!battle.atFront));
        button(f,{165,167,53,27},"后阵",Command::moveGeneral,2*lane,
               hasGeneral&&(!sameLane||battle.atFront));
        button(f,{222,167,53,27},"随军",Command::clearRank,0,battle.rankOrders[lane].rounds>0);
        button(f,{279,167,54,27},"返回",Command::orders);
    }

    void drawDuel(pxa::game::Frame& f) {
        fillBackground(f,art::siege);
        battleHud(f,true);
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
        battlePanel(f,{8,190,368,27});
        boxText(f,line,{19,194,346,19},13,gold,1);
        constexpr UiIcon icons[]={spearIcon,squareIcon,flankIcon};
        for (int i=0;i<3;++i)
            button(f,{8+99*i,229,93,28},duelMoves[i],Command::duelMove,i,battle.duel.ready(),false,icons[i]);
        button(f,{307,229,69,28},"暂停",Command::pause,0,true,false,pauseIcon);
    }
