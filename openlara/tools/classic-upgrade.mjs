// Existing real OLS2 checkpoints must survive an engine upgrade unchanged.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {runGuest} from './guest-host.mjs';
import {checkpoint,telemetry,assertRestored} from './classic-save-probe.mjs';
const [baseline,candidate,assets,out]=process.argv.slice(2);
if(!out)throw Error('usage: node classic-upgrade.mjs BASELINE.wasm CANDIDATE.wasm ASSETS REPORT.json');
const lara=JSON.parse(fs.readFileSync(assets+'/game/data-manifest.json')).levels.find(l=>l.id===3).lara_entities[0];
const files=new Map(),key='data/classic-save.bin',a=await runGuest(baseline,assets,files);
a.menu(4);a.buttons(1);a.tick(40);a.buttons(8);a.tick(20);a.buttons(0);a.menu(1);
const saved=checkpoint(files.get(key),lara);assert.notDeepEqual(saved.position,lara.position);a.stop();
const b=await runGuest(candidate,assets,files);b.menu(2);b.tick(180);assertRestored(telemetry(b.logs),saved);
b.menu(1);const upgraded=checkpoint(files.get(key),lara);assert.deepEqual(upgraded.position,saved.position);b.stop();
const c=await runGuest(baseline,assets,files);c.menu(2);c.tick(180);assertRestored(telemetry(c.logs),upgraded);c.stop();
const report={baseline_checkpoint_loaded:true,candidate_checkpoint_backward_readable:true,
 saved_position:saved.position,saved_rotation:saved.rotation,upgraded_rotation:upgraded.rotation,yaw_tolerance_units:1};
fs.writeFileSync(out,JSON.stringify(report,null,2)+'\n');console.log(report);
