// Exercise the complete original PC demo across repeated level/save lifetimes.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {runGuest} from './guest-host.mjs';

const [wasm,media,output,cyclesArg]=process.argv.slice(2);
const cycles=cyclesArg===undefined?3:Number(cyclesArg);
assert(Number.isInteger(cycles) && cycles>=1 && cycles<=100,'cycles must be 1..100');
if(!output)throw Error('usage: node classic-memory.mjs MAIN.wasm MEDIA_DIR REPORT.json [CYCLES]');
const files=new Map(),g=await runGuest(wasm,media,files,{mediaRoot:media});
const report={maximum_bytes:9*1024*1024,cycles:[]};
try {
    for(let cycle=0;cycle<cycles;++cycle) {
        g.menu(3);
        assert(g.logs.some(s=>s.includes('ready level=3')),'original PC demo did not start');
        g.buttons(1);g.tick(30);g.buttons(0);g.tick(30);
        g.menu(1);g.tick(10);
        assert(files.has('data/classic-save.bin'),'save was not committed');
        g.menu(2);g.tick(30);
        assert(g.logs.filter(s=>s==='OpenLara save restored').length>=cycle+1,
            'save was not restored during this cycle');
        g.menu(6);g.tick(10);
        assert(g.logs.filter(s=>s.includes('ready level=0')).length>=cycle+2,
            'title was not reloaded during this cycle');
        const state=g.info();
        assert(state.linear_bytes<=report.maximum_bytes,'linear memory budget exceeded');
        report.cycles.push(state);
    }
    report.logs=g.logs;
    report.passed=true;
    fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');
    console.log('PASS: '+cycles+' PC demo/title/save/restore cycles; peak linear bytes='+g.info().linear_bytes);
} finally {g.stop();}
