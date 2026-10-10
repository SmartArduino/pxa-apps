#!/usr/bin/env python3
"""Instrument isolated C/C++ copies once at release, never during a hold.

No gameplay, clock, audio or draw calls are changed. The trace counts callbacks,
MOVE events and charging draw attempts so the real workloads can be compared.
"""
import argparse
import shutil
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
assert args.source.resolve() != args.output.resolve()
for name in ("jump-jump-3d", "jump-jump-3d-cpp", "common"):
    shutil.copytree(args.source / name, args.output / name,
                   ignore=shutil.ignore_patterns("docs", "tests", "__pycache__"),
                   dirs_exist_ok=True)

def replace(path, before, after):
    source = path.read_text()
    assert source.count(before) == 1, (path, before)
    path.write_text(source.replace(before, after))

fields = """
static uint64_t trace_down_us, trace_previous_us, trace_max_gap_us;
static uint32_t trace_ticks, trace_steps, trace_moves, trace_draws;
"""
reset = """trace_down_us = {pointer}.timestamp_us;
                trace_previous_us = trace_max_gap_us = 0;
                trace_ticks = trace_steps = trace_moves = trace_draws = 0;"""
ticks = """
        if(g_game.state==J3_STATE_CHARGING) {
            ++trace_ticks; trace_steps += steps;
            if(trace_previous_us && {now}>trace_previous_us &&
               {now}-trace_previous_us>trace_max_gap_us)
                trace_max_gap_us={now}-trace_previous_us;
            trace_previous_us={now};
        }
"""
release = """
                const unsigned trace_charge_us=(unsigned)(g_game.charge*1e6f);
                j3_game_release(&g_game);
                char trace[384];
                snprintf(trace,sizeof(trace),"J3RELEASE down=%llu up=%llu charge_us=%u ticks=%u steps=%u moves=%u draws=%u max_gap_us=%llu vx_q6=%d vy_q6=%d vz_q6=%d land_x_q6=%d land_z_q6=%d state=%u",
                    (unsigned long long)trace_down_us,(unsigned long long){pointer}.timestamp_us,
                    trace_charge_us,trace_ticks,trace_steps,trace_moves,trace_draws,
                    (unsigned long long)trace_max_gap_us,(int)(g_game.vx*1e6f),
                    (int)(g_game.vy*1e6f),(int)(g_game.vz*1e6f),
                    (int)(g_game.land_x*1e6f),(int)(g_game.land_z*1e6f),g_game.state);
                {log}
"""
c = args.output / "jump-jump-3d/main.c"
replace(c, "#include <stdint.h>", "#include <stdint.h>" + fields + """
static unsigned trace_field(char *out, unsigned at, const char *key, int64_t value) {
    char digits[24]; unsigned count=0;
    while(*key) out[at++]=*key++;
    out[at++]='=';
    if(value<0) {out[at++]='-'; value=-value;}
    do {digits[count++]=(char)('0'+value%10);value/=10;} while(value);
    while(count) out[at++]=digits[--count];
    out[at++]=' '; out[at]=0; return at;
}
""")
replace(c, "static int render_frame(void) {", "static int render_frame(void) {\n    if(g_game.state==J3_STATE_CHARGING) ++trace_draws;")
replace(c, "        g_game.rng ^= (uint32_t)timestamp_us;", ticks.replace("{now}", "timestamp_us") + "        g_game.rng ^= (uint32_t)timestamp_us;")
replace(c, "            if (pointer.phase == PXA_POINTER_DOWN) {", "            if(pointer.phase==PXA_POINTER_MOVE) ++trace_moves;\n            if (pointer.phase == PXA_POINTER_DOWN) {\n                " + reset.replace("{pointer}", "pointer"))
replace(c, "                j3_game_release(&g_game);", release.replace("{pointer}", "pointer").replace("{log}", '(void)pxa_log_write(2, trace);'))
source = c.read_text()
start = source.index('                snprintf(trace,sizeof(trace),"J3RELEASE ')
end = source.index('                (void)pxa_log_write(2, trace);', start)
c.write_text(source[:start] + """                const char *keys[]={"down","up","charge_us","ticks","steps","moves","draws","max_gap_us","vx_q6","vy_q6","vz_q6","land_x_q6","land_z_q6","state"};
                const int64_t values[]={trace_down_us,pointer.timestamp_us,trace_charge_us,trace_ticks,trace_steps,trace_moves,trace_draws,trace_max_gap_us,
                    (int)(g_game.vx*1e6f),(int)(g_game.vy*1e6f),(int)(g_game.vz*1e6f),
                    (int)(g_game.land_x*1e6f),(int)(g_game.land_z*1e6f),g_game.state};
                const char *prefix="J3RELEASE ";unsigned at=0;
                while(*prefix) trace[at++]=*prefix++;
                for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);++i)
                    at=trace_field(trace,at,keys[i],values[i]);
""" + source[end:])

cpp = args.output / "jump-jump-3d-cpp/main.cpp"
replace(cpp, "using namespace jump;", "using namespace jump;" + fields)
replace(cpp, "        auto draw=frame_memory.draw();", "        if(g_game.state==J3_STATE_CHARGING) ++trace_draws;\n        auto draw=frame_memory.draw();")
replace(cpp, "        auto steps=stepper.advance(now);", "        auto steps=stepper.advance(now);" + ticks.replace("{now}", "now"))
replace(cpp, "    void pointer(const pxa::ui::CanvasPointer& p){if(!renderer||initializing)return;", "    void pointer(const pxa::ui::CanvasPointer& p){if(!renderer||initializing)return;\n        if(p.phase==pxa::ui::pointer_phase_move) ++trace_moves;")
replace(cpp, "        if(p.phase==pxa::ui::pointer_phase_down){", "        if(p.phase==pxa::ui::pointer_phase_down){\n            " + reset.replace("{pointer}", "p"))
replace(cpp, "            j3_game_release(&g_game);", release.replace("{pointer}", "p").replace("{log}", '(void)context->log().write(pxa::LogLevel::info,trace);'))
print("Prepared isolated release-only diagnostics; build with J3_FORCE_SCALE_SHIFT=0")
