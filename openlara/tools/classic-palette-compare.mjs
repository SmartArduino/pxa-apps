// Compare palette invalidation in the real Guest, including persisted gamma.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {runGuest} from './guest-host.mjs';
const [baseline,candidate,media,output]=process.argv.slice(2);
if(!output)throw Error('usage: node classic-palette-compare.mjs BASELINE.wasm CANDIDATE.wasm MEDIA_DIR REPORT.json');
const af=new Map(),bf=new Map();
let a=await runGuest(baseline,media,af,{stopAtReady:true,alignLevelTime:true}),b=await runGuest(candidate,media,bf,{stopAtReady:true,alignLevelTime:true});
let compared=0;
function compare(frames,label) {
    for(let i=0;i<frames;++i) {a.tick();b.tick();assert(b.frame().equals(a.frame()),label+' frame '+i);++compared;}
}
function gamma(g) {g.menu(7);for(let i=0;i<4;++i)g.press(2);g.press(16);g.tick(20);g.press(32);}
try {
    for(const g of [a,b]) {g.menu(4);assert(g.logs.some(s=>s.includes('ready level=3')),'PC demo was not loaded');}
    // Includes wraparound to zero; both palette arrays keep the same address.
    for(let level=0;level<8;++level) {
        for(const g of [a,b])gamma(g);
        compare(30,'brightness '+level);
        if(level===3) {for(const g of [a,b]) {g.menu(6);g.menu(4);}compare(30,'level reload');}
    }
    for(const g of [a,b])gamma(g);
    compare(20,'nonzero gamma');
    a.stop();b.stop();a=await runGuest(baseline,media,af,{stopAtReady:true,alignLevelTime:true});b=await runGuest(candidate,media,bf,{stopAtReady:true,alignLevelTime:true});
    for(const g of [a,b])g.menu(4);
    compare(60,'persisted gamma');
    for(const g of [a,b])g.menu(1);
    assert.deepEqual(bf.get('data/classic-save.bin'),af.get('data/classic-save.bin'),'game state changed');
    assert.equal(b.info().pcm_sha256,a.info().pcm_sha256,'PCM changed');
    fs.writeFileSync(output,JSON.stringify({passed:true,identical_frames:compared,gamma_steps:9,level_reload:true,persisted_gamma:true,checkpoint_identical:true,pcm_identical:true},null,2)+'\n');
    console.log('PASS: '+compared+' frames, palette/gamma wrap, reload, persisted settings and checkpoint');
} finally {a.stop();b.stop();}
