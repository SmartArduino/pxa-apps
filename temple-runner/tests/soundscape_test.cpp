// SPDX-License-Identifier: MIT
#include "../soundscape.hpp"
#include <cassert>
#include <cstdio>
using namespace relic;
int main(){
 Soundscape audio;State s;start(s,42);
 unsigned coins=0;
 for(uint64_t t=0;t<1000000;t+=1000){audio.request(Cue::Coin,t);if(audio.ready(0,t)){audio.accepted(0,t);coins++;}}
 assert(coins<=7&&coins>=6); // Dense magnet pickups do not become a machine gun.
 audio.clear();audio.request(Cue::Jump,1000000);audio.enable(false);
 assert(!audio.ready(1,1001000));audio.request(Cue::Reward,1002000);audio.enable(true);assert(!audio.ready(2,1003000));
 audio.request(Cue::Coin,1004000);assert(!audio.ready(0,1204001)); // Never replay a cold-load event late.
 audio.clear();audio.request(Cue::Reward,2000000);audio.request(Cue::Confirm,2000100);
 assert(audio.ready(2,2000200)->cue==Cue::Reward);
 audio.clear();s.lane=-1;SoundSnapshot before(s);action(s,Action::Left);audio.action(Action::Left,before,s,3000000);assert(!audio.ready(1,3000000));
 before=SoundSnapshot(s);action(s,Action::Jump);audio.action(Action::Jump,before,s,3000000);assert(audio.ready(1,3000000)->cue==Cue::Jump);
 audio.clear();s.jump_t=.01f;before=SoundSnapshot(s);s.jump_t=0;audio.frame(before,s,4000000);assert(audio.ready(1,4000000)->cue==Cue::Land);
 audio.clear();before=SoundSnapshot(s);s.coins++;s.quests++;audio.frame(before,s,5000000);assert(!audio.ready(0,5000000)&&audio.ready(2,5000000)->cue==Cue::Reward);
 before=SoundSnapshot(s);s.mode=Mode::Dead;audio.frame(before,s,5000100);assert(!audio.ready(0,5000100)&&!audio.ready(1,5000100)&&audio.ready(2,5000100)->cue==Cue::Hit);
 audio.clear();s.mode=Mode::Running;before=SoundSnapshot(s);action(s,Action::Pause);audio.action(Action::Pause,before,s,6000000);assert(!audio.ready(2,6000000));
 puts("Soundscape passed: pickup limiting, mute, stale-event expiry, priorities, accepted actions, landing and collision");
}
