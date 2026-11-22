#pragma once

namespace tide {

struct RegionalSurvey {
    int port;
    Text site, report;
    std::array<Text,2> routes, observations;
    Text approach, junction, sketch, full, archive_sketch, archive_full;
    std::array<LandLeg,5> legs;
    int sketch_reward, full_reward;
};

inline constexpr RegionalSurvey RegionalSurveys[]{
    {madeira,"山间水渠","灌渠图",{"走田埂","沿水渠"},{"记分水口","查上游"},
     "种甘蔗的人在等上游来水。田埂好走，沿渠更快，但石道湿滑，得备足淡水。",
     "分水口没坏。可把两边水量记下来，也可继续往上查是哪段渠漏了水。",
     "你记下了两处糖田分到的水量。该回丰沙尔交图了。",
     "落石堵住了山渠。你画下落石的位置，也记了绕开滑坡的路。",
     "抄图人收下分水记录，要交给轮值的守渠人。",
     "看到你画的绕路，守渠人备好绳和镐，明早便能上山清理。",
     {{{2,2,2},{1,2,3},{1,1,1},{2,2,3},{1,1,1}}}, 165,245},
    {cadiz,"湾畔盐田","盐田图",{"走长堤","走潮滩"},{"记盐闸","查潮沟"},
     "盐工请你画几条水道。长堤随时能走，抄近路得等潮退。",
     "盐闸的水流很慢。记下闸口就能交图，也可沿外沟看看是不是淤泥挡住了海水。",
     "你记下了两道闸口和盐池的位置。带着草图回加的斯。",
     "你发现外沟拐角淤了沙。退潮时从长堤下去就能清理，你把下堤处也画进图里。",
     "档案室收下草图。盐工可以照着图交接闸口。",
     "盐工照着你画的图去疏沟。往后装盐的船可以少等几天。",
     {{{2,3,2},{1,2,2},{1,1,1},{1,2,3},{1,1,1}}}, 160,235},
};

inline int regional_survey_at(int port) {
    for (int i=0;i<int(std::size(RegionalSurveys));++i)
        if (RegionalSurveys[i].port==port) return i;
    return -1;
}

} // namespace tide
