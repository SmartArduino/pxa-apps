// SPDX-License-Identifier: MIT
// Bake MIT pixelfx into bounded RGB565 + 4-bit-alpha RLE. No JavaScript runs on
// the device. The vendored upstream modules and their notices stay unmodified.
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';
import {createFrame,clearFrame,createFx,composite} from './pixelfx/src/core/fx.js';
const app=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const review=process.argv.indexOf('--review');const reviewDir=review>=0?path.resolve(process.argv[review+1]):null;
if(reviewDir)fs.mkdirSync(reviewDir,{recursive:true});
const choices=[['impact','impacts/bullet-impact',40,18],['explosion','explosions/small-explosion',64,24],['large','explosions/big-explosion',84,28],['heal','holy/heal-aoe',56,22],['reward','pickups/collect-burst',46,20]];
let all=[],offsets=[],records=[],info=[];
for(const [name,id,width,count] of choices){
 const mod=await import(pathToFileURL(path.join(app,'tools/pixelfx/src/fx',id+'.js')));
 const [sw,sh]=mod.meta.size,scale=width/sw,height=Math.ceil(sh*scale),ox=Math.round(mod.meta.origin[0]*scale),oy=Math.round(mod.meta.origin[1]*scale);
 const f=createFrame(sw,sh),rgba=new Uint8ClampedArray(sw*sh*4),fx=createFx(mod,{x:mod.meta.origin[0],y:mod.meta.origin[1],seed:7,stone:"#626b58",...(name==='reward'?{palette:["#2a180d","#6b4b22","#ae7d31","#d9a847","#f6d273","#fff4c2","#ffffff"]}:{})});
 const first=offsets.length;let previous=0;
 for(let frame=0;frame<count;frame++){
  // The first frame includes the ignition flash; the remaining frames span the
  // complete effect, including cooling smoke and falling debris.
  const target=(name==='impact'||name==='explosion'||name==='large')?(frame<4?[1/120,.03,.065,.11][frame]:.2+(mod.meta.duration-.2)*(frame-4)/(count-5)):Math.max(1/120,mod.meta.duration*frame/(count-1));
  fx.update(target-previous);previous=target;clearFrame(f);fx.draw(f);composite(f,rgba,null,.7);
  const pixels=[];const preview=new Uint8Array(width*height*4);
  for(let y=0;y<height;y++)for(let x=0;x<width;x++){
   let red=0,green=0,blue=0,alpha=0,n=0;
   const x0=Math.floor(x/scale),x1=Math.min(sw,Math.ceil((x+1)/scale)),y0=Math.floor(y/scale),y1=Math.min(sh,Math.ceil((y+1)/scale));
   for(let yy=y0;yy<y1;yy++)for(let xx=x0;xx<x1;xx++){let i=(yy*sw+xx)*4,a=rgba[i+3];red+=rgba[i]*a;green+=rgba[i+1]*a;blue+=rgba[i+2]*a;alpha+=a;n++;}
   let a=n?Math.round(alpha/n/255*16):0;
   if(a<2||!alpha){pixels.push(null);continue;}
   red=Math.round(red/alpha);green=Math.round(green/alpha);blue=Math.round(blue/alpha);
   let color=((red&248)<<8)|((green&252)<<3)|(blue>>3);pixels.push([color,a]);
   let j=(y*width+x)*4;preview[j]=red;preview[j+1]=green;preview[j+2]=blue;preview[j+3]=Math.min(255,a*16);
  }
  offsets.push(all.length);
  for(let at=0;at<pixels.length;){const pixel=pixels[at];let length=1;while(length<128&&at+length<pixels.length&&JSON.stringify(pixels[at+length])===JSON.stringify(pixel))length++;
   if(!pixel)all.push(128|length-1);else all.push(length-1,pixel[0]&255,pixel[0]>>8,pixel[1]);at+=length;
  }
  if(reviewDir&&[0,Math.floor(count/5),Math.floor(count/2),count-2].includes(frame))fs.writeFileSync(path.join(reviewDir,`${name}-${frame}-${width}x${height}.rgba`),preview);
 }
 records.push(`{${width},${height},${ox},${oy},${count},${first}}`);info.push({name,id,width,height,frames:count,sourceDuration:mod.meta.duration});
}
offsets.push(all.length);
let header='// Generated from MIT pixelfx, pinned 5294518ce7c72e48b6100a744596e0acf57f2826.\n// Copyright (c) 2026 Patch, Hugo Duprez. See assets/third-party/pixelfx-LICENSE.\n#pragma once\n#include <array>\n#include <cstdint>\nnamespace iron::vfx {\nstruct Clip{int width,height,ox,oy,frames,first;};\n';
header+='inline constexpr std::array<Clip,5> clips={{'+records.join(',')+'}};\n';
header+=`inline constexpr std::array<uint32_t,${offsets.length}> offsets={`+offsets.join(',')+'};\n';
header+=`inline constexpr std::array<uint8_t,${all.length}> bytes={`+all.join(',')+'};\n}\n';
fs.writeFileSync(path.join(app,'fx_data.hpp'),header);
if(reviewDir)fs.writeFileSync(path.join(reviewDir,'atlas.json'),JSON.stringify(info,null,2));
console.log(JSON.stringify({bytes:all.length,frames:offsets.length-1,effects:info}));
