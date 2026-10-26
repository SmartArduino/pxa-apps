// Include all title fade frames, not only the fully opaque image.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {runGuest} from './guest-host.mjs';
const [baseline,candidate,assets,output]=process.argv.slice(2);
if(!output)throw Error('usage: node classic-title-compare.mjs BASELINE.wasm CANDIDATE.wasm ASSETS_DIR REPORT.json');
const a=await runGuest(baseline,assets),b=await runGuest(candidate,assets);
try {
    for(let i=0;i<180;++i) {
        a.tick();b.tick();
        assert.deepEqual(b.frame(),a.frame(),'title/fade pixels differ at frame '+i);
    }
    assert.equal(a.info().pcm_sha256,b.info().pcm_sha256,'title audio changed');
    fs.writeFileSync(output,JSON.stringify({passed:true,identical_title_frames:180,pcm_identical:true})+'\n');
    console.log('PASS: 180 title/fade frames and PCM match the original renderer');
} finally {a.stop();b.stop();}
