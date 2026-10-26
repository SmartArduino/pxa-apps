// Real PC level loading under deterministic Host scheduling and adverse reads.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {runGuest} from './guest-host.mjs';
const [baseline,candidate,media,output]=process.argv.slice(2);
if(!output)throw Error('usage: node classic-loading-compare.mjs BASELINE.wasm CANDIDATE.wasm MEDIA_DIR REPORT.json');
const report={passed:false,scope:'Deterministic Host callback counts; not hardware load times',cases:{}};
async function load(wasm,options) {
    const g=await runGuest(wasm,media,new Map(),{stopAtReady:true,...options});
    try {
        g.menu(4);assert(g.logs.some(s=>s.includes('ready level=3')),'PC level did not load');
        const info=g.info(),[name,level]=Object.entries(info.mediaReads).find(([n,v])=>n.endsWith('.phd') && v.bytes>2000000)??[];
        assert(level,'full PC level was not read');return {name,level,max_event_reads:info.maxEventMediaReads,blocked:info.blockedMediaReads};
    } finally {g.stop();}
}
for(const [name,options] of [['fast',{}],['slow',{mediaReadCostUs:9000}],['short',{ioChunk:257}],['blocked',{mediaWouldBlock:true}]]) {
    console.log('CHECK',name);
    const a=await load(baseline,options),b=await load(candidate,options);
    assert.equal(b.level.bytes,a.level.bytes);assert(b.max_event_reads<=8,'callback read-count bound exceeded');
    if(name==='fast')assert(b.level.batches<=Math.ceil(a.level.batches/8)+1,'fast reads still wait for every block');
    if(name==='slow')assert.equal(b.max_event_reads,1,'slow I/O exceeded the callback time budget');
    if(name==='blocked')assert(b.blocked>0);
    report.cases[name]={baseline:a,candidate:b};
    fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');
}
const [a,b]=await Promise.all([baseline,candidate].map(w=>runGuest(w,media,new Map(),{stopAtReady:true,alignLevelTime:true})));
try {
    for(const g of [a,b])g.menu(4);
    for(let i=0;i<180;++i){if(i===30||i===80)for(const g of [a,b])g.buttons(i===30?8:0);a.tick();b.tick();assert(b.frame().equals(a.frame()),'post-load frame '+i);}
    report.identical_post_load_frames=180;
} finally {a.stop();b.stop();}
const target=report.cases.fast.candidate.name;
const truncated=await runGuest(candidate,media,new Map(),{stopAtReady:true,truncateMedia:target});
try {truncated.menu(4);assert(truncated.logs.some(s=>s.includes('SD read failed or truncated')));assert(!truncated.logs.some(s=>s.includes('ready level=3')));report.truncation_rejected=true;} finally {truncated.stop();}
const paused=await runGuest(candidate,media,new Map(),{stopAtReady:true,ioChunk:1024});
try {
    paused.press(128);for(let i=0;i<4;++i)paused.press(2);paused.press(16);
    paused.focus(false);const before=paused.info().mediaReads[target].bytes;paused.tick(100);
    assert.equal(paused.info().mediaReads[target].bytes,before,'background loading continued');
    paused.focus(true);paused.waitFor('ready level=3');report.background_pause_resume=true;
} finally {paused.stop();}
report.passed=true;fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');
console.log('PASS: fast/slow/short/blocked reads, callback bounds, full bytes, 180 post-load frames, truncation and background/resume');
console.log('Full PC level callbacks: '+report.cases.fast.baseline.level.batches+' -> '+report.cases.fast.candidate.level.batches);
