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
const testAudio=process.env.PXA_TEST_AUDIO_PCM==='1';
const audioPackets=[];let audioOpens=0,audioRetries=0;
const mainLoop=process.env.PXA_TEST_MAIN_LOOP==='1';
const requireTickStart=process.env.PXA_TEST_REQUIRE_TICK_START==='1';
const nativeEnd=Symbol('native main-loop deadline');
const eventPort=0x200000001n;
let nativeWaits=0,queuingNativeInput=false,currentEvent;
let callbackSequence=0,lastSlowIoCallback=-1,ioBoundaryChecks=0;
let clockPeriod=0,lastTick=0;
const callOps={},ioOps={},pathOps={};
let stopping=false;
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
const systemBars=JSON.parse(process.env.PXA_TEST_SYSTEM_BAR_INSETS??'[0,0,0,0]');
if(!Array.isArray(systemBars)||systemBars.length!==4||systemBars.some((n,i)=>
 !Number.isInteger(n)||n<0||n>(i%2?320:240)))throw Error('invalid system bar insets');
function windowSnapshot(){
 const rev=Buffer.alloc(8);rev.writeBigUInt64LE(1n);
 const dims=Buffer.alloc(8);dims.writeUInt32LE(320);dims.writeUInt32LE(240,4);
 const density=Buffer.alloc(8);density.writeUInt32LE(1);density.writeUInt32LE(1,4);
 const bars=Buffer.alloc(16);[systemBars[3],systemBars[0],systemBars[1],systemBars[2]].forEach((v,i)=>bars.writeUInt32LE(v,i*4));
 return Buffer.concat([result(0),record(1,rev),record(2,dims),record(3,dims),
  record(4,density),record(5,Buffer.alloc(16)),record(6,bars),record(7,Buffer.from([2])),record(8,Buffer.from([1]))]);
}
function pxa_call(route,token,ptr,len){
 if(stopping)throw Error('Core imports during stop are forbidden');
 calls++;const service=route&65535,op=route>>>16;callOps[service+':'+op]=(callOps[service+':'+op]||0)+1;const p=Buffer.from(instance.exports.memory.buffer,ptr,len);let r;
 if(requireIoCoop && service===5 && lastSlowIoCallback>=0){
  if(lastSlowIoCallback===callbackSequence)throw Error('slow I/O followed by FS control without a callback boundary');
  ioBoundaryChecks++;lastSlowIoCallback=-1;
 }
 if(service===19){const s=p.subarray(1).toString();
  if(requireTickStart && s.startsWith('Starting ') && (currentEvent?.service!==4 || currentEvent?.op!==0x8001))
   throw Error('Engine started before a foreground Clock event');
  logs.push(s);console.log(s);if(process.env.PXA_TEST_TRACE_EXIT&&(s.includes("Game exited")||s.includes("Invalid game Free")))console.trace("Engine exit");if(process.env.PXA_TEST_TRACE_ALLOC&&s.startsWith("PXA ALLOC"))console.trace("Allocation caller");return 0;}
 if(service===1&&op===2){const h=p.readBigUInt64LE();const f=files.get(h);if(f?.fd!==undefined)fs.closeSync(f.fd);files.delete(h);return 0;}
 if(service===1&&op===5){if(!mainLoop)return -3;r=result(0,12);r.writeBigUInt64LE(eventPort,4);}
 else if(service===2&&op===2)r=windowSnapshot();
 else if(service===16&&op===1){r=result(0,24);r.writeBigUInt64LE(0x100000001n,4);r.writeUInt32LE(640,12);r.writeUInt32LE(153600,16);r[20]=2;}
 else if(service===16&&op===2)r=result(0);
 else if(service===11) {r=testAudio ? result(0,12) : result(-3);if(testAudio)r.writeBigUInt64LE(0x300000001n,4);}
 else if(service===10&&testAudio){
  if(op===5){
   if(p.length!==16 || p.readBigUInt64LE()!==0x300000001n || p[13] || p[14] || p[15])throw Error('PCM open request');
   const rate=p.readUInt32LE(8),channels=p[12];
   if(rate<8000||rate>48000||![1,2].includes(channels))throw Error('PCM format');
   const h=handle({audio:true,rate,channels,attempt:0});++audioOpens;
   const hbytes=Buffer.alloc(8);hbytes.writeBigUInt64LE(h);
   const ratebytes=Buffer.alloc(4);ratebytes.writeUInt32LE(rate);
   const frame=Buffer.alloc(2);frame.writeUInt16LE(20);
   const sink=Buffer.alloc(4);sink.writeUInt32LE(48000);
   const queue=Buffer.alloc(4);queue.writeUInt32LE(Math.floor(rate/50)*4);
   r=Buffer.concat([result(0),record(3,hbytes),record(4,ratebytes),record(5,Buffer.from([channels])),
     record(6,frame),record(7,sink),record(8,Buffer.from([1])),record(9,queue)]);
  } else if(op===2 || op===4)r=result(0);
  else throw Error('unexpected PCM control '+op);
 }
 else if(service===4 && op===1){if(p.length!==2)return -1;const period=p.readUInt16LE();if(period && (period<16 || period>1000))return -1;clockPeriod=period;return 0;}
 else if(service===4 && (op===4 || op===5)){
  if(process.env.PXA_TEST_CLOCK_TIMING==='0')return -3;
  const minimum=Number(process.env.PXA_TEST_CLOCK_MIN_US||1000);
  if(op===4){if(p.length || !token)return -1;r=result(0,24);[minimum,60000000,1,1000,3].forEach((v,i)=>r.writeUInt32LE(v,4+i*4));}
  else {if(p.length!==4 || !token)return -1;const period=p.readUInt32LE();
   if(process.env.PXA_TEST_CLOCK_REJECT==='1')r=result(-4);
   else if(period && (period<minimum || period>60000000))r=result(-1);
   else {clockPeriod=period/1000;r=result(0,8);r.writeUInt32LE(period,4);}
  }
 }
 else if(service===5){
  if([1,5].includes(op)){const name=records(p).get(1)?.toString();if(name)pathOps[name]=(pathOps[name]||0)+1;}
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
   else if(op===11){if(p.readUInt32LE(8)!==1)throw Error('media write');const name=mediaPath(p,14),st=fs.statSync(name);r=result(0,32);r.writeBigUInt64LE(handle({fd:fs.openSync(name,'r'),pos:0,readonly:true}),4);r[12]=1;r.writeBigUInt64LE(1n,16);r.writeBigUInt64LE(BigInt(st.size),24);}
   else if(op===16){const st=fs.statSync(mediaPath(p,10));r=result(0,20);r[4]=st.isDirectory()?2:1;r.writeBigUInt64LE(BigInt(st.size),12);}
   else throw Error('unimplemented FS op '+op);
  }catch(e){if(!['ENOENT','EEXIST','ENOTDIR','EISDIR'].includes(e.code))throw e;r=result(['ENOENT','ENOTDIR'].includes(e.code)?-5:-2);}
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
function pxa_io(h,op,ptr,len){if(stopping)throw Error('Core I/O during stop is forbidden');ioOps[op]=(ioOps[op]||0)+1;const p=Buffer.from(instance.exports.memory.buffer,ptr,len);
 if(h===eventPort && mainLoop){
  if(op!==0x201 || len<20 || len>4096)return -1;
  const timeout=Number(p.readBigUInt64LE())/1000,until=Date.now()+timeout;
  if(timeout>60000)return -16;
  ++nativeWaits;++callbackSequence;
  for(;;){
   if(Date.now()>=end)throw nativeEnd;
   queuingNativeInput=true;
   try{maintainInputs();}finally{queuingNativeInput=false;}
   if(clockPeriod && Date.now()-lastTick>=clockPeriod){
    const payload=Buffer.alloc(8);payload.writeBigUInt64LE(BigInt(Date.now())*1000n);
    events.push({service:4,op:0x8001,payload});ticks++;lastTick=Date.now();
   }
   if(events.length){const bytes=eventBytes(events[0]);if(bytes.length>len)return -9;
    bytes.copy(p);events.shift();return bytes.length;}
   if(!timeout)return -7;
   if(Date.now()>=until)return -12;
   Atomics.wait(ioDelayWord,0,0,1);
  }
 }
 if(ioDelayMs && files.get(h)?.fd!==undefined){Atomics.wait(ioDelayWord,0,0,ioDelayMs);lastSlowIoCallback=callbackSequence;}
 if(h===0x100000001n){
  if(op===0x100){mapped=ptr;mappedBytes=len/2;return len;}
  if(op===0x101){p.fill(0);p[0]=bufferIndex;bufferIndex^=1;return 4;}
  if(op===0x102){frame=Buffer.from(instance.exports.memory.buffer,mapped+p[0]*mappedBytes,mappedBytes);frame=Buffer.from(frame);frames++;
   firstFrameAt??=Date.now();const second=Math.floor((Date.now()-firstFrameAt)/1000);frameWindows.set(second,(frameWindows.get(second)??0)+1);return 16;}
  throw Error('Unsupported surface I/O '+op);
 }
 const f=files.get(h);if(!f)return -6;
 if(f.audio){
  if(op!==2 || !len || len%(f.channels*2) || len>Math.floor(f.rate/50)*f.channels*2)throw Error('PCM write shape');
  if(f.pending){if(!p.equals(f.pending))throw Error('PCM retry changed bytes');f.pending=null;}
  else if((++f.attempt)%3===1){f.pending=Buffer.from(p);++audioRetries;return -7;}
  audioPackets.push(Buffer.from(p));return len;
 }
 if(op===16 || op===17){
  if(!f.readonly)return -3;
  if(op===17){if(len!==8||!p.readBigUInt64LE())return -2;f.retired=p.readBigUInt64LE()>(f.retired??0n)?p.readBigUInt64LE():f.retired;return 0;}
  if(len<24)return -2;const offset=p.readBigUInt64LE(),cookie=p.readBigUInt64LE(8),count=p.readUInt32LE(16);
  if(!cookie||!count||count>4096||len!==24+count||p.readUInt32LE(20))return -2;
  if(cookie<=(f.retired??0n))return -10;f.retired=cookie;
  if(offset>=BigInt(fs.fstatSync(f.fd).size))return 0;
  return fs.readSync(f.fd,p,24,count,Number(offset));
 }

 if(op===1){const n=fs.readSync(f.fd,p,0,len,f.pos);f.pos+=n;return n;}
 if(op===2){if(f.readonly)return -6;const n=fs.writeSync(f.fd,p,0,len,f.pos);f.pos+=n;return n;}
 return -3;
}
const imports={wasi_snapshot_preview1:wasi.wasiImport,'pxa.core.v2':{pxa_call,pxa_io}};
instance=(await WebAssembly.instantiate(fs.readFileSync(wasm),imports)).instance;
wasi.initialize(instance);
function eventBytes(e){const b=Buffer.alloc(20+e.payload.length);b.writeUInt16LE(e.service);b.writeUInt16LE(e.op,2);b.writeBigUInt64LE(e.token??0n,4);b.writeUInt32LE(e.payload.length,12);e.payload.copy(b,20);return b;}
function event(e){
 if(queuingNativeInput){events.push(e);return;}
 callbackSequence++;const b=eventBytes(e),ptr=instance.exports.malloc(b.length);
 Buffer.from(instance.exports.memory.buffer,ptr,b.length).set(b);
 const previous=currentEvent;currentEvent=e;
 try{const r=instance.exports.pxa_app_on_event(ptr,b.length);if(r<0)throw Error('event rejected '+r);}
 finally{currentEvent=previous;instance.exports.free(ptr);}
}
const rc=instance.exports.pxa_app_start(0,0);if(rc)throw Error('start '+rc);
const start=Date.now(),end=start+Number(seconds)*1000;
let pauseAt=-1,resumeAt=-1,pausedFrames=-1;
if(process.env.PXA_TEST_BACKGROUND){pauseAt=start+1000;resumeAt=start+2000;}
const controls=actions?actions.split(',').map(s=>s.split(':').map(Number)):[];
const pointers=JSON.parse(process.env.PXA_TEST_POINTERS??'[]');
function controller(buttons,connected=1,id=0){const payload=Buffer.alloc(32);payload.writeUInt32LE(1);payload.writeUInt32LE(1,4);payload.writeUInt32LE(1,8);payload.writeUInt16LE(10,12);payload.writeBigUInt64LE(BigInt(Date.now())*1000n,16);payload[24]=id;payload[25]=connected;payload.writeUInt32LE(buttons,28);event({service:3,op:0x8001,payload});}
function pointer(phase,x,y){const payload=Buffer.alloc(36);payload.writeUInt32LE(1);payload.writeUInt32LE(1,4);payload.writeUInt32LE(1,8);payload.writeUInt16LE(7,12);payload.writeBigUInt64LE(BigInt(Date.now())*1000n,16);payload[25]=phase;payload.writeUInt16LE(1,26);payload.writeInt32LE(x,28);payload.writeInt32LE(y,32);event({service:3,op:0x8001,payload});}
function maintainInputs(){
 if(pauseAt>0 && Date.now()>=pauseAt){event({service:17,op:0x8005,payload:Buffer.from([0])});pausedFrames=frames;pauseAt=-1;}
 if(resumeAt>0 && Date.now()>=resumeAt){if(frames!==pausedFrames)throw Error('frames advanced in background');event({service:17,op:0x8005,payload:Buffer.from([1])});resumeAt=-1;logs.push('BACKGROUND RESUME PASS');}

 while(controls.length&&(Date.now()-start)/1000>=controls[0][0])controller(...controls.shift().slice(1));
 while(pointers.length&&(Date.now()-start)/1000>=pointers[0][0])pointer(...pointers.shift().slice(1));
}
try{while(Date.now()<end){
 maintainInputs();
 for(let i=0;i<1000&&events.length;i++)event(events.shift());
 if(clockPeriod && Date.now()-lastTick>=clockPeriod){const payload=Buffer.alloc(8);payload.writeBigUInt64LE(BigInt(Date.now())*1000n);event({service:4,op:0x8001,payload});ticks++;lastTick=Date.now();}
 await new Promise(r=>setTimeout(r,1));
}}catch(error){if(error!==nativeEnd)throw error;}
if(requireIoCoop && !ioBoundaryChecks)throw Error('no slow I/O boundary checked');
if(frame){const rgb=Buffer.alloc(320*240*3);for(let i=0;i<320*240;i++){const c=frame.readUInt16LE(i*2);rgb[i*3]=(c>>11)*255/31;rgb[i*3+1]=((c>>5)&63)*255/63;rgb[i*3+2]=(c&31)*255/31;}fs.writeFileSync(output+'.ppm',Buffer.concat([Buffer.from('P6\n320 240\n255\n'),rgb]));}
// Host revocation precedes the stop callback, where Core imports are forbidden.
for(const f of files.values())if(f.fd!==undefined)fs.closeSync(f.fd);
files.clear();clockPeriod=0;stopping=true;
instance.exports.pxa_app_stop(0);
const report={audioOpens,audioRetries,audioBytes:audioPackets.reduce((n,p)=>n+p.length,0),frames,ticks,calls,callOps,ioOps,pathOps,mainLoop,requireTickStart,nativeWaits,bytes:instance.exports.memory.buffer.byteLength,
 dataEnd:instance.exports.__data_end?.value,heapBase:instance.exports.__heap_base?.value,
 elapsedMs:Date.now()-start,firstFrameMs:firstFrameAt-start,frameWindows:Object.fromEntries(frameWindows),ioBoundaryChecks,
 scope:'Node protocol host, not device FPS',logs};fs.writeFileSync(output+'.json',JSON.stringify(report,null,2));if(!frames)throw Error('No frame submitted');
if(testAudio){if(!audioOpens||!audioRetries||!report.audioBytes)throw Error('PCM path not exercised');fs.writeFileSync(output+'.pcm',Buffer.concat(audioPackets));}
console.log(JSON.stringify({frames,ticks,calls,bytes:report.bytes}));
