import fs from 'node:fs';
import assert from 'node:assert/strict';
import {runGuest} from './guest-host.mjs';
const [wasm,assets,out]=process.argv.slice(2);
if(!out)throw Error('usage:node classic-resolution.mjs MAIN.wasm ASSETS REPORT.json');
function width(g) {return Number(g.logs.filter(s=>s.includes('OpenLara video gameplay=')).at(-1).match(/gameplay=(\d+)/)[1]);}
const report={};
for(const target of [240,160]) {
 const files=new Map(),setup=await runGuest(wasm,assets,files);
 while(width(setup)!==target) {
  setup.menu(7);for(let i=0;i<5;++i)setup.press(2);setup.press(16);setup.tick(60);setup.press(32);
 }
 setup.stop();
 // Give both instances the same persisted settings/read path and startup
 // timing; only the final reserved byte (video mode) differs.
 const nativeFiles=new Map([...files].map(([k,v])=>[k,Buffer.from(v)]));
 const settings=nativeFiles.get('data/classic-settings.bin');assert(settings);settings[settings.length-1]=0;
 const a=await runGuest(wasm,assets,nativeFiles),b=await runGuest(wasm,assets,files);
 assert.equal(width(a),320);
 assert.equal(width(b),target,'resolution did not persist');
 for(const g of [a,b])g.tick(180);
 assert.deepEqual(a.frame(),b.frame(),'title UI should remain native');
 for(const g of [a,b]) {g.menu(4);g.tick(180);}
 assert.notDeepEqual(a.frame(),b.frame(),'gameplay resolution had no effect');
 const actions=new Map([[0,8],[30,0],[45,1],[100,0],[120,32],[121,0],[150,256],[151,0],[180,16],[210,0]]);
 for(let i=0;i<240;++i)for(const g of [a,b]) {if(actions.has(i))g.buttons(actions.get(i));g.tick();}
 for(const g of [a,b]) {g.press(64);g.tick(30);}
 assert.deepEqual(a.frame(),b.frame(),'native inventory UI should remain unchanged');
 for(const g of [a,b]) {g.press(64);g.tick(30);}
 for(const g of [a,b])g.menu(1);
 assert.deepEqual(files.get('data/classic-save.bin'),nativeFiles.get('data/classic-save.bin'),'render resolution changed complete saved gameplay state');
 for(const g of [a,b])g.menu(8);
 assert.deepEqual(a.frame(),b.frame(),'platform controls UI should remain native');
 a.stop();b.stop();
 const c=await runGuest(wasm,assets,files);assert.equal(width(c),target);c.menu(2);c.tick(30);
 assert(c.logs.includes('OpenLara save restored'));c.stop();
 report[target]={persisted:true,complete_gameplay_save_identical:true,native_title_inventory_and_platform_ui:true,checkpoint_restored:true};
}
fs.writeFileSync(out,JSON.stringify(report,null,2)+'\n');console.log(report);
