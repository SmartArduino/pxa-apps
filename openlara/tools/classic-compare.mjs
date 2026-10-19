// Compare every City RGB565 frame under identical input and fixed timing.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import {runGuest} from './guest-host.mjs';
const [reference,candidate,assets,output,checkpointFlag]=process.argv.slice(2);
if(!output)throw Error('usage: node classic-compare.mjs REFERENCE.wasm CANDIDATE.wasm ASSETS_DIR REPORT.json [--checkpoint]');
// Checkpoints also compare hidden simulation state and require the same
// adapter generation. Pixel checks can compare older renderer adapters too.
const compareCheckpoint=checkpointFlag==='--checkpoint';
async function run(wasm) {
    const files=new Map(),g=await runGuest(wasm,assets,files);g.menu(4);g.tick(180);
    const actions=new Map([[0,8],[30,0],[45,1],[100,0],[120,32],[121,0],
        [150,256],[151,0],[180,16],[210,0],[240,1024],[260,0],[275,512],[300,0]]);
    const hashes=[];
    for(let i=0;i<360;++i) {
        if(actions.has(i))g.buttons(actions.get(i));g.tick();
        hashes.push(crypto.createHash('sha256').update(g.frame()).digest('hex'));
    }
    if(compareCheckpoint)g.menu(1);
    const save=files.get('data/classic-save.bin');
    if(compareCheckpoint)assert(save,'checkpoint was not written');
    const result={info:g.info(),hashes,save};g.stop();return result;
}
const baseline=await run(reference),optimized=await run(candidate);
assert.deepEqual(optimized.hashes,baseline.hashes,'City pixels changed');
assert.equal(optimized.info.pcm_sha256,baseline.info.pcm_sha256,'PCM audio changed');
if(compareCheckpoint)assert.deepEqual(optimized.save,baseline.save,'Complete game checkpoint changed');
const checkpoint_sha256=compareCheckpoint?crypto.createHash('sha256').update(baseline.save).digest('hex'):null;
delete baseline.save;delete optimized.save;
const report={scope:'360 complete City frames: turn, walk, jump, weapon, action, look, slow walk; title UI excluded',
    identical:true,pcm_identical:true,complete_checkpoint_identical:compareCheckpoint,checkpoint_sha256,baseline,optimized};
fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify({identical:true,frames:baseline.hashes.length,baseline:baseline.info,optimized:optimized.info},null,2));
