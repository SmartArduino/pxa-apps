import fs from 'node:fs';
import assert from 'node:assert/strict';
import {runGuest} from './guest-host.mjs';
const [oldWasm,newWasm,media,out]=process.argv.slice(2);
if(!out)throw Error('usage: node classic-scale-compare.mjs BASELINE.wasm CANDIDATE.wasm MEDIA_ROOT REPORT.json');
const result={passed:false,profiles:{}};
function width(g) {return Number(g.logs.filter(s=>s.includes('OpenLara video gameplay=')).at(-1).match(/gameplay=(\d+)/)[1]);}
const clone=files=>new Map([...files].map(([k,v])=>[k,Buffer.from(v)]));
for(const target of [240,160]) {
 const files=new Map(),setup=await runGuest(oldWasm,media,files);
 while(width(setup)!==target) {setup.menu(7);for(let n=0;n<5;++n)setup.press(2);setup.press(16);setup.tick(60);setup.press(32);}
 setup.stop();const af=clone(files),bf=clone(files);
 const a=await runGuest(oldWasm,media,af,{stopAtReady:true,alignLevelTime:true}),b=await runGuest(newWasm,media,bf,{stopAtReady:true,alignLevelTime:true});
 try {
  for(const g of [a,b]){assert.equal(width(g),target);g.menu(4);g.tick(30);}
  for(let i=0;i<120;++i) {
   if(i%30===0)for(const g of [a,b])g.buttons([8,0,1,0][i/30]);
   a.tick();b.tick();assert(b.frame().equals(a.frame()),target+' scale changed frame '+i);
  }
  for(const g of [a,b]){g.buttons(0);g.menu(1);}
  assert.deepEqual(bf.get('data/classic-save.bin'),af.get('data/classic-save.bin'),'saved gameplay state changed');
  assert.equal(a.info().pcm_sha256,b.info().pcm_sha256,'audio changed');
  result.profiles[target]={identical_frames:120,checkpoint_identical:true,pcm_identical:true};
 } finally {a.stop();b.stop();}
}
result.passed=true;fs.writeFileSync(out,JSON.stringify(result,null,2)+'\n');console.log(result);
