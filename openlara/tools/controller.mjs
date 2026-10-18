// Exercise real Guest controller events, independently of SDL's input adapter.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {runGuest} from './guest-host.mjs';
const [wasm,assets,output]=process.argv.slice(2);
if(!output)throw Error('usage: node controller.mjs MAIN.wasm ASSETS_DIR OUTPUT.json');
const report={};
async function guest() {const g=await runGuest(wasm,assets);g.menu(3);return g;}
for(const [name,mask,x,y] of [['start_inventory',64,200,20],['select_pause',128,180,220],['action',16,280,200],['jump',32,240,150],['weapon',256,200,100]]) {
    const pad=await guest(),touch=await guest();
    pad.press(mask);touch.pointer(x,y,0);touch.tick();touch.pointer(x,y,2);touch.tick();
    pad.tick(40);touch.tick(40);
    assert.deepEqual(pad.frame(),touch.frame(),name+' differs from touch equivalent');
    report[name+'_matches_touch']=true;pad.stop();touch.stop();
}
for(const lifecycle of [false,true]) {
    const a=await guest(),b=await guest();
    for(const g of [a,b]) {g.buttons(8);g.tick(30);}
    b.buttons(0);
    if(lifecycle) {for(const g of [a,b]) {g.focus(false);g.tick(2);g.focus(true);}}
    else a.buttons(0,false);
    a.tick(90);b.tick(90);
    assert.deepEqual(a.frame(),b.frame(),lifecycle?'held key survived background':'held key survived disconnect');
    report[lifecycle?'background_clears_held_keys':'disconnect_clears_held_keys']=true;
    a.stop();b.stop();
}
const menu=await guest();menu.press(128);for(let n=0;n<4;++n)menu.press(2);menu.press(16);menu.tick(100);
assert(menu.logs.some(s=>s.includes('ready level=3')));report.controller_only_level_selection=true;menu.stop();
fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');console.log(report);
