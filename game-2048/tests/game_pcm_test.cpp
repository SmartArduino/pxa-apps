// SPDX-License-Identifier: CC0-1.0
// Link each real game with the real Pocket Game runtime and SDK audio codec.
#ifndef POCKET_AUDIO_GAME
#error POCKET_AUDIO_GAME must identify 1=2048, 2=Chess or 3=Sokoban
#endif
#define main audio_backend_test_main
#include "audio_test.cpp"
#undef main

namespace {
std::vector<uint8_t> saved_game() {
    uint8_t raw[4096]; const size_t n=game::serialize(raw,sizeof(raw));
    assert(n && n<=sizeof(raw)); return {raw,raw+n};
}
void advance(unsigned count) {
    for(unsigned i=0;i<count;++i) {
        test_us+=20000; game::tick(20); service_sound();
    }
}
void zero_pcm() {
    assert(!captured.empty());
    assert(std::all_of(captured.begin(),captured.end(),[](int16_t s){return s==0;}));
}
}
int main() {
    for(unsigned legacy=0;legacy<2;++legacy) {
        ready(legacy); game::init(); const auto initial=saved_game();
        advance(500); zero_pcm(); assert(captured.size()>=160000 && saved_game()==initial);
        const size_t idle_samples=captured.size(); captured.clear();
        dispatch_key(POCKET_AUDIO_GAME==2 ? pocket::Key::A : pocket::Key::RIGHT);
        const auto interacted=saved_game(); assert(interacted!=initial);
        advance(25); check_samples();
        const auto sounding=std::count_if(captured.begin(),captured.end(),[](int16_t s){return s!=0;});
        captured.clear(); advance(500); zero_pcm(); assert(saved_game()==interacted);
        // A pause flush followed by idle resume must not introduce any melody.
        toggle_pause(); assert(paused); toggle_pause(); assert(!paused);
        captured.clear(); advance(100); zero_pcm(); assert(saved_game()==interacted);
        printf("PASS %s %s: startup/10s idle %zu zero samples, real key SFX %ld nonzero samples, 10s post-effect and pause/resume zero PCM; exact save bytes stable\n",
            game::app_id(),legacy?"legacy":"PCM",idle_samples,long(sounding));
    }
    return 0;
}
