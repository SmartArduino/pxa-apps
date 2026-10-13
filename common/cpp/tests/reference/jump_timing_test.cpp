// Golden positions obtained by executing the upstream JavaScript _jump.
// Regenerate: jump-jump-3d/tools/reference/extract_upstream.cjs.
#include "jump3d_clock.hpp"
#include "jump3d_game.hpp"
#include <array>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace jump;
struct Sample {float charge,time,x,y,vy;};
constexpr Sample upstream[]{
    {0.000000000F,0.050000000F,0.000000000F,1.135000000F,13.500000000F},
    {0.000000000F,0.140000000F,0.000000000F,1.734400000F,13.500000000F},
    {0.000000000F,0.250000000F,0.000000000F,1.675000000F,13.500000000F},
    {0.000000000F,0.320000000F,0.000000000F,1.183600000F,13.500000000F},
    {0.050000000F,0.050000000F,0.017500000F,1.129950000F,13.575000000F},
    {0.050000000F,0.140000000F,0.049000000F,1.736100000F,13.575000000F},
    {0.050000000F,0.250000000F,0.087500000F,1.684950000F,13.575000000F},
    {0.050000000F,0.320000000F,0.112000000F,1.198800000F,13.575000000F},
    {0.400000000F,0.050000000F,0.140000000F,1.094600000F,14.100000000F},
    {0.400000000F,0.140000000F,0.392000000F,1.748000000F,14.100000000F},
    {0.400000000F,0.250000000F,0.700000000F,1.754600000F,14.100000000F},
    {0.400000000F,0.320000000F,0.896000000F,1.305200000F,14.100000000F},
    {0.700000000F,0.050000000F,0.245000000F,1.064300000F,14.550000000F},
    {0.700000000F,0.140000000F,0.686000000F,1.758200000F,14.550000000F},
    {0.700000000F,0.250000000F,1.225000000F,1.814300000F,14.550000000F},
    {0.700000000F,0.320000000F,1.568000000F,1.396400000F,14.550000000F},
    {1.000000000F,0.050000000F,0.350000000F,1.034000000F,15.000000000F},
    {1.000000000F,0.140000000F,0.980000000F,1.768400000F,15.000000000F},
    {1.000000000F,0.250000000F,1.750000000F,1.874000000F,15.000000000F},
    {1.000000000F,0.320000000F,2.240000000F,1.487600000F,15.000000000F},
    {2.200000000F,0.050000000F,0.750000000F,1.025000000F,16.800000000F},
    {2.200000000F,0.140000000F,2.100000000F,1.921400000F,16.800000000F},
    {2.200000000F,0.250000000F,3.750000000F,2.225000000F,16.800000000F},
    {2.200000000F,0.320000000F,4.800000000F,1.964600000F,16.800000000F},
    {3.000000000F,0.050000000F,0.750000000F,1.085000000F,18.000000000F},
    {3.000000000F,0.140000000F,2.100000000F,2.089400000F,18.000000000F},
    {3.000000000F,0.250000000F,3.750000000F,2.525000000F,18.000000000F},
    {3.000000000F,0.320000000F,4.800000000F,2.348600000F,18.000000000F},
};
int main(){
    static_assert(sizeof(j3_game_t)<=348,"Effects must fit the previous game's state budget");
    for(const auto& row:upstream){
        j3_game_t game{};j3_game_reset(&game,1);
        j3_game_press(&game);j3_game_charge(&game,row.charge);j3_game_release(&game);
        j3_game_tick(&game,row.time);
        assert(game.state==J3_STATE_FLYING);
        assert(std::abs(game.px-row.x)<.00001F);
        assert(std::abs(game.py-row.y)<.00001F);
        assert(std::abs(game.launch_vy-row.vy)<.00001F);
        if(row.time>=.32F)assert(std::abs(game.spin+6.283185307F)<.00001F);
    }
    for(auto cadence:std::array{16000u,20000u,24000u,29999u,30000u,31000u,
                             32000u,36000u,40000u,48000u,80000u,100000u}){
        j3_game_t game{};j3_game_reset(&game,1);GameClock clock;
        std::uint64_t now=1000000;assert(clock.advance(now)==0);
        j3_game_press(&game);
        // Hold duration is 700 ms regardless of display cadence.
        for(unsigned elapsed=0;elapsed<700000;){
            auto dt=std::min(cadence,700000-elapsed);elapsed+=dt;now+=dt;
            j3_game_tick(&game,clock.advance(now));
        }
        assert(std::abs(game.charge-.7F)<.00001F);
        j3_game_release(&game);
        const float planned=game.land_time;
        unsigned flight=0;
        while(game.state==J3_STATE_FLYING){flight+=cadence;now+=cadence;
            j3_game_tick(&game,clock.advance(now));}
        assert(game.state==J3_STATE_READY && game.score==2);
        assert(float(flight)*1e-6F>=planned-.00001F);
        assert(float(flight)*1e-6F<planned+float(cadence)*1e-6F+.00001F);
        assert(std::abs(game.px-game.land_x)<.00001F);
        printf("cadence_us=%u flight_wall_us=%u planned_us=%u\n",cadence,flight,unsigned(planned*1e6F));
        auto snapshot=game.time;
        clock.reset();assert(clock.advance(now+60000000)==0);
        assert(game.time==snapshot);
    }
    GameClock clock;
    assert(clock.advance(0)==0 && clock.advance(1000000)==0);
    assert(clock.advance(1000000)==0 && clock.advance(999999)==0);
    assert(std::abs(clock.advance(1500000)-.5F)<.00001F);
    std::puts("upstream motion: 28 golden positions, 12 frame cadences, monotonic flip and pause passed");
}
