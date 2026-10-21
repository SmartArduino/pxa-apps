// Real complete-engine Guest checks using original PC demo data. No device FPS.
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import {runGuest,mediaDirectory} from './guest-host.mjs';
import {checkpoint as decodeCheckpoint,telemetry,assertRestored} from './classic-save-probe.mjs';
const [wasm,assets,output]=process.argv.slice(2);
if(!output)throw Error('usage: node classic-integration.mjs MAIN.wasm ASSETS_DIR REPORT.json');
const files=new Map([['data/save.bin',Buffer.from('legacy fixed save kept')]]),report={};
const savedPath='data/classic-save.bin';
const manifest=JSON.parse(fs.readFileSync(path.join(mediaDirectory(assets),'data-manifest.json'))),
    lara=manifest.levels.find(level=>level.id===3)?.lara_entities?.[0];
assert(lara,'prepare PC demo data again to include Lara entity metadata');
const checkpoint=file=>decodeCheckpoint(file,lara);
async function guest(options={}) {return runGuest(wasm,assets,files,options);}
const a=await guest({partialWrite:true});
a.tick(180);
const titlePixels=a.frame(),titleArt=fs.readFileSync(path.join(mediaDirectory(assets),'title.scr')),
    titlePalette=fs.readFileSync(path.join(mediaDirectory(assets),'title.pal'));
let colored=0;
for(const y of [20,30])for(const x of [80,120,160,200,240]) {
    const expected=titlePalette.readUInt16LE(2*titleArt[Math.floor(y*160/240)*240+Math.floor(x*240/320)]);
    assert.equal(titlePixels.readUInt16LE((y*320+x)*2),expected,'title art/palette is missing');
    colored+=expected!==0;
}
assert(colored>0);report.title_art_palette=true;
a.menu(3);assert(a.logs.some(s=>s.includes('ready level=3')),'demo New Game must choose installed Vilcabamba');
a.buttons(1);a.tick(40);a.buttons(0);a.tick(30);a.menu(1);
assert(a.logs.includes('OpenLara save committed'));const saved=Buffer.from(files.get(savedPath)),pose=Buffer.from(checkpoint(saved).pose);
const savedLara=checkpoint(saved);
assert.throws(()=>decodeCheckpoint(saved,{...lara,index:0}),/not Lara/,'trap record was accepted as player state');
assert.notDeepEqual(savedLara.position,lara.position,'Lara did not move before saving');
a.buttons(8);a.tick(30);a.buttons(0);a.tick(180);
const displaced=telemetry(a.logs);
assert.notEqual(displaced[3]&65535,savedLara.rotation,'turn input did not change Lara rotation');
a.menu(2);a.tick(180);assertRestored(telemetry(a.logs),savedLara);a.menu(1);
assert(a.logs.includes('OpenLara save restored'));assert.deepEqual(checkpoint(files.get(savedPath)).pose,pose);
report.lara_entity_index=lara.index;report.saved_lara={position:savedLara.position,rotation:savedLara.rotation,room:savedLara.room};
report.displaced_lara=displaced;report.restored_lara=telemetry(a.logs);
assert.equal(files.get('data/save.bin').toString(),'legacy fixed save kept');
report.same_instance_restore=true;report.run=a.info();assert(report.run.nonzero>1000);assert(report.run.blocks>0);a.stop();
const b=await guest();b.menu(2);b.tick(180);assertRestored(telemetry(b.logs),savedLara);b.menu(1);
assert(b.logs.includes('OpenLara save restored'));assert.deepEqual(checkpoint(files.get(savedPath)).pose,pose);
report.cross_instance_restore=true;b.stop();
const beforeFailure=Buffer.from(files.get(savedPath));
const c=await guest({failReplace:true});c.menu(3);c.menu(1);
assert(c.logs.some(s=>s.includes('persistence failed')));assert.deepEqual(files.get(savedPath),beforeFailure);
report.failed_replace_previous_kept=true;c.stop();
const corrupt=Buffer.from(saved);corrupt[20]^=255;files.set(savedPath,corrupt);
const d=await guest({denyAudio:true});d.menu(3);d.menu(2);
assert(d.logs.some(s=>s.includes('corrupt private file')));assert(!d.logs.includes('OpenLara save restored'));
assert(d.info().denied);assert.equal(d.info().submitted,0);report.corrupt_save_rejected=true;d.stop();
// Valid outer checksum with malicious internal record length must also fail.
function crc(data) {let c=0xffffffff;for(const v of data) {c^=v;for(let j=0;j<8;++j)c=(c>>>1)^(0xedb88320&-(c&1));}return (~c)>>>0;}
const forged=Buffer.from(saved);forged.writeUInt32LE(0x7fffffff,20);forged.writeUInt32LE(crc(forged.subarray(16)),12);files.set(savedPath,forged);
const e=await guest();e.menu(2);assert(e.logs.some(s=>s.includes('invalid classic save')));assert(!e.logs.includes('OpenLara save restored'));
report.forged_record_rejected=true;e.stop();files.set(savedPath,saved);
const f=await guest();f.menu(7);f.press(2);f.press(16);f.tick(100);
assert(f.logs.includes('OpenLara settings committed'));f.stop();
const g=await guest();g.tick(180);const muted=g.info().nonzero;g.tick(180);
assert.equal(g.info().nonzero-muted,0,'music disabled setting was not restored');
g.menu(7);g.press(2);g.press(16);g.tick(120);assert(g.info().nonzero>muted+1000,'music did not restart after enable');
report.music_setting_persisted=true;g.stop();
const h=await guest({mediaRoot:mediaDirectory(assets),ioChunk:2048});
h.menu(3);h.waitFor('ready level=3');h.tick(180);
assert(!h.logs.some(s=>s.includes('music media read failed')));assert(h.info().nonzero>1000);
report.sd_seek_and_short_reads=true;report.sd=h.info();h.stop();
const i=await guest({musicChunk:97});i.tick(240);
assert(!i.logs.some(s=>s.includes('music media read failed')));assert(i.info().nonzero>1000);
report.short_asset_music_reads=true;i.stop();
const j=await guest();j.menu(9);j.press(2);j.press(2);j.press(2);j.press(16);j.tick(30);
assert(!j.logs.some(s=>s.includes('ready level=4')),'uninstalled level was opened');j.press(32);j.press(32);
j.menu(3);j.tick(90);fs.writeFileSync(output+'.rgb565',j.frame());report.level_catalog_missing_guard=true;
report.final=j.info();j.stop();
const k=await guest(),transitionMemory=[];
for(let n=0;n<20;++n) {
    k.menu(4);k.menu(6);transitionMemory.push(k.info().linear_bytes);
}
assert(transitionMemory.slice(10).every(n=>n===transitionMemory[10]),'level transitions keep growing memory');
report.repeated_level_transition_memory=transitionMemory;k.stop();
fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report,null,2));
