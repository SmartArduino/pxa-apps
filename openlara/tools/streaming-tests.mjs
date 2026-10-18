import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import {runGuest} from './guest-host.mjs';
const [wasm,fixtures,output]=process.argv.slice(2);
if(!output)throw Error('usage: node streaming-tests.mjs MAIN.wasm DATA_TESTS_DIR REPORT.json');
const report={};
for(const [name,folder,options] of [
    ['pcm','pcm-assets',{}],['large_pcm','large-pcm-assets',{}],
    ['short_pcm','large-pcm-assets',{musicChunk:97}],
    ['sd_pcm','large-pcm-assets',{mediaRoot:path.join(fixtures,'large-pcm-assets/game'),ioChunk:1024}],
]) {
    const g=await runGuest(wasm,path.join(fixtures,folder),new Map(),options);g.tick(240);
    assert(!g.logs.some(s=>s.includes('music media read failed')),name);
    report[name]=g.info();assert(report[name].nonzero>10000,name+' PCM audio absent');
    g.stop();
}
assert.equal(report.pcm.pcm_sha256,report.large_pcm.pcm_sha256,'bank offsets changed decoded PCM');
assert.equal(report.pcm.pcm_sha256,report.short_pcm.pcm_sha256,'short reads changed decoded PCM');
assert.equal(report.pcm.linear_bytes,report.large_pcm.linear_bytes,'large bank was loaded into Guest memory');
report.byte_exact_across_large_bank_and_short_reads=true;
report.large_bank_bytes=fs.statSync(path.join(fixtures,'large-pcm-assets/game/tracks.ad4')).size;
fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report,null,2));
