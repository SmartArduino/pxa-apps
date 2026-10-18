/* Protocol test host for cooperative engine execution. This does not replace
 * the PXA simulator/device acceptance tests. Run with Node 22+. */
import fs from 'node:fs';
import path from 'node:path';
import {WASI} from 'node:wasi';
const [wasm, assets, state, output, seconds='5', actions=''] = process.argv.slice(2);
if(!output)throw Error('usage: node host.mjs main.wasm ASSETS_DIR STATE_DIR OUTPUT_PREFIX [SECONDS]');
fs.mkdirSync(state,{recursive:true});
fs.mkdirSync(path.dirname(output),{recursive:true});
const wasi=new WASI({version:'preview1',args:[],env:{},preopens:{},returnOnExit:true});
const ioDelayMs=Number(process.env.PXA_TEST_IO_DELAY_MS || 0);
if(!Number.isFinite(ioDelayMs)||ioDelayMs<0||ioDelayMs>1000)throw Error('invalid test I/O delay');
const ioDelayWord=new Int32Array(new SharedArrayBuffer(4));
const requireIoCoop=process.env.PXA_TEST_REQUIRE_IO_COOP==='1';
let callbackSequence=0,lastSlowIoCallback=-1,ioBoundaryChecks=0;
let instance, frame, frames=0, seq=100, ticks=0, calls=0, mapped=0, mappedBytes=0, bufferIndex=0, firstFrameAt;
const frameWindows=new Map();
const files=new Map(), events=[],logs=[];
const mediaRoot=process.env.PXA_TEST_APP_MEDIA;
function result(status,n=4){const b=Buffer.alloc(n);b.writeInt32LE(status);return b;}
function record(tag,payload){const b=Buffer.alloc(4+payload.length);b.writeUInt16LE(tag);b.writeUInt16LE(payload.length,2);payload.copy(b,4);return b;}
function records(p){const m=new Map();for(let i=0;i+4<=p.length;){const tag=p.readUInt16LE(i),n=p.readUInt16LE(i+2);m.set(tag,p.subarray(i+4,i+4+n));i+=4+n;}return m;}
function asset(p){const len=p.readUInt16LE(0);const name=p.subarray(p.length-len).toString();if(!name.startsWith('assets/'))throw Error('asset path');return path.join(assets,name.slice(7));}
function hostPath(p){if(p.startsWith('/')||p.split('/').includes('..'))throw Error('private path');return path.join(state,p);}
function mediaPath(p,offset){if(!mediaRoot || !files.get(p.readBigUInt64LE())?.media)throw Error('media handle');const name=p.subarray(offset).toString();if(!name||name.startsWith('/')||name.split('/').some(s=>!s||s==='.'||s==='..'))throw Error('media path');return path.join(mediaRoot,name);}
function handle(resource){const h=0x100000000n+BigInt(++seq);files.set(h,resource);return h;}
function pxa_call(route,token,ptr,len){
 calls++;const service=route&65535,op=route>>>16,p=Buffer.from(instance.exports.memory.buffer,ptr,len);let r;
 if(requireIoCoop && service===5 && lastSlowIoCallback>=0){
  if(lastSlowIoCallback===callbackSequence)throw Error('slow I/O followed by FS control without a callback boundary');
  ioBoundaryChecks++;lastSlowIoCallback=-1;
 }
 if(service===19){const s=p.subarray(1).toString();logs.push(s);console.log(s);if(process.env.PXA_TEST_TRACE_EXIT&&(s.includes("Game exited")||s.includes("Invalid game Free")))console.trace("Engine exit");return 0;}
 if(service===1&&op===2){const h=p.readBigUInt64LE();const f=files.get(h);if(f?.fd!==undefined)fs.closeSync(f.fd);files.delete(h);return 0;}
 if(service===16&&op===1){r=result(0,24);r.writeBigUInt64LE(0x100000001n,4);r.writeUInt32LE(640,12);r.writeUInt32LE(153600,16);r[20]=2;}
 else if(service===16&&op===2)r=result(0);
 else if(service===11)r=result(-3);
 else if(service===5){
  try{
   if(op===1){const m=records(p),name=hostPath(m.get(1).toString()),flags=m.get(3).readUInt32LE();let f;
    if(flags&64)f={entries:fs.readdirSync(name,{withFileTypes:true}),at:0,name};else{
     let fl=(flags&2)?((flags&16)?'w+':(flags&32)?'a+':'r+'):'r';
     if((flags&4)&&!fs.existsSync(name))fs.writeFileSync(name,Buffer.alloc(0));
     f={fd:fs.openSync(name,fl),pos:0};}
    r=result(0,12);r.writeBigUInt64LE(handle(f),4);
   }else if(op===5){const name=hostPath(records(p).get(1).toString()),st=fs.statSync(name);r=result(0,13);r[4]=st.isDirectory()?2:1;r.writeBigUInt64LE(BigInt(st.size),5);}
   else if(op===2){fs.mkdirSync(hostPath(records(p).get(1).toString()));r=result(0);}
   else if(op===3){fs.rmSync(hostPath(records(p).get(1).toString()));r=result(0);}
   else if(op===8){const m=records(p);fs.renameSync(hostPath(m.get(1).toString()),hostPath(m.get(2).toString()));r=result(0);}
   else if(op===6){const f=files.get(p.readBigUInt64LE()),offset=Number(p.readBigInt64LE(8)),origin=p[16];f.pos=(origin===0?0:origin===1?f.pos:fs.fstatSync(f.fd).size)+offset;r=result(0,12);r.writeBigUInt64LE(BigInt(f.pos),4);}
   else if(op===7){const f=files.get(p.readBigUInt64LE()),entry=f.entries[f.at++];
    if(!entry){r=result(0,8);r.writeUInt16LE(7,4);}else{
     const size=Buffer.alloc(8);size.writeBigUInt64LE(BigInt(fs.statSync(path.join(f.name,entry.name)).size));
     r=Buffer.concat([result(0),record(4,Buffer.from(entry.name)),record(5,Buffer.from([entry.isDirectory()?2:1])),record(6,size)]);
    }
   }
   else if(op===9){if(!mediaRoot || p.length!==28 || p[0]!==5)throw Error('app media configuration');r=result(0,16);r.writeBigUInt64LE(handle({media:true}),4);r.writeUInt32LE(3,12);}
   else if(op===11){if(p.readUInt32LE(8)!==1)throw Error('media write');const name=mediaPath(p,14),st=fs.statSync(name);r=result(0,32);r.writeBigUInt64LE(handle({fd:fs.openSync(name,'r'),pos:0,readonly:true}),4);r[12]=1;r.writeBigUInt64LE(BigInt(st.size),24);}
   else if(op===16){const st=fs.statSync(mediaPath(p,10));r=result(0,20);r[4]=st.isDirectory()?2:1;r.writeBigUInt64LE(BigInt(st.size),12);}
   else throw Error('unimplemented FS op '+op);
  }catch(e){if(!['ENOENT','EEXIST','ENOTDIR','EISDIR'].includes(e.code))throw e;r=result(-6);}
 }else if(service===21){try{
    const name=asset(p),st=fs.statSync(name);if(!st.isFile())throw Object.assign(new Error('not an asset'),{code:'ENOENT'});
    if(op===1){r=result(0,24);r[4]=5;r[5]=0;r.writeUInt32LE(st.size,12);r.writeUInt32LE(st.size,16);}
    else if(op===5){const offset=p.readUInt32LE(4),count=Math.min(p.readUInt32LE(8),st.size-offset);r=result(0,12+count);r.writeUInt32LE(offset,4);r.writeUInt32LE(st.size,8);const fd=fs.openSync(name,'r');fs.readSync(fd,r,12,count,offset);fs.closeSync(fd);}
    else throw Error('asset op '+op);
  }catch(e){if(e.code!=='ENOENT')throw e;r=result(-6);}
 }
 if(r&&token)events.push({service,op,token,payload:Buffer.from(r)});
 return 0;
}
function pxa_io(h,op,ptr,len){const p=Buffer.from(instance.exports.memory.buffer,ptr,len);
 if(ioDelayMs && files.get(h)?.fd!==undefined){Atomics.wait(ioDelayWord,0,0,ioDelayMs);lastSlowIoCallback=callbackSequence;}
 if(h===0x100000001n){
  if(op===0x100){mapped=ptr;mappedBytes=len/2;return len;}
  if(op===0x101){p.fill(0);p[0]=bufferIndex;bufferIndex^=1;return 4;}
  if(op===0x102){frame=Buffer.from(instance.exports.memory.buffer,mapped+p[0]*mappedBytes,mappedBytes);frame=Buffer.from(frame);frames++;
   firstFrameAt??=Date.now();const second=Math.floor((Date.now()-firstFrameAt)/1000);frameWindows.set(second,(frameWindows.get(second)??0)+1);return 16;}
  throw Error('Unsupported surface I/O '+op);
 }
 const f=files.get(h);if(!f)return -6;
 if(op===1){const n=fs.readSync(f.fd,p,0,len,f.pos);f.pos+=n;return n;}
 if(op===2){if(f.readonly)return -6;const n=fs.writeSync(f.fd,p,0,len,f.pos);f.pos+=n;return n;}
 return -3;
}
const imports={wasi_snapshot_preview1:wasi.wasiImport,'pxa.core.v2':{pxa_call,pxa_io}};
instance=(await WebAssembly.instantiate(fs.readFileSync(wasm),imports)).instance;
wasi.initialize(instance);
function event(e){callbackSequence++;const b=Buffer.alloc(20+e.payload.length);b.writeUInt16LE(e.service);b.writeUInt16LE(e.op,2);b.writeBigUInt64LE(e.token??0n,4);b.writeUInt32LE(e.payload.length,12);e.payload.copy(b,20);const ptr=instance.exports.malloc(b.length);Buffer.from(instance.exports.memory.buffer,ptr,b.length).set(b);const r=instance.exports.pxa_app_on_event(ptr,b.length);instance.exports.free(ptr);if(r<0)throw Error('event rejected '+r);}
const rc=instance.exports.pxa_app_start(0,0);if(rc)throw Error('start '+rc);
const start=Date.now(),end=start+Number(seconds)*1000;
let pauseAt=-1,resumeAt=-1,pausedFrames=-1;
if(process.env.PXA_TEST_BACKGROUND){pauseAt=start+1000;resumeAt=start+2000;}
const controls=actions?actions.split(',').map(s=>s.split(':').map(Number)):[];
const pointers=JSON.parse(process.env.PXA_TEST_POINTERS??'[]');
function controller(buttons){const payload=Buffer.alloc(32);payload.writeUInt32LE(1);payload.writeUInt32LE(1,4);payload.writeUInt32LE(1,8);payload.writeUInt16LE(10,12);payload.writeBigUInt64LE(BigInt(Date.now())*1000n,16);payload[25]=1;payload.writeUInt32LE(buttons,28);event({service:3,op:0x8001,payload});}
function pointer(phase,x,y){const payload=Buffer.alloc(36);payload.writeUInt32LE(1);payload.writeUInt32LE(1,4);payload.writeUInt32LE(1,8);payload.writeUInt16LE(7,12);payload.writeBigUInt64LE(BigInt(Date.now())*1000n,16);payload[25]=phase;payload.writeUInt16LE(1,26);payload.writeInt32LE(x,28);payload.writeInt32LE(y,32);event({service:3,op:0x8001,payload});}
while(Date.now()<end){
 if(pauseAt>0 && Date.now()>=pauseAt){event({service:17,op:0x8005,payload:Buffer.from([0])});pausedFrames=frames;pauseAt=-1;}
 if(resumeAt>0 && Date.now()>=resumeAt){if(frames!==pausedFrames)throw Error('frames advanced in background');event({service:17,op:0x8005,payload:Buffer.from([1])});resumeAt=-1;logs.push('BACKGROUND RESUME PASS');}

 while(controls.length&&(Date.now()-start)/1000>=controls[0][0])controller(controls.shift()[1]);
 while(pointers.length&&(Date.now()-start)/1000>=pointers[0][0])pointer(...pointers.shift().slice(1));
 for(let i=0;i<1000&&events.length;i++)event(events.shift());
 const payload=Buffer.alloc(8);payload.writeBigUInt64LE(BigInt(Date.now())*1000n);event({service:4,op:0x8001,payload});ticks++;
 await new Promise(r=>setTimeout(r,1));
}
if(requireIoCoop && !ioBoundaryChecks)throw Error('no slow I/O boundary checked');
if(frame){const rgb=Buffer.alloc(320*240*3);for(let i=0;i<320*240;i++){const c=frame.readUInt16LE(i*2);rgb[i*3]=(c>>11)*255/31;rgb[i*3+1]=((c>>5)&63)*255/63;rgb[i*3+2]=(c&31)*255/31;}fs.writeFileSync(output+'.ppm',Buffer.concat([Buffer.from('P6\n320 240\n255\n'),rgb]));}
instance.exports.pxa_app_stop(0);
const report={frames,ticks,calls,bytes:instance.exports.memory.buffer.byteLength,
 dataEnd:instance.exports.__data_end?.value,heapBase:instance.exports.__heap_base?.value,
 elapsedMs:Date.now()-start,firstFrameMs:firstFrameAt-start,frameWindows:Object.fromEntries(frameWindows),ioBoundaryChecks,
 scope:'Node protocol host, not device FPS',logs};fs.writeFileSync(output+'.json',JSON.stringify(report,null,2));if(!frames)throw Error('No frame submitted');
console.log(JSON.stringify({frames,ticks,calls,bytes:report.bytes}));
