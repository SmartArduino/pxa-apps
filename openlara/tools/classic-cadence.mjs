// Exercise real Clock wire negotiation and frame-boundary cancellation.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {runGuest} from './guest-host.mjs';
const [wasm,assets,out]=process.argv.slice(2);
if(!out)throw Error('usage: node classic-cadence.mjs MAIN.wasm ASSETS REPORT.json');
const report={};
const a=await runGuest(wasm,assets,new Map(),{presentationCostUs:100000});a.menu(4);
let start=a.clockEvents.length;a.tick();let trace=a.clockEvents.slice(start);
assert.deepEqual(trace.map(e=>e.op),['legacy','present','configure']);
assert.equal(trace[0].period_us,0);assert.equal(trace[1].period_us,0);assert.equal(trace[2].period_us,1000);
report.slow_frame_disarms_then_rearms_at_1ms=true;
a.focus(false);let frames=a.info().frames;a.tick(20);assert.equal(a.info().frames,frames);
a.focus(true);a.tick(10);assert(a.info().frames>frames);report.background_resume=true;a.stop();
for(const [key,options] of [['unsupported_timing',{legacyClock:true}],['configuration_error',{failFineClock:true}]]) {
 const g=await runGuest(wasm,assets,new Map(),options);g.menu(4);frames=g.info().frames;g.tick(5);
 assert(g.info().frames>frames);assert.equal(g.info().clock_period_us,16000);
 report[key+'_keeps_legacy_clock']=true;g.stop();
}
const fast=await runGuest(wasm,assets);start=fast.clockEvents.length;fast.tick();trace=fast.clockEvents.slice(start);
assert.equal(trace.at(-1).period_us,16000);report.fast_frames_remain_bounded=true;fast.stop();
fs.writeFileSync(out,JSON.stringify(report,null,2)+'\n');console.log(report);
