import fs from 'node:fs';
import assert from 'node:assert/strict';
import {runGuest} from './guest-host.mjs';
import {checkpoint} from './classic-save-probe.mjs';
const [baseline,candidate,assets,out]=process.argv.slice(2);
if(!out)throw Error('usage: node classic-latency.mjs BASELINE.wasm CANDIDATE.wasm ASSETS REPORT.json');
const lara=JSON.parse(fs.readFileSync(assets+'/game/data-manifest.json')).levels.find(x=>x.id===3).lara_entities[0];
async function slow(wasm) {
 const files=new Map(),a=await runGuest(wasm,assets,files);a.menu(4);a.menu(1);
 const initial=checkpoint(files.get('data/classic-save.bin'),lara);
 a.buttons(8);a.tick(10,333334);a.buttons(0);a.menu(1);
 const saved=checkpoint(files.get('data/classic-save.bin'),lara);a.stop();
 return {initial_rotation:initial.rotation,rotation:saved.rotation,position:saved.position};
}
async function paused(ticks) {
 const files=new Map(),a=await runGuest(candidate,assets,files);a.menu(4);a.buttons(1);a.tick(30);a.buttons(0);
 a.press(128);a.tick(ticks,500000);a.press(128);a.tick(30);a.menu(1);
 const result=checkpoint(files.get('data/classic-save.bin'),lara);a.stop();return {position:result.position,rotation:result.rotation};
}
async function pulse(wasm,mode) {
 const g=await runGuest(wasm,assets);g.menu(4);g.tick(30);
 if(mode!=='none')g.buttons(32);
 if(mode==='brief')g.buttons(0);
 g.tick();g.buttons(0);g.tick(10);const frame=g.frame();g.stop();return frame;
}
async function lifecycle(ticks) {
 const files=new Map(),g=await runGuest(candidate,assets,files);g.menu(4);g.buttons(1);g.tick(30);g.buttons(0);
 g.focus(false);g.tick(ticks,500000);g.focus(true);g.tick(30);g.menu(1);
 const saved=checkpoint(files.get('data/classic-save.bin'),lara);g.stop();return {position:saved.position,rotation:saved.rotation};
}
const before=await slow(baseline),after=await slow(candidate),short=await paused(1),long=await paused(30);
async function releases(wasm) {
 const g=await runGuest(wasm,assets);g.menu(4);const before=g.info().frames;
 for(let i=0;i<20;++i)g.release();
 const rendered=g.info().frames-before;g.buttons(8);g.buttons(0);g.tick();
 assert(g.info().frames>before,'clock did not resume frame production');g.stop();return rendered;
}
const releaseBefore=await releases(baseline),releaseAfter=await releases(candidate);
assert(releaseBefore>0,'baseline no longer reproduces release-driven rendering');
assert.equal(releaseAfter,0,'surface releases still run expensive frames ahead of controller delivery');
assert.equal(before.rotation,before.initial_rotation,'baseline no longer reproduces slow-frame input freeze');
assert.notEqual(after.rotation,after.initial_rotation,'candidate still freezes input below4FPS');
assert.deepEqual(short,long,'long PXA pause was added to the resumed simulation');
assert.deepEqual(await pulse(baseline,'brief'),await pulse(baseline,'none'),'baseline no longer reproduces lost brief button');
assert.deepEqual(await pulse(candidate,'brief'),await pulse(candidate,'held'),'brief candidate press did not reach a game update');
assert.notDeepEqual(await pulse(candidate,'brief'),await pulse(candidate,'none'),'latched brief button had no game effect');
assert.deepEqual(await lifecycle(0),await lifecycle(30),'background time was added to resumed simulation');
const report={scope:'actual complete City Guest; slow frames, release notifications, brief input, pause/lifecycle',baseline:before,candidate:after,release_frames:{baseline:releaseBefore,candidate:releaseAfter},slow_frame_input_fixed:true,brief_button_delivered_once:true,background_does_not_catch_up:true,pause_duration_does_not_change_pose:true,pause_tick_us:500000,paused_pose:long};
fs.writeFileSync(out,JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report));
