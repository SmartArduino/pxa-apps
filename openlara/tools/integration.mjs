// Exercise the actual WASM Guest; this does not measure physical-device FPS.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {runGuest} from './guest-host.mjs';
const [wasm,assets,output]=process.argv.slice(2);
if(!output)throw Error('usage: node integration.mjs MAIN.wasm ASSETS_DIR REPORT.json');
const files=new Map(),report={};
const guest=options=>runGuest(wasm,assets,files,options);
const a=await guest({partialWrite:true});a.tick(180);assert(a.info);a.menu(3);a.buttons(8);a.tick(25);a.buttons(0);a.tick(30);a.menu(1);
assert(a.logs.includes('OpenLara save committed'),'save not committed');const saved=Buffer.from(files.get('data/save.bin'));
assert(saved.length>256);a.buttons(1);a.tick(40);a.buttons(0);report.first=a.info();assert(report.first.nonzero>1000);assert(report.first.blocks>0,'backpressure not exercised');a.stop();
const b=await guest();b.menu(2);if(!b.logs.includes('OpenLara save restored')) {console.log(b.logs);fs.writeFileSync(output+'.save',saved);}
assert(b.logs.includes('OpenLara save restored'),'save not restored');b.menu(1);assert(b.logs.includes('OpenLara save committed'));if(!files.get('data/save.bin').subarray(244,254).equals(saved.subarray(244,254))) {console.log('SAVED',saved.subarray(244,280),'AFTER',files.get('data/save.bin').subarray(244,280),b.logs);fs.writeFileSync(output+'.after',files.get('data/save.bin'));fs.writeFileSync(output+'.save',saved);}
assert(files.get('data/save.bin').subarray(244,254).equals(saved.subarray(244,254)),'restored Lara position/angles changed');report.restored=b.info();b.stop();
const beforeFailure=Buffer.from(files.get('data/save.bin'));
const c=await guest({failReplace:true});c.menu(3);c.menu(1);assert(c.logs.some(s=>s.includes('persistence failed')));assert(files.get('data/save.bin').equals(beforeFailure));report.failed_replace_previous_kept=true;c.stop();
files.set('data/save.bin',Buffer.from(saved));files.get('data/save.bin')[20]^=0xff;
const d=await guest({denyAudio:true});d.menu(3);d.menu(2);assert(!d.logs.includes('OpenLara save restored'));assert(d.logs.some(s=>s.includes('corrupt private file')));report.corrupt_save_rejected=true;report.audio_denied=d.info();d.stop();
files.set('data/save.bin',saved.subarray(0,37));const e=await guest();e.menu(2);assert(!e.logs.includes('OpenLara save restored'));assert(e.logs.some(s=>s.includes('corrupt private file')));report.truncated_save_rejected=true;e.stop();
const f=await guest();f.menu(7);f.press(2);f.press(16);f.tick(100);
assert(f.logs.includes('OpenLara settings committed'));assert(files.has('data/settings.bin'));f.stop();
const g=await guest();g.tick(180);const opening=g.info().nonzero;g.tick(180);const silent=g.info();assert.equal(silent.nonzero-opening,0,'music setting was not retained after restart');
g.menu(7);g.press(2);g.press(16);g.tick(100);g.stop();
const h=await guest();h.tick(180);const audible=h.info();assert(audible.nonzero>1000,'music did not resume after settings restore');
h.menu(4);assert(h.logs.some(s=>s.includes('ready level=3')));h.menu(1);h.menu(2);
assert.equal(h.logs.filter(s=>s==='OpenLara save restored').length,1,'City save/restore failed');
h.menu(5);assert(h.logs.some(s=>s.includes('ready level=1')));h.menu(1);h.menu(2);
assert.equal(h.logs.filter(s=>s==='OpenLara save restored').length,2,'Gym save/restore failed');h.stop();
report.settings_persisted={disabled_steady_nonzero:silent.nonzero-opening,enabled_nonzero:audible.nonzero};report.all_levels_loaded=true;report.all_playable_levels_saved_and_restored=true;
report.logs={first:a.logs,restored:b.logs};fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report,null,2));
