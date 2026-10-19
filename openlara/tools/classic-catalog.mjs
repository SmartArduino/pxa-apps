import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import assert from 'node:assert/strict';
import {runGuest} from './guest-host.mjs';
const [wasm,assets,out]=process.argv.slice(2),report={};
if(!out)throw Error('usage: node classic-catalog.mjs MAIN.wasm ASSETS REPORT.json');
const dir=fs.mkdtempSync(path.join(os.tmpdir(),'openlara-data-inbox-'));
function wait(a,text,start) {for(let i=0;i<2000 && !a.logs.slice(start).some(s=>s.includes(text));++i)a.tick();assert(a.logs.slice(start).some(s=>s.includes(text)),text);}
try {
 for(const name of ['title.phd','level2.phd','title.scr','title.pal','tracks.ad4','levels.idx'])fs.copyFileSync(path.join(assets,'game',name),path.join(dir,name));
 const a=await runGuest(wasm,assets,new Map(),{mediaRoot:dir});a.menu(4);assert(a.mediaOpens.includes('level2.phd'));
 fs.mkdirSync(path.join(dir,'DATA'));
 for(const name of ['title.phd','level2.phd'])fs.copyFileSync(path.join(dir,name),path.join(dir,'DATA',name.toUpperCase()));
 a.menu(10);const help=a.frame();fs.writeFileSync(out+'.help.rgb565',help);const start=a.logs.length;a.press(16);wait(a,'ready level=0',start);a.tick(180);a.menu(4);
 assert(a.mediaOpens.filter(n=>n==='DATA/LEVEL2.PHD').length>=2,'rescan found the header but actual level loading did not use DATA');a.stop();report.uppercase_folder_rescan_loads_actual_level=true;
 fs.renameSync(path.join(dir,'DATA'),path.join(dir,'data'));
 for(const name of ['TITLE.PHD','LEVEL2.PHD'])fs.renameSync(path.join(dir,'data',name),path.join(dir,'data',name.toLowerCase()));
 fs.writeFileSync(path.join(dir,'data','level1.phd'),Buffer.alloc(32));
 const large=Buffer.alloc(6*1024*1024+1);large.writeUInt32LE(0x20);fs.writeFileSync(path.join(dir,'data','level3a.phd'),large);
 const b=await runGuest(wasm,assets,new Map(),{mediaRoot:dir});b.menu(4);
 assert(b.mediaOpens.filter(n=>n==='data/level2.phd').length>=2,'folder was not detected again after restart');report.lowercase_and_restart_without_regenerating_index=true;
 b.menu(9);const before=b.logs.length;b.press(2);b.press(16);b.tick(40);
 assert(!b.logs.slice(before).some(s=>s.includes('ready level=4')),'oversized level became playable');b.press(32);b.press(32);
 b.menu(9);b.press(1);const beforeCaves=b.logs.length;b.press(16);b.tick(40);assert(!b.logs.slice(beforeCaves).some(s=>s.includes('ready level=2')),'invalid header became playable');b.stop();report.invalid_and_oversized_files_not_enabled=true;
 fs.writeFileSync(out,JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report));
} finally {fs.rmSync(dir,{recursive:true,force:true});}
