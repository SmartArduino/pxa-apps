// Compare actual RGB565 frames at identical fixed times and scripted input.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {runGuest} from './guest-host.mjs';
const [reference,candidate,assets,output]=process.argv.slice(2);
if(!output)throw Error('usage: node compare-render.mjs REFERENCE.wasm CANDIDATE.wasm ASSETS_DIR OUTPUT.json');
async function run(wasm) {
    const g=await runGuest(wasm,assets);g.menu(3);
    const actions=new Map([[30,8],[60,0],[90,1],[150,0],[180,32],[190,0],[210,256],[211,0],[230,16],[280,0]]);
    for(let i=0;i<300;++i) {if(actions.has(i))g.buttons(actions.get(i));g.tick();}
    const result=g.info();g.stop();return result;
}
const baseline=await run(reference),optimized=await run(candidate);
assert.equal(baseline.frames,optimized.frames);
assert.equal(baseline.frame_sha256,optimized.frame_sha256,'rendered pixels changed');
const report={scope:'Title, touch-equivalent menu, Caves, 300 frames of turn/walk/jump/weapon/action',identical:true,baseline,optimized};
fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report,null,2));
