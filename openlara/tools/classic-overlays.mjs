// Opaque PXA UI must match the former render-then-clear path.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {runGuest} from './guest-host.mjs';
const [baseline,candidate,assets,out]=process.argv.slice(2);
if(!out)throw Error('usage: node classic-overlays.mjs BASELINE.wasm CANDIDATE.wasm ASSETS REPORT.json');
const report={};
for(const [name,selection] of [['controls',8],['settings',7]]) {
 const a=await runGuest(baseline,assets),b=await runGuest(candidate,assets);
 for(const g of [a,b]) {g.menu(4);g.menu(selection);g.tick(30);}
 fs.writeFileSync(out+'.'+name+'.baseline.rgb565',a.frame());
 fs.writeFileSync(out+'.'+name+'.candidate.rgb565',b.frame());
 assert(b.frame().equals(a.frame()),name+' changed when hidden 3D rendering was removed');
 report[name+'_pixels_identical']=true;a.stop();b.stop();
}
fs.writeFileSync(out,JSON.stringify(report,null,2)+'\n');console.log(report);
