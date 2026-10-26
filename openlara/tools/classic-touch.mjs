// Validate touch lifetimes against the complete PC demo, not a mock keypad.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {runGuest} from './guest-host.mjs';
const [wasm,assets,output]=process.argv.slice(2);
if(!output)throw Error('usage: node classic-touch.mjs MAIN.wasm ASSETS_DIR REPORT.json');
const a=await runGuest(wasm,assets),b=await runGuest(wasm,assets);
try {
    for(const g of [a,b]) {g.menu(4);g.tick(60);}
    const advance=()=>{for(const g of [a,b])g.tick(10);};
    a.pointer(245,165,1);advance();
    assert.deepEqual(a.frame(),b.frame(),'hover without down triggered jump');
    a.pointer(-5,120,0);a.pointer(245,165,1);advance();
    assert.deepEqual(a.frame(),b.frame(),'outside press became an inside drag');
    a.pointer(245,165,0);a.pointer(400,300,3);advance();
    assert.deepEqual(a.frame(),b.frame(),'cancelled press still triggered jump');
    a.pointer(245,165,0);a.pointer(400,300,2);b.buttons(32);b.buttons(0);advance();
    assert.deepEqual(a.frame(),b.frame(),'brief touch jump differs from brief controller jump');
    for(const g of [a,b])g.tick(90);
    a.pointer(245,165,1);advance();
    assert.deepEqual(a.frame(),b.frame(),'release outside left touch held');
    a.pointer(245,165,0);b.buttons(32);advance();
    assert.deepEqual(a.frame(),b.frame(),'touch did not recover after outside release');
    a.pointer(400,300,2);b.buttons(0);
    fs.writeFileSync(output,JSON.stringify({passed:true,hover_ignored:true,outside_down_ignored:true,
        cancel_releases:true,brief_jump_matches_controller:true,outside_up_releases:true})+'\n');
    console.log('PASS: real PC demo hover, outside down/up, cancel and brief touch/controller jump');
} finally {a.stop();b.stop();}
