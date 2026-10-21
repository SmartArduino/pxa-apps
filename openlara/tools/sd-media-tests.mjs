// Execute the actual Guest with absent storage, missing data, and blocked reads.
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import assert from 'node:assert/strict';
import {runGuest} from './guest-host.mjs';
const [wasm,media,output]=process.argv.slice(2);
if(!output)throw Error('usage: node sd-media-tests.mjs MAIN.wasm MEDIA_ROOT OUTPUT.json');
const report={};
const absent=await runGuest(wasm,media,new Map(),{mediaRoot:null,allowLoadFailure:true});
assert(absent.logs.some(s=>s.includes('SD media unavailable')));
assert.equal(absent.info().frames,0);absent.stop();report.no_media_no_asset_fallback=true;
const empty=fs.mkdtempSync(path.join(os.tmpdir(),'openlara-empty-media-'));
try {
    const missing=await runGuest(wasm,media,new Map(),{mediaRoot:empty,allowLoadFailure:true});
    assert(missing.logs.some(s=>s.includes('missing SD game data')));
    assert.equal(missing.info().frames,0);missing.stop();report.missing_media_no_asset_fallback=true;
} finally {fs.rmSync(empty,{recursive:true});}
const blocked=await runGuest(wasm,media,new Map(),{mediaWouldBlock:true,ioChunk:2048,musicChunk:97});
blocked.menu(3);assert(blocked.logs.some(s=>s.includes('ready level=3')));
blocked.tick(400);assert(!blocked.logs.some(s=>s.includes('read failed')));
assert(blocked.info().blockedMediaReads>100);assert(blocked.info().nonzero>1000);
report.blocked_and_short_reads=blocked.info();blocked.stop();
fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report));
