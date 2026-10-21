// Minimal deterministic Core 2/WASI Host used by integration and pixel checks.
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import {WASI} from 'node:wasi';
function records(p) {
    const out=new Map();for(let i=0;i<p.length;) {let tag=p.readUInt16LE(i),n=p.readUInt16LE(i+2);out.set(tag,p.subarray(i+4,i+4+n));i+=4+n;}return out;
}
function rec(tag,b) {const r=Buffer.alloc(b.length+4);r.writeUInt16LE(tag);r.writeUInt16LE(b.length,2);b.copy(r,4);return r;}
function u64(n) {let b=Buffer.alloc(8);b.writeBigUInt64LE(BigInt(n));return b;}
function u32(n) {let b=Buffer.alloc(4);b.writeUInt32LE(n);return b;}
export function mediaDirectory(root) {
    const external=path.join(root,'files');
    return fs.existsSync(external)?external:path.join(root,'game');
}
export async function runGuest(wasm,assets,files=new Map(),options={}) {
    options={mediaRoot:mediaDirectory(assets),...options};
    const wasi=new WASI({version:'preview1',args:[],env:{},preopens:{},returnOnExit:true});
    let instance,now=1000000n,eventPtr,mapped,bufferBytes,index=0,frames=0,nextHandle=10n;
    const mediaOpens=[],events=[],handles=new Map(),assetCache=new Map(),logs=[],pcmHash=crypto.createHash('sha256'),frameHash=crypto.createHash('sha256');
    let clockPeriod=0;const clockEvents=[];
    let lastFrame,blockedMediaReads=0;
    let submitted=0,accepted=0,queued=0,audioTime=now,nonzero=0,blocks=0,denied=false;
    const memory=(ptr,n)=>Buffer.from(instance.exports.memory.buffer,ptr,n);
    const status=(s,n=4)=>{const b=Buffer.alloc(n);b.writeInt32LE(s);return b;};
    function advanceAudio() {const n=Math.floor(Number(now-audioTime)*16000/1e6);accepted+=Math.min(queued,n);queued=Math.max(0,queued-n);audioTime=now;}
    function call(route,token,ptr,len) {
        const service=route&65535,op=route>>>16,p=memory(ptr,len);let r;
        if(service===1 && op===2)handles.delete(p.readBigUInt64LE());
        else if(service===19)logs.push(p.subarray(1).toString());
        else if(service===4) {
            if(op===1) {clockPeriod=p.readUInt16LE()*1000;clockEvents.push({op:'legacy',period_us:clockPeriod});}
            else if(op===4) {
                if(options.legacyClock)r=status(-3);
                else {r=status(0,24);r.writeUInt32LE(1000,4);r.writeUInt32LE(60000000,8);r.writeUInt32LE(1,12);r.writeUInt32LE(1,16);r.writeUInt32LE(3,20);}
            } else if(op===5) {
                if(options.failFineClock)r=status(-3);
                else {clockPeriod=p.readUInt32LE();r=status(0,8);r.writeUInt32LE(clockPeriod,4);clockEvents.push({op:'configure',period_us:clockPeriod});}
            }
        }
        else if(service===16 && op===1) {r=status(0,24);r.writeBigUInt64LE(0x100000001n,4);r.writeUInt32LE(640,12);r.writeUInt32LE(153600,16);r[20]=2;}
        else if(service===16 && op===2)r=status(0);
        else if(service===5) {
            if(op===2)r=status(0);
            else if(op===1) {
                const a=records(p),name=a.get(1).toString(),flags=a.get(3).readUInt32LE();
                if(flags&2) {files.set(name,Buffer.alloc(0));}
                if(!files.has(name))r=status(-6);
                else {const handle=0x200000000n+nextHandle++;handles.set(handle,{name,pos:0,write:!!(flags&2)});r=status(0,12);r.writeBigUInt64LE(handle,4);}
            } else if(op===9 && options.mediaRoot) {
                r=status(0,16);r.writeBigUInt64LE(0x500000001n,4);r.writeUInt32LE(3,12);
            } else if(op===11 && options.mediaRoot) {
                const name=p.subarray(14,14+p.readUInt16LE(12)).toString();
                assert(!name.includes('..') && !path.isAbsolute(name));
                mediaOpens.push(name);
                const file=path.join(options.mediaRoot,name);
                if(!fs.existsSync(file))r=status(-6);
                else {const data=fs.readFileSync(file),handle=0x200000000n+nextHandle++;handles.set(handle,{name,pos:0,media:data});r=status(0,32);r.writeBigUInt64LE(handle,4);r[12]=1;r.writeBigUInt64LE(BigInt(data.length),24);}
            } else if(op===6 && handles.has(p.readBigUInt64LE())) {
                const f=handles.get(p.readBigUInt64LE()),offset=Number(p.readBigInt64LE(8));
                assert.equal(p[16],0);assert(offset>=0);f.pos=offset;
                r=status(0,12);r.writeBigUInt64LE(BigInt(offset),4);
            } else if(op===8) {
                const a=records(p),src=a.get(1).toString(),dst=a.get(2).toString();
                if(options.failReplace)r=status(-11);
                else {files.set(dst,files.get(src));files.delete(src);r=status(0);}
            } else r=status(-3);
        } else if(service===21 && op===5) {
            if(!options.allowLegacyAssets)throw Error('Game requested package media instead of scoped SD storage');
            const n=p.readUInt16LE(),name=p.subarray(12,12+n).toString();
            assert(name.startsWith('assets/game/'));
            const file=path.join(assets,name.slice(7));
            if(!fs.existsSync(file))r=status(-6);
            else {
                if(!assetCache.has(file))assetCache.set(file,fs.readFileSync(file));
                const data=assetCache.get(file),offset=p.readUInt32LE(4);
                const limit=(token>>48n)===0x4d55n && options.musicChunk?options.musicChunk:0xffffffff;
                const count=Math.min(p.readUInt32LE(8),data.length-offset,limit);
                assert(count>=0);r=status(0,12+count);r.writeUInt32LE(offset,4);r.writeUInt32LE(data.length,8);data.copy(r,12,offset,offset+count);
            }
        } else if(service===11 && op===2) {
            denied=!!options.denyAudio;r=status(denied?-4:0,denied?4:12);if(!denied)r.writeBigUInt64LE(0x300000001n,4);
        } else if(service===10) {
            if(op===1)r=Buffer.concat([status(0),rec(3,u64(0x400000001n)),rec(4,u32(16000)),rec(5,Buffer.from([1])),rec(6,Buffer.from([20,0]))]);
            else if(op===3) {advanceAudio();r=Buffer.concat([status(0),rec(2,u64(submitted)),rec(3,u64(accepted)),rec(4,u32(queued)),rec(5,u32(0))]);}
            else {if(op===4)queued=0;r=status(0);}
        }
        if(r && token)events.push({service,op,token,payload:r});return 0;
    }
    function io(h,op,ptr,len) {
        const p=memory(ptr,len);
        if(h===0x100000001n) {
            if(op===0x100) {mapped=ptr;bufferBytes=len/2;return len;}
            if(op===0x101) {p.fill(0);p[0]=index;index^=1;return 4;}
            if(op===0x102) {lastFrame=Buffer.from(memory(mapped+p[0]*bufferBytes,bufferBytes));frameHash.update(lastFrame);++frames;clockEvents.push({op:'present',period_us:clockPeriod});now+=BigInt(options.presentationCostUs??0);return 16;}
        } else if(h===0x400000001n) {
            advanceAudio();if(queued+len/2>1280) {++blocks;return -7;}
            assert.equal(len,640);queued+=len/2;submitted+=len/2;
            pcmHash.update(p);for(let i=0;i<len;i+=2)if(p.readInt16LE(i))++nonzero;return len;
        } else if(handles.has(h)) {
            const f=handles.get(h);
            if(op===1) {if(f.media && options.mediaWouldBlock && f.pos>0 && ((f.reads=(f.reads??0)+1)%7===0)) {++blockedMediaReads;return -7;}const data=f.media??files.get(f.name),n=Math.min(len,data.length-f.pos,f.media?(f.name==='tracks.ad4'?(options.musicChunk??options.ioChunk??0xffffffff):(options.ioChunk??0xffffffff)):0xffffffff);data.copy(p,0,f.pos,f.pos+n);f.pos+=n;return n;}
            if(op===2) {const n=Math.min(len,options.partialWrite?111:len);files.set(f.name,Buffer.concat([files.get(f.name),p.subarray(0,n)]));f.pos+=n;return n;}
        }
        throw Error('unknown I/O '+h+' '+op);
    }
    instance=(await WebAssembly.instantiate(fs.readFileSync(wasm),{wasi_snapshot_preview1:{...wasi.wasiImport,clock_time_get:(_c,_p,ptr)=>{memory(ptr,8).writeBigUInt64LE(now*1000n);return 0;}},'pxa.core.v2':{pxa_call:call,pxa_io:io}})).instance;
    wasi.initialize(instance);eventPtr=instance.exports.malloc?instance.exports.malloc(8192):instance.exports.__heap_base.value;
    assert(eventPtr,'event scratch allocation failed');
    const needed=eventPtr+8192-instance.exports.memory.buffer.byteLength;if(needed>0)instance.exports.memory.grow(Math.ceil(needed/65536));
    function event({service,op,token=0n,payload}) {
        const b=Buffer.alloc(20+payload.length);b.writeUInt16LE(service);b.writeUInt16LE(op,2);b.writeBigUInt64LE(token,4);b.writeUInt32LE(payload.length,12);payload.copy(b,20);memory(eventPtr,b.length).set(b);
        assert(instance.exports.pxa_app_on_event(eventPtr,b.length)>=0);
    }
    function drain() {let n=0;while(events.length) {assert(++n<5000);event(events.shift());}}
    function tick(n=1,stepUs=33334) {while(n--) {now+=BigInt(stepUs);if(clockPeriod)event({service:4,op:0x8001,payload:u64(now)});drain();}}
    function release(stepUs=20000) {now+=BigInt(stepUs);const p=Buffer.alloc(20);p.writeBigUInt64LE(0x100000001n);p.writeBigUInt64LE(BigInt(frames),12);event({service:16,op:0x8001,payload:p});drain();}
    function buttons(buttons,connected=true) {const p=Buffer.alloc(32);p.writeUInt32LE(1);p.writeUInt32LE(1,4);p.writeUInt32LE(1,8);p.writeUInt16LE(10,12);p.writeBigUInt64LE(now,16);p[25]=connected?1:0;p.writeUInt32LE(buttons,28);event({service:3,op:0x8001,payload:p});drain();}
    function pointer(x,y,phase) {const p=Buffer.alloc(36);p.writeUInt32LE(1);p.writeUInt32LE(1,4);p.writeUInt32LE(1,8);p.writeUInt16LE(7,12);p.writeBigUInt64LE(now,16);p[25]=phase;p.writeInt32LE(x,28);p.writeInt32LE(y,32);event({service:3,op:0x8001,payload:p});drain();}
    function focus(foreground) {event({service:17,op:0x8005,payload:Buffer.from([foreground?1:0])});drain();}
    function press(mask) {buttons(mask);tick();buttons(0);tick();}
    function menu(selection) {
        press(128);for(let i=0;i<selection;++i)press(2);press(16);tick(100);
        // SD loading cooperates after each bounded read; await actual completion,
        // independent of the old package asset callback's chunk size.
        const loading=()=>[...handles.values()].some(f=>f.media && /\.(phd|pkd)$/.test(f.name) && f.pos<f.media.length);
        for(let n=0;n<20000 && loading();++n)tick();
        assert(!loading(),'level read did not complete');
    }
    assert.equal(instance.exports.pxa_app_start(0,0),0);drain();tick(10);
    for(let i=0;i<4000 && !logs.some(s=>s.includes('ready level=0'));++i)tick();
    if(!options.allowLoadFailure)assert(logs.some(s=>s.includes('ready level=0')),'title failed to load: '+logs.join('\n'));
    function waitFor(text,limit=10000) {for(let n=0;n<limit;++n) {if(logs.some(s=>s.includes(text)))return;tick();}assert.fail('timed out: '+text);}
    return {tick,release,buttons,pointer,focus,press,menu,waitFor,logs,mediaOpens,clockEvents,frame:()=>Buffer.from(lastFrame),stop:()=>instance.exports.pxa_app_stop(0),info:()=>({blockedMediaReads,frames,submitted,accepted,nonzero,blocks,denied,frame_sha256:frameHash.copy().digest('hex'),pcm_sha256:pcmHash.copy().digest('hex'),linear_bytes:instance.exports.memory.buffer.byteLength,clock_period_us:clockPeriod})};
}
