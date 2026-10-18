// Native inventory hints must follow the configured action/cancel buttons.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {runGuest} from './guest-host.mjs';
const [wasm,assets,output]=process.argv.slice(2);
if(!output)throw Error('usage: node classic-controls-settings.mjs WASM ASSETS REPORT.json');
const files=new Map(),g=await runGuest(wasm,assets,files);
function hint(frame) {
    const rows=[];
    for(let y=202;y<=207;++y)rows.push(frame.subarray((y*320+16)*2,(y*320+112)*2));
    return Buffer.concat(rows);
}
function backHint(frame) {
    const rows=[];
    for(let y=202;y<=207;++y)rows.push(frame.subarray((y*320+200)*2,(y*320+304)*2));
    return Buffer.concat(rows);
}
g.tick(180);const defaultHint=hint(g.frame());
g.menu(7);g.press(2);g.press(2);g.press(16);g.tick(100);
assert(g.logs.includes('OpenLara settings committed'));
g.press(32);g.tick(120);
const swappedHint=hint(g.frame());
assert.notDeepEqual(swappedHint,defaultHint,'native hint did not change after swapping A/B');
fs.writeFileSync(output+'.swapped.rgb565',g.frame());g.stop();
const h=await runGuest(wasm,assets,files);h.tick(180);
assert.deepEqual(hint(h.frame()),swappedHint,'swapped hint did not survive settings reload');
h.tick(30);const closed=backHint(h.frame());
h.press(32);h.tick(30);assert.notDeepEqual(backHint(h.frame()),closed,'swapped B did not open the passport');
fs.writeFileSync(output+'.chosen.rgb565',h.frame());
h.press(16);h.tick(30);assert.deepEqual(backHint(h.frame()),closed,'swapped A did not cancel the passport');
fs.writeFileSync(output+'.cancelled.rgb565',h.frame());
assert(!h.logs.some(s=>s.includes('ready level=3')),'cancel selected New Game');h.stop();
const base=await runGuest(wasm,assets),swapped=await runGuest(wasm,assets,files);
for(const guest of [base,swapped]) {guest.menu(4);guest.tick(30);}
base.press(32);swapped.press(16);
for(const guest of [base,swapped])guest.tick(20);
assert.deepEqual(base.frame(),swapped.frame(),'swapped A did not perform the default B jump');
base.stop();swapped.stop();
const report={native_hint_follows_swap:true,swap_hint_persisted:true,swapped_B_confirms_A_cancels:true,
    swapped_A_matches_default_B_jump:true};
fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');console.log(report);
