    static constexpr int rosterSize=5;
    int rosterCount() const {
        int count=1+officerCount;
        for (int slot=officerCount;slot<rulerSlot;++slot) count+=campaign.unlocked(slot);
        return count;
    }
    int rosterSlot(int index) const {
        if (!index) return rulerSlot;
        if (index<=officerCount) return index-1;
        for (int slot=officerCount;slot<rulerSlot;++slot)
            if (campaign.unlocked(slot) && index--==officerCount+1) return slot;
        return campaign.commander;
    }
    int rosterPosition() const {
        if (officerSlot==rulerSlot) return 0;
        if (officerSlot<officerCount) return officerSlot+1;
        int index=officerCount+1;
        for (int slot=officerCount;slot<officerSlot;++slot) index+=campaign.unlocked(slot);
        return index;
    }
    int rosterStart() const {return rosterPosition()/rosterSize*rosterSize;}
    bool recruitingAllowed() const {return rosterFrom!=Page::deploy && rosterFrom!=Page::search;}
    bool eligibleOfficer(int slot) const {
        if (!campaign.unlocked(slot) || campaign.held(slot)) return false;
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
        constexpr UiIcon icons[]={musicIcon,drumIcon,voiceIcon};
        for (int i=0;i<3;++i) {
            const int y=48+i*54;
            const bool enabled=audio.enabled(bits[i]);
            skinFrame(f,{10,y,364,46});
            contain(f,iconTile(icons[i]),{22,y+10,26,26},true);
            boxText(f,names[i],{57,y+6,188,34},16,gold);
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
            boxText(f,line,{campaign.alliedRequest?218:21,211,campaign.alliedRequest?143:228,15},12,gold,
                    campaign.alliedRequest?2:0);
        }
        if (const auto request=campaign.alliedRequest;request) {
            std::snprintf(line,sizeof line,"盟军%s%s",campaign.alliedOrder()?"将攻":"暂缓出击",cityName(request.target));
            boxText(f,line,{21,211,194,15},12,mint);
            button(f,{114,234,120,25},"查看盟军",Command::alliedOffensive,request.target);
        }
        button(f,{10,234,96,25},"返回",Command::back);
        button(f,{250,232,124,27},"推进一季",Command::advanceSeason,0,
               campaign.canAdvance(seasonPlan),true);
    }

    void drawAlliedOffensive(pxa::game::Frame& f) {
        fillBackground(f,art::warHall);
        header(f,"请盟军出兵",cityName(alliedTarget));
        const auto request=campaign.alliedRequest;
        const bool thisTarget=request && request.target==alliedTarget;
        const bool canPay=campaign.actions && campaign.grain>=alliedAttackFood;
        const auto pressure=campaign.borderPressure();
        const auto& enemy=campaign.cities[alliedTarget];
        char line[92];
        skinFrame(f,{10,43,364,44});
        std::snprintf(line,sizeof line,"%s · 守军%d · 城防%d",ownerName(enemy.owner),enemy.troops,enemy.fort);
        boxText(f,line,{19,47,346,17},13,gold);
        std::snprintf(line,sizeof line,"耗%d粮与1政令 · 夺城归出兵盟军",alliedAttackFood);
        boxText(f,line,{19,65,346,17},12,ivory);
        int y=94;
        for (int side=0;side<3;++side) {
            if (!campaign.allied(side)) continue;
            const auto forecast=campaign.alliedForecast(side,alliedTarget,pressure);
            const bool ordered=thisTarget && request.side==side;
            battlePanel(f,{10,y,364,53},ordered?jadePlaque:bronzePlaque);
            boxText(f,factions[side].title,{20,y+5,105,18},15,ordered?mint:gold);
            if (ordered) {
                if (forecast.road)
                    std::snprintf(line,sizeof line,"%s集结 · 季末出击",cityName(forecast.road.from));
                else std::snprintf(line,sizeof line,"出兵受阻 · 季末退粮");
                button(f,{265,y+11,99,29},"撤销请援",Command::cancelAllied);
            } else if (forecast.road) {
                const bool likely=forecast.attackers>forecast.defenders+70*enemy.fort;
                std::snprintf(line,sizeof line,"从%s出发 · %s",cityName(forecast.road.from),
                              likely?"兵力占优":"胜算偏低");
                button(f,{265,y+11,99,29},"请其出兵",Command::requestAllied,side,!request&&canPay,true);
            } else {
                std::snprintf(line,sizeof line,"邻城无可用军队");
                button(f,{265,y+11,99,29},"暂缓出兵",Command::requestAllied,side,false);
            }
            boxText(f,line,{20,y+27,232,18},13,forecast.road?ivory:dim);
            if (forecast.road) {
                std::snprintf(line,sizeof line,"兵约%d · 敌%d",forecast.attackers,forecast.defenders);
                boxText(f,line,{125,y+5,132,18},12,mint,2);
            }
            y+=59;
        }
        if (!thisTarget && request) {
            std::snprintf(line,sizeof line,"已请%s出兵%s",factions[request.side].title,cityName(request.target));
            boxText(f,line,{19,213,346,16},12,dim);
        } else if (!request && !canPay) {
            boxText(f,"粮草或政令不足",{19,213,346,16},12,coral);
        }
        button(f,{10,234,96,25},"返回",Command::back);
    }

    void drawRegions(pxa::game::Frame& f) {
        char line[84];
        int paid=0;
        for (int i=0;i<regionalGoalCount;++i) paid+=campaign.regionalPaid(i);
        std::snprintf(line,sizeof line,"已领 %d/%d",paid,regionalGoalCount);
        fillBackground(f,art::warHall);
        header(f,"州郡军略",line);
        for (int i=0;i<regionalGoalCount;++i) {
            const Rect row{10+(i%3)*122,42+(i/3)*29,118,26};
            const bool focused=registerControl(row,Command::chooseRegion,i);
            plaque(f,row,true,focused||i==regionalChoice);
            boxText(f,regionalGoals[i].name,{row.x+7,row.y+3,73,20},13,ivory);
            const bool done=campaign.regionalPaid(i), ready=campaign.canClaimRegional(i);
            if (done || ready) std::snprintf(line,sizeof line,"%s",done?"已领":"可领");
            else std::snprintf(line,sizeof line,"%d/3",campaign.regionalProgress(i));
            boxText(f,line,{row.x+81,row.y+3,31,20},12,ready?gold:done?mint:dim,2);
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
        std::snprintf(line,sizeof line,"赏金 %d  粮 %d  %s",goal.gold,goal.grain,gearSpecs[goal.gear].name);
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
                      districtName(gazetteerCity),siteKinds[sites[gazetteerCity].kind]);
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
        if (latestChronicle(campaign.year(),campaign.season(),gazetteerCity)>=0)
            button(f,{109,235,77,24},"城中纪事",Command::chronicle,1);
        button(f,{195,235,84,24},"上一城",Command::browseCity,-1);
        button(f,{290,235,84,24},"下一城",Command::browseCity,1);
    }

    int nearbyChronicle(int step) const {
        const bool byCity=chronicleFrom==Page::gazetteer;
        const int latest=latestChronicle(campaign.year(),campaign.season());
        for (int i=chronicleCursor+step;i>=0 && i<=latest;i+=step)
            if (!byCity || chronicle[i].city==gazetteerCity) return i;
        return -1;
    }

    void drawChronicle(pxa::game::Frame& f) {
        fillBackground(f,art::warHall,.88f);
        char line[88];
        std::snprintf(line,sizeof line,"%d年 %s",campaign.year(),seasons[campaign.season()]);
        header(f,chronicleFrom==Page::gazetteer?"城中纪事":"天下纪事",line);
        skinFrame(f,{10,45,364,178});
        if (chronicleCursor>=0) {
            const auto& entry=chronicle[chronicleCursor];
            std::snprintf(line,sizeof line,"%d年 %s · %s",entry.year,seasons[entry.season],
                          cityLabel(entry.city,entry.year,entry.season));
            boxText(f,line,{25,49,334,24},13,mint);
            boxText(f,entry.title,{25,74,334,29},21,gold);
            bookText(f,entry.body,{25,107,332,53},ivory);
            boxText(f,entry.source,{25,161,333,20},12,dim);
            if (entry.deed.bit) {
                const bool done=campaign.chronicleDeeds&entry.deed.bit;
                const bool own=campaign.own(entry.city);
                button(f,{25,187,130,26},done?"差事已办":entry.deed.label,
                       Command::chronicleDeed,chronicleCursor,campaign.canDispatchDeed(chronicleCursor));
                boxText(f,done?"":own?entry.deed.terms:"领有此城可办",
                        {162,188,96,25},11,dim,1);
            }
            button(f,{263,187,95,26},"地图定位",Command::chronicleMap,entry.city);
        } else {
            boxText(f,"本季暂无纪事",{26,88,332,31},19,gold,1);
            boxText(f,"推进战役后，可在此阅读对应年代的故事。",{26,128,332,28},13,ivory,1);
        }
        button(f,{10,234,83,25},"返回",Command::back);
        const int previous=nearbyChronicle(-1),next=nearbyChronicle(1);
        button(f,{172,234,96,25},"上一篇",Command::chronicleStep,previous,previous>=0);
        button(f,{278,234,96,25},"下一篇",Command::chronicleStep,next,next>=0);
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

        skinFrame(f,{190,43,184,84});
        boxText(f,"太守自治",{199,47,166,21},15,gold);
        const auto delegated=campaign.stewardship(selected,mayor);
        if (mayor<0) std::snprintf(stats,sizeof stats,"%s",c.policy==CityPolicy::manual?"任命太守后可设置":"暂无太守 · 自治暂停");
        else if (c.policy==CityPolicy::livelihoods)
            std::snprintf(stats,sizeof stats,"民心+%d · 增收钱粮",delegated.order);
        else if (c.policy==CityPolicy::garrison)
            std::snprintf(stats,sizeof stats,"季末额外补兵%d",delegated.troops);
        else std::snprintf(stats,sizeof stats,"手动经营 · 自治免耗政令");
        boxText(f,stats,{199,71,166,18},12,dim);
        for (int i=0;i<int(std::size(cityPolicies));++i)
            button(f,{198+56*i,96,52,24},cityPolicies[i],Command::cityPolicy,i,
                   mayor>=0 || i==int(CityPolicy::manual),i==int(c.policy));

        constexpr const char* choices[]={"征募","屯田","筑城","兴市","安民","寻访"};
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
        button(f,{228,233,146,27},"太守任免",Command::roster,0,true,true,sealIcon);
    }

    void drawSearch(pxa::game::Frame& f) {
        const int candidate=campaign.localTalent(selected);
        const bool discovered=campaign.knowsTalent(candidate);
        const bool hired=discovered && campaign.unlocked(talentSlot(candidate));
        const bool done=campaign.alreadySearched(selected);
        const auto local=localFinds(selected);
        const auto odds=campaign.searchOdds(selected,searchingOfficer);
        char line[96];
        fillBackground(f,art::cityCourtyard,.88f);
        header(f,"城中寻访",cityName(selected));

        skinFrame(f,{10,41,364,57});
        if (searchingOfficer>=0) {
            const auto& officer=campaign.leader(searchingOfficer);
            portrait(f,officer.portrait,{18,47,43,43});
            boxText(f,officer.name,{70,47,185,22},16,gold);
            std::snprintf(line,sizeof line,"智%d  疲劳%d",officer.counsel,campaign.generals[searchingOfficer].fatigue);
            boxText(f,line,{70,72,185,17},12,dim);
        } else boxText(f,"无人可出访",{20,56,235,26},15,dim);
        button(f,{271,54,92,28},"换访将",Command::searchOfficer);

        skinFrame(f,{10,104,364,60});
        const bool talentLead=candidate>=0 && !discovered;
        if (talentLead && !done && searchingOfficer>=0)
            std::snprintf(line,sizeof line,"人才线索  %s  %d%%",talents[candidate].officer.name,odds.talent);
        else if (candidate>=0)
            std::snprintf(line,sizeof line,"%s  %s",talents[candidate].officer.name,
                          hired?"已入帐":discovered?"可延揽":"有乡里传闻");
        else std::snprintf(line,sizeof line,"本地寻访可得钱粮、军械或历练");
        boxText(f,line,{21,109,342,21},14,talentLead?gold:ivory);
        if (local.item!=noGear && !done && searchingOfficer>=0 && odds.localItem)
            std::snprintf(line,sizeof line,"地方特产  %s  %d%%",gearSpecs[local.item].name,odds.localItem);
        else if (local.item!=noGear) {
            const char* note=done || searchingOfficer<0?"":odds.talent==100?" · 本次先访人才":" · 库藏已满";
            std::snprintf(line,sizeof line,"地方特产  %s%s",gearSpecs[local.item].name,note);
        }
        else std::snprintf(line,sizeof line,"智力较高的访将更易找到军械");
        boxText(f,line,{21,136,342,20},13,mint);

        plaque(f,{10,171,364,53});
        switch (discovery.kind) {
            case talentFound:std::snprintf(line,sizeof line,"寻得人才  %s",talents[discovery.value].officer.name);break;
            case moneyFound:std::snprintf(line,sizeof line,"寻得 %d 金",discovery.value);break;
            case grainFound:std::snprintf(line,sizeof line,"寻得 %d 粮",discovery.value);break;
            case gearFound:std::snprintf(line,sizeof line,"寻得军械  %s",gearSpecs[discovery.value].name);break;
            case loreFound:std::snprintf(line,sizeof line,"访得兵书，历练 +%d",discovery.value);break;
            case noFind: {
                const char* status=done?"本季已寻访":!campaign.actions?"政令不足":
                    campaign.gold<Campaign::searchCost?"金不足":"每城每季一次，消耗1政令";
                std::snprintf(line,sizeof line,"%s",status);
                break;
            }
        }
        if (hired && (discovery.kind==talentFound || discovery.kind==noFind))
            std::snprintf(line,sizeof line,"%s已入帐，可在名册任用",talents[candidate].officer.name);
        else if (discovered && (campaign.talentOffered&(1u<<candidate)))
            std::snprintf(line,sizeof line,"婉拒入帐，已花%d金，下季可再访",talents[candidate].fee/4);
        boxText(f,line,{21,177,342,20},13,discovery.kind==noFind?ivory:mint,1);
        if (discovered) boxText(f,talents[candidate].service,{21,199,342,18},12,dim,1);
        else if (talentLead) {
            const bool fictional=candidate<legacyTalentCount || candidate==legacyTalentCount+2;
            boxText(f,fictional?"此人为游戏人物":"据籍贯或行迹安排的寻访线索",
                    {21,199,342,18},12,dim,1);
        } else if (local.item>=twinJian)
            boxText(f,"特产取材于地方故事",{21,199,342,18},12,dim,1);

        button(f,{10,234,73,25},"返回",Command::back);
        button(f,{90,234,127,25},done?"本季已访":"寻访35金",
            Command::survey,0,campaign.canSearch(selected,searchingOfficer),!discovered);
        if (discovered && !hired) {
            std::snprintf(line,sizeof line,"延揽%d金 %d%%",talents[candidate].fee,
                          campaign.welcomeChance(selected,candidate));
            button(f,{225,234,149,25},line,Command::welcomeTalent,candidate,
                   campaign.canWelcome(selected,candidate),true);
        }
    }

    void drawDispatch(pxa::game::Frame& f) {
        fillBackground(f,art::warHall);
        const bool enemy=!campaign.own(departure.to);
        const char* goal=cityName(departure.to);
        const auto route=campaign.marchRoute(departure.from,departure.to);
        const int ally=campaign.alliedWaypoint(route);
        char stats[70];
        std::snprintf(stats,sizeof stats,"%s → %s",cityName(departure.from),goal);
        header(f,enemy?"军队开拔":"守军调拨",stats);
        skinFrame(f,{10,43,364,60});
        if (dispatchLead>=0) portrait(f,campaign.leader(dispatchLead).portrait,{18,50,46,46});
        text(f,dispatchLead>=0?campaign.leader(dispatchLead).name:"无闲将",76,52,16,gold);
        if (route.legs()>1)
            std::snprintf(stats,sizeof stats,"%s%s · %d段",ally>=0?"借道":"经",
                          cityName(ally>=0?ally:route.cities[1]),route.legs());
        else std::snprintf(stats,sizeof stats,"目的地守军 %d",campaign.cities[departure.to].troops);
        boxText(f,stats,{76,77,168,17},13,ally>=0?mint:dim);
        button(f,{252,60,109,29},"选择领军",Command::roster);

        for (int i=0;i<3;++i) {
            Rect r{10+122*i,112,118,52};
            bool hi=registerControl(r,Command::chooseSize,i);
            plaque(f,r,true,hi||dispatchSize==i);
            text(f,dispatchNames[i],r.cx(),r.y+5,14,ivory,1);
            number(f,campaign.dispatchTo(departure.from,departure.to,DispatchSize(i)),r.cx(),r.y+28,16,mint,1);
        }
        button(f,{11,172,115,28},"急行抵达",Command::approach,0,true,!stageSiege,horseIcon);
        if (enemy) button(f,{135,172,115,28},"扎营围城",Command::approach,1,true,stageSiege,wallsIcon);
        else {
            const int moving=campaign.dispatchTo(departure.from,departure.to,dispatchSize);
            std::snprintf(stats,sizeof stats,"留守%d · 目标余位%d",
                          campaign.cities[departure.from].troops-moving,armyLimit-campaign.cities[departure.to].troops);
            boxText(f,stats,{135,175,229,22},13,mint,1);
        }
        plaque(f,{10,204,364,22});
        boxText(f,stageSiege?"次季抵达，城防减二":enemy?"抵达后可立即布阵":"抵达后补入守军",
                {18,207,232,16},12,ivory);
        if (dispatchLead>=0) {
            std::snprintf(stats,sizeof stats,"耗粮 %d",campaign.supplyCost(route,dispatchSize,dispatchLead,stageSiege));
            boxText(f,stats,{253,207,112,16},12,gold,2);
        }
        button(f,{10,231,91,25},"返回",Command::back);
        button(f,{244,229,130,28},enemy?"下令开拔":"下令调兵",Command::send,0,
               campaign.canSend(route,dispatchSize,dispatchLead,stageSiege),true);
    }

    void drawFiefs(pxa::game::Frame& f) {
        fillBackground(f,art::warHall);
        std::array<int,cityCount> fiefs{};
        const int threatened=campaign.nextAttack().to;
        int count=0;
        if (campaign.own(threatened)) fiefs[count++]=threatened;
        for (int city=0;city<cityCount;++city)
            if (city!=threatened && campaign.own(city)) fiefs[count++]=city;
        char line[72];
        std::snprintf(line,sizeof line,"%d处城关",count);
        header(f,"领地总览",line);
        const auto pressure=campaign.borderPressure();
        plaque(f,{10,42,364,31});
        std::snprintf(line,sizeof line,"起点 %s     政令 %d/%d",
                      cityName(selected),campaign.actions,campaign.actionCapacity());
        boxText(f,line,{20,43,344,15},12,gold);
        boxText(f,"每季政令   1至4城 2 · 5至12城 3 · 13城起 4",{20,57,344,15},12,dim);
        const int first=fiefsPage*fiefRows;
        for (int i=first;i<std::min(first+fiefRows,count);++i) {
            const int city=fiefs[i];
            const auto& c=campaign.cities[city];
            const int mayor=campaign.governor(city), y=77+(i-first)*36;
            battlePanel(f,{10,y,364,34},city==threatened?vermilionPlaque:bronzePlaque);
            boxText(f,cityName(city),{18,y+2,69,17},15,gold);
            boxText(f,city==threatened?"告急":pressure[city]?"前线":"后方",
                    {18,y+19,65,12},11,city==threatened?coral:dim);
            if (mayor>=0) std::snprintf(line,sizeof line,"太守 %s",campaign.leader(mayor).name);
            else std::snprintf(line,sizeof line,"未任太守");
            boxText(f,line,{91,y+2,117,17},12,ivory);
            std::snprintf(line,sizeof line,"兵%d · %s",c.troops,
                          mayor<0 && c.policy!=CityPolicy::manual?"自治暂停":cityPolicies[int(c.policy)]);
            boxText(f,line,{91,y+18,127,15},12,mint);
            button(f,{219,y+4,68,26},"内政",Command::fiefDesk,city);
            button(f,{293,y+4,73,26},city==selected?"当前起点":"设为起点",Command::origin,city,true,city==selected);
        }
        const int lastPage=std::max(0,(count-1)/fiefRows);
        button(f,{10,232,79,26},"返回",Command::back);
        button(f,{106,232,72,26},"上一页",Command::fiefPage,-1,fiefsPage>0);
        std::snprintf(line,sizeof line,"%d/%d",fiefsPage+1,lastPage+1);
        boxText(f,line,{183,232,79,26},14,dim,1);
        button(f,{267,232,107,26},"下一页",Command::fiefPage,1,fiefsPage<lastPage);
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

    void chooseGearCategory(GearSlot category) {
        gearCategory=category;
        const auto item=campaign.generals[officerSlot].kit[category];
        if (item!=noGear) {
            gearCursor=item;
            return;
        }
        for (int i=0;i<noGear;++i) if (gearSpecs[i].slot==category) {
            gearCursor=i;
            return;
        }
    }
    void drawGear(pxa::game::Frame& f) {
        static_assert(std::size(art::equipment)==noGear);
        fillBackground(f,armoryBackdrop());
        const auto& lead=campaign.leader(officerSlot);
        const auto& veteran=campaign.generals[officerSlot];
        const auto equipped=veteran.kit[gearCategory];
        header(f,"军械与操练",lead.name);
        constexpr UiIcon categories[]={maceIcon,armorIcon,saddleIcon,fanIcon};
        static_assert(std::size(categories)==gearSlotCount);
        for (int i=0;i<gearSlotCount;++i)
            button(f,{10+92*i,40,88,24},gearSlotNames[i],Command::gearCategory,i,true,i==gearCategory,categories[i]);
        int count=0;
        for (const auto& spec:gearSpecs) count+=spec.slot==gearCategory;
        const int step=364/count;
        char line[96];
        int cell=0;
        for (int i=0;i<noGear;++i) {
            const auto& spec=gearSpecs[i];
            if (spec.slot!=gearCategory) continue;
            const Rect r{10+step*cell++,69,step-4,91};
            const bool hi=registerControl(r,Command::pickGear,i);
            skinFrame(f,r,hi||gearCursor==i);
            contain(f,art::equipment[i],{r.cx()-21,r.y+6,42,43},true);
            boxText(f,spec.name,{r.x+4,r.y+51,r.w-8,18},13,ivory,1);
            std::snprintf(line,sizeof line,"库存%d",campaign.stock[i]);
            boxText(f,equipped==i?"已装备":line,{r.x+4,r.y+70,r.w-8,16},12,equipped==i?mint:dim,1);
        }
        plaque(f,{10,165,364,55});
        boxText(f,gearSpecs[gearCursor].name,{19,166,346,18},15,gold,1);
        boxText(f,gearSpecs[gearCursor].hint,{19,184,346,17},12,ivory,1);
        std::snprintf(line,sizeof line,"等级%d · 历练%d · 疲劳%d",veteran.level(),veteran.xp,veteran.fatigue);
        boxText(f,line,{20,201,184,17},12,dim);
        std::snprintf(line,sizeof line,"%d",campaign.gold);
        iconText(f,coinsIcon,line,{212,201,71,17},12,gold);
        iconText(f,orderIcon,"购置 1",{290,201,74,17},12,dim);
        button(f,{10,229,50,27},"返回",Command::back);
        button(f,{64,229,75,27},"操练85金",Command::drill,officerSlot,
               campaign.canChangeGear(officerSlot)&&campaign.actions&&campaign.gold>=85&&
               (veteran.level()<5||veteran.fatigue));
        std::snprintf(line,sizeof line,"购置%d金",gearSpecs[gearCursor].price);
        button(f,{143,229,89,27},line,Command::buyGear,gearCursor,campaign.canBuyGear(Gear(gearCursor)));
        button(f,{236,229,58,27},"卸下",Command::unequip,gearCategory,
               campaign.canChangeGear(officerSlot)&&equipped!=noGear);
        button(f,{298,229,76,27},"装备",Command::equip,gearCursor,campaign.mayEquip(officerSlot,Gear(gearCursor)),true);
    }

    int prisonEntries() const {
        return prisonTab==0?int(campaign.prisoners.size()):prisonTab==1?commanderSlots:cityCount;
    }
    bool hasPrisonEntry(int entry) const {
        if (entry<0) return false;
        if (prisonTab==0) return bool(campaign.prisoners[entry]);
        if (prisonTab==1) return campaign.held(entry);
        return campaign.cities[entry].prisoners>0;
    }
    int nextPrisoner(int entry,int step) const {
        const int size=prisonEntries();
        for (int n=0;n<size;++n) {
            entry=(entry+step+size)%size;
            if (hasPrisonEntry(entry)) return entry;
        }
        return -1;
    }
    const Officer* prisonOfficer() const {
        if (!hasPrisonEntry(prisonIndex) || prisonTab==2) return nullptr;
        if (prisonTab==1) return &campaign.leader(prisonIndex);
        const auto& person=campaign.prisoners[prisonIndex];
        return &officers[person.side][person.officer];
    }
    void openPrison(int tab) {
        prisonFrom=page==Page::council?Page::council:Page::map;
        prisonMessage=nullptr;
        prisonTab=tab;
        prisonIndex=nextPrisoner(-1,1);
        if (page==Page::report) {
            if (tab==0 && aftermath.captive>=0) prisonIndex=aftermath.captive;
            if (tab==1 && aftermath.ourCaptive>=0) prisonIndex=aftermath.ourCaptive;
            if (tab==2 && campaign.cities[reportCity].prisoners) prisonIndex=reportCity;
        }
        show(Page::captive);
    }
    void actOnPrisoner(int action) {
        if (!hasPrisonEntry(prisonIndex)) return;
        if (prisonTab==0) {
            if (action==2) {
                const auto result=campaign.admit(prisonIndex);
                if (result==Offer::unavailable) return;
                prisonMessage=result==Offer::joined?"俘将已归顺":"拒绝归顺，已花55金";
                logState(result==Offer::joined?"guest":"captive-refused");
            } else {
                if (!campaign.disposeCaptive(prisonIndex,action==1)) return;
                prisonMessage=action==1?"收取110金，俘将已放回":"俘将已放归，城中民心回升";
                logState("captive-released");
            }
        } else if (prisonTab==1) {
            if (!campaign.ransomGeneral(prisonIndex)) return;
            prisonMessage="我将已归队";
            logState("our-ransomed");
        } else {
            if (!(action==0?campaign.releasePrisoners(prisonIndex):campaign.enlistPrisoners(prisonIndex))) return;
            prisonMessage=action==0?"俘兵已遣返，城中民心回升":"俘兵已编入守军";
            logState(action==0?"prisoners-released":"prisoners-enlisted");
        }
        if (!hasPrisonEntry(prisonIndex)) prisonIndex=nextPrisoner(prisonIndex,1);
        audio.cue(confirmSound);
        checkpoint();
    }
    void drawCaptive(pxa::game::Frame& f) {
        fillBackground(f,art::warHall);
        header(f,"俘虏营");
        constexpr const char* tabs[]={"敌方俘将","我方被俘","双方俘兵"};
        for (int i=0;i<3;++i)
            button(f,{10+123*i,39,118,24},tabs[i],Command::prisonTab,i,true,i==prisonTab);
        skinFrame(f,{10,68,364,122});
        char line[72];
        const int entry=prisonIndex;
        if (const auto* actor=prisonOfficer()) {
            const int city=prisonTab==0?campaign.prisoners[entry].city:campaign.detained[entry].city;
            portrait(f,actor->portrait,{23,81,89,89});
            boxText(f,actor->name,{124,79,215,28},21,gold);
            std::snprintf(line,sizeof line,"关押于 %s",cityName(city));
            boxText(f,line,{124,107,230,20},14,ivory);
            if (prisonTab==0) {
                const auto& person=campaign.prisoners[entry];
                std::snprintf(line,sizeof line,"%s  ·  招降率 %d%%",factions[person.side].title,
                              campaign.admitChance(entry));
                boxText(f,line,{124,131,229,19},13,mint);
                std::snprintf(line,sizeof line,"下季逃脱率 %d%%",campaign.escapeChance(entry,1));
                boxText(f,line,{124,153,229,20},13,dim);
            } else {
                boxText(f,"被俘期间无法领军或理政",{124,132,232,21},13,coral);
                boxText(f,"可赎回、夺城救出，亦可能自行脱身",{124,154,232,21},12,dim);
            }
        } else if (prisonTab==2 && entry>=0) {
            const bool ours=campaign.own(entry);
            boxText(f,cityName(entry),{23,77,334,29},20,gold,1);
            std::snprintf(line,sizeof line,ours?"收押敌兵 %d 人":"我方被俘 %d 人",campaign.cities[entry].prisoners);
            boxText(f,line,{23,112,334,25},17,ours?mint:coral,1);
            boxText(f,ours?"可整编或遣返，拖延可能有俘兵逃散":"夺城可救回，部分俘兵也会就近脱身",
                    {23,145,334,22},13,dim,1);
        } else {
            constexpr const char* empty[]={"当前没有敌方俘将","我方武将均未被俘","当前没有收押的士兵"};
            boxText(f,empty[prisonTab],{23,113,338,27},16,dim,1);
        }
        const char* detail=prisonMessage;
        if (!detail && entry>=0 && prisonTab==0) {
            const auto& p=campaign.prisoners[entry];
            detail=p.offered==campaign.turn?"本季已劝降，下季可再试":
                campaign.freeGuest()<0?"客将席位已满":"招降成功220金，遭拒55金";
        }
        if (!detail && entry>=0 && prisonTab==2 && campaign.own(entry)) detail="每次最多整编180人，消耗1政令与粮食";
        if (detail) boxText(f,detail,{55,193,274,26},12,gold,1);
        const bool canPage=entry>=0 && nextPrisoner(entry,1)!=entry;
        button(f,{10,193,40,25},"<",Command::prisonPage,-1,canPage);
        button(f,{334,193,40,25},">",Command::prisonPage,1,canPage);
        button(f,{10,228,62,28},"返回",Command::back);
        if (entry<0) return;
        if (prisonTab==0) {
            button(f,{78,228,76,28},"放归",Command::captiveChoice,0);
            button(f,{160,228,101,28},"索赎110金",Command::captiveChoice,1,campaign.mayRansom(entry));
            button(f,{267,228,107,28},"招降",Command::captiveChoice,2,campaign.mayAdmit(entry),true);
        } else if (prisonTab==1) {
            button(f,{243,228,131,28},"赎回160金",Command::captiveChoice,0,
                   campaign.gold>=Campaign::friendlyRansom,true);
        } else if (campaign.own(entry)) {
            const int count=campaign.enlistCapacity(entry);
            button(f,{179,228,76,28},"遣返",Command::captiveChoice,0);
            std::snprintf(line,sizeof line,"整编%d人",count);
            button(f,{261,228,113,28},count?line:"整编",Command::captiveChoice,1,count>0,true);
        }
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
            const char* job=!campaign.unlocked(i)?"待招揽":campaign.held(i)?"被俘":campaign.inField(i)?"出征中":
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
        std::snprintf(desc,sizeof desc,"%s · 武%d 智%d · %d级",roleNames[int(battleRole(actor))],
                      actor.valor,actor.counsel,veteran.level());
        boxText(f,desc,{190,160,174,22},13,gold,2);
        if (campaign.held(officerSlot)) {
            std::snprintf(desc,sizeof desc,"被关押于%s，可前往俘虏营赎回",
                          cityName(campaign.detained[officerSlot].city));
        } else if (talent) {
            std::snprintf(desc,sizeof desc,"%s",talent->service);
        } else if (!campaign.unlocked(officerSlot)) {
            if (campaign.hireOffered&(1u<<officerSlot))
                std::snprintf(desc,sizeof desc,"婉拒入帐，已花%d金，下季可再访",campaign.hireCost(officerSlot)/4);
            else std::snprintf(desc,sizeof desc,"克%d城  %d金  %d%%",campaign.hireRenown(officerSlot),
                               campaign.hireCost(officerSlot),campaign.hireChance(officerSlot));
        } else if (veteran.station>=0) {
            std::snprintf(desc,sizeof desc,"%s太守",cityName(veteran.station));
        } else {
            const int pieces=gearSlotCount-std::count(veteran.kit.begin(),veteran.kit.end(),noGear);
            std::snprintf(desc,sizeof desc,"疲劳%d  已配军械%d件",veteran.fatigue,pieces);
        }
        boxText(f,desc,{20,200,344,19},12,dim,2);
        button(f,{9,229,63,27},"返回",Command::back);
        button(f,{77,229,37,27},"<",Command::rosterPage,-1,first>0);
        button(f,{119,229,37,27},">",Command::rosterPage,1,last<rosterCount());
        button(f,{161,229,66,27},"军械",Command::gear,0,campaign.unlocked(officerSlot)&&!campaign.held(officerSlot),false,maceIcon);
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
        const auto center=[&](Rect r) {return r.shiftY(tallScene/2);};
        const int lane=tacticsRank;
        const bool scoped=lane>=0;
        const auto preview=proposedOrder?battle.previewOrder(lane,*proposedOrder):std::nullopt;
        char enemyLabel[60];
        const char* intent=plannedEnemyOrder(enemyLabel);
        battlePanel(f,center({39,61,306,(preview?186:scoped?188:165)+(intent?12:0)}));
        char label[84];
        if (scoped) {
            std::snprintf(label,sizeof label,"%s  我军 %d · 敌军 %d",rankNames[lane],
                          battle.ranks[0][lane],battle.ranks[1][lane]);
        } else std::snprintf(label,sizeof label,"全军号令");
        boxText(f,label,{51,64,282,23},16,gold,1);
        for (int i=-1;i<Battle::rankCount;++i)
            button(f,center({51+72*(i+1),91,66,21}),i<0?"全军":rankNames[i],
                   Command::battleRank,i,true,lane==i);
        if (scoped) {
            const auto order=battle.rankOrders[lane];
            const auto support=battle.supportSize(lane);
            const char* source=support?rankNames[battle.supportSource(lane)]:"邻路";
            if (order.rounds)
                std::snprintf(label,sizeof label,"%s%d合 · %s可拨%d",maneuvers[order.maneuver],
                              order.rounds,source,support);
            else std::snprintf(label,sizeof label,"分路耗9令 · %s可拨%d",source,support);
        } else if (battle.maneuverRounds) {
            std::snprintf(label,sizeof label,"全军%s · 余%d合",maneuvers[battle.maneuver],battle.maneuverRounds);
        } else {
            std::snprintf(label,sizeof label,"%s",battle.anchored?"全军驻守 · 下令进军后推进":"全军战法耗18令，持续两合");
        }
        if (scoped && battle.coordinating() && battle.aid.target==lane)
            std::snprintf(label,sizeof label,"%s%s · 余%d合",rankNames[battle.aid.source],
                          battle.shieldCover()?"掩护":"协战",battle.aid.rounds);
        boxText(f,label,{51,113,282,17},12,dim,1);
        constexpr UiIcon icons[]={wedgeIcon,squareIcon,flankIcon,withdrawIcon};
        const auto activeOrder=scoped?battle.stance(0,lane):battle.maneuver;
        for (int i=press;i<=fallBack;++i)
            button(f,center({51+72*(i-press),133,66,26}),maneuvers[i],Command::previewOrder,i,
                   scoped?battle.mayDirectRank(lane,Maneuver(i)):battle.mayManeuver(Maneuver(i)),
                   (preview?*proposedOrder:activeOrder)==i,icons[i-press]);
        const auto forecast=battle.previewClash();
        const auto losses=[&](const Battle::Clash& estimate,int side) {
            const auto& ranks=estimate[side];
            return scoped?ranks[lane]:ranks[0]+ranks[1]+ranks[2];
        };
        const int ours=losses(forecast,0), theirs=losses(forecast,1);
        if (preview) {
            for (int side=0;side<2;++side) {
                const int before=side?theirs:ours;
                const int after=losses(preview->losses,side);
                const int benefit=(after-before)*(side?1:-1);
                const Rect resultBox=center({51+144*side,165,138,28});
                battlePanel(f,resultBox,benefit>0?jadePlaque:benefit<0?vermilionPlaque:bronzePlaque);
                std::snprintf(label,sizeof label,"%s%d → %d",side?"敌损 ":"我损 ",before,after);
                boxText(f,label,resultBox.inset(5,3),13,benefit>0?mint:benefit<0?coral:ivory,1);
            }
            std::snprintf(label,sizeof label,"%s预计 · 耗%d令 · 余%d令",
                          battle.round?"下合":"接敌",preview->cost,battle.signal-preview->cost);
            boxText(f,label,{51,195,282,18},12,gold,1);
            button(f,center({51,217,138,25}),"取消",Command::cancelOrder);
            button(f,center({195,217,138,25}),"下令",Command::maneuver,*proposedOrder,true,true,ordersIcon);
            if (intent) boxText(f,intent,{51,244,282,13},12,coral,1);
            return;
        }
        std::snprintf(label,sizeof label,"令%d · %s预计  我损%d · 敌损%d",
                      battle.signal,battle.round?"下合":"接敌",ours,theirs);
        boxText(f,label,{51,scoped?222:198,282,20},13,
                ours>theirs?coral:ours<theirs?mint:dim,1);
        if (intent) boxText(f,intent,{51,scoped?242:218,282,16},12,coral,1);
        if (!scoped) {
            button(f,center({51,167,84,27}),"单挑",Command::duel,0,battle.mayDuel(),true,duelIcon);
            button(f,center({141,167,60,27}),"驻守",Command::anchorArmy,0,!battle.anchored,battle.anchored);
            button(f,center({207,167,60,27}),"进军",Command::maneuver,measured,battle.mayManeuver(measured));
            button(f,center({273,167,60,27}),"返回",Command::orders);
            return;
        }
        const bool hasGeneral=battle.canMoveGeneral() && battle.ranks[0][lane]>0 && !battle.routed[0][lane];
        const bool sameLane=battle.generalLane==lane;
        button(f,center({51,167,53,27}),"增援",Command::rankSupport,0,battle.supportSize(lane)>0,true);
        button(f,center({108,167,53,27}),"阵前",Command::moveGeneral,2*lane+1,
               hasGeneral&&(!sameLane||!battle.atFront));
        button(f,center({165,167,53,27}),"后阵",Command::moveGeneral,2*lane,
               hasGeneral&&(!sameLane||battle.atFront));
        button(f,center({222,167,53,27}),"随军",Command::clearRank,0,battle.rankOrders[lane].rounds>0);
        button(f,center({279,167,54,27}),"返回",Command::orders);
        int x=51;
        for (int source=std::max(0,lane-1);source<=std::min(Battle::rankCount-1,lane+1);++source) {
            if (source==lane) continue;
            const auto troop=battle.troopFor(0,source);
            const char* order=troop==shields?"掩护":ranged(troop)?"援射":"夹击";
            std::snprintf(label,sizeof label,"%s%s · %d令",rankNames[source],order,Battle::supportCost);
            button(f,center({x,197,138,22}),label,Command::rankAssist,source,battle.mayAid(source,lane));
            x+=144;
        }
        if (lane!=1) boxText(f,"邻路协战两合",{195,198,138,19},12,dim,1);
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
            constexpr int foot=172;
            contactShadow(f,x,foot,64);
            const auto* actor=i?battle.opponent():&battle.officer();
            const int signature=signatureFor(actor);
            const int leaderPose=zhaoAnimation(signature)?(pose==1?4:pose==5?5:pose?3:0):
                                 pose==1?3:pose?2:0;
            const auto sprite=signature>=0?generalFrame(signature,i,leaderPose):
                art::duelFrames[i][pose];
            groundedTile(f,sprite,x,foot,106,106);
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
        if (battle.paused) return;
        constexpr UiIcon icons[]={spearIcon,squareIcon,flankIcon};
        for (int i=0;i<3;++i)
            button(f,{8+99*i,229,93,28},duelMoves[i],Command::duelMove,i,battle.duel.ready(),false,icons[i]);
        button(f,{307,229,69,28},"暂停",Command::pause,0,true,false,pauseIcon);
    }
