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
export async function runGuest(wasm,assets,files=new Map(),options={}) {
    const wasi=new WASI({version:'preview1',args:[],env:{},preopens:{},returnOnExit:true});
    let instance,now=1000000n,eventPtr,mapped,bufferBytes,index=0,frames=0,nextHandle=10n;
    const events=[],handles=new Map(),logs=[],pcmHash=crypto.createHash('sha256'),frameHash=crypto.createHash('sha256');
    let lastFrame;
    let submitted=0,accepted=0,queued=0,audioTime=now,nonzero=0,blocks=0,denied=false;
    const memory=(ptr,n)=>Buffer.from(instance.exports.memory.buffer,ptr,n);
    const status=(s,n=4)=>{const b=Buffer.alloc(n);b.writeInt32LE(s);return b;};
    function advanceAudio() {const n=Math.floor(Number(now-audioTime)*16000/1e6);accepted+=Math.min(queued,n);queued=Math.max(0,queued-n);audioTime=now;}
    function call(route,token,ptr,len) {
        const service=route&65535,op=route>>>16,p=memory(ptr,len);let r;
        if(service===19)logs.push(p.subarray(1).toString());
        else if(service===16 && op===1) {r=status(0,24);r.writeBigUInt64LE(0x100000001n,4);r.writeUInt32LE(640,12);r.writeUInt32LE(153600,16);r[20]=2;}
        else if(service===16 && op===2)r=status(0);
        else if(service===5) {
            if(op===2)r=status(0);
            else if(op===1) {
                const a=records(p),name=a.get(1).toString(),flags=a.get(3).readUInt32LE();
                if(flags&2) {files.set(name,Buffer.alloc(0));}
                if(!files.has(name))r=status(-6);
                else {const handle=0x200000000n+nextHandle++;handles.set(handle,{name,pos:0,write:!!(flags&2)});r=status(0,12);r.writeBigUInt64LE(handle,4);}
            } else if(op===8) {
                const a=records(p),src=a.get(1).toString(),dst=a.get(2).toString();
                if(options.failReplace)r=status(-11);
                else {files.set(dst,files.get(src));files.delete(src);r=status(0);}
            } else r=status(-3);
        } else if(service===21 && op===5) {
            const n=p.readUInt16LE(),name=p.subarray(12,12+n).toString();
            assert(name.startsWith('assets/game/'));
            const data=fs.readFileSync(path.join(assets,name.slice(7))),offset=p.readUInt32LE(4),count=Math.min(p.readUInt32LE(8),data.length-offset);
            r=status(0,12+count);r.writeUInt32LE(offset,4);r.writeUInt32LE(data.length,8);data.copy(r,12,offset,offset+count);
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
            if(op===0x102) {lastFrame=Buffer.from(memory(mapped+p[0]*bufferBytes,bufferBytes));frameHash.update(lastFrame);++frames;return 16;}
        } else if(h===0x400000001n) {
            advanceAudio();if(queued+len/2>1280) {++blocks;return -7;}
            assert.equal(len,640);queued+=len/2;submitted+=len/2;
            pcmHash.update(p);for(let i=0;i<len;i+=2)if(p.readInt16LE(i))++nonzero;return len;
        } else if(handles.has(h)) {
            const f=handles.get(h);
            if(op===1) {const data=files.get(f.name),n=Math.min(len,data.length-f.pos);data.copy(p,0,f.pos,f.pos+n);f.pos+=n;return n;}
            if(op===2) {const n=Math.min(len,options.partialWrite?111:len);files.set(f.name,Buffer.concat([files.get(f.name),p.subarray(0,n)]));f.pos+=n;return n;}
        }
        throw Error('unknown I/O '+h+' '+op);
    }
    instance=(await WebAssembly.instantiate(fs.readFileSync(wasm),{wasi_snapshot_preview1:{...wasi.wasiImport,clock_time_get:(_c,_p,ptr)=>{memory(ptr,8).writeBigUInt64LE(now*1000n);return 0;}},'pxa.core.v2':{pxa_call:call,pxa_io:io}})).instance;
    wasi.initialize(instance);eventPtr=instance.exports.__heap_base.value;
    const needed=eventPtr+8192-instance.exports.memory.buffer.byteLength;if(needed>0)instance.exports.memory.grow(Math.ceil(needed/65536));
    function event({service,op,token=0n,payload}) {
        const b=Buffer.alloc(20+payload.length);b.writeUInt16LE(service);b.writeUInt16LE(op,2);b.writeBigUInt64LE(token,4);b.writeUInt32LE(payload.length,12);payload.copy(b,20);memory(eventPtr,b.length).set(b);
        assert(instance.exports.pxa_app_on_event(eventPtr,b.length)>=0);
    }
    function drain() {let n=0;while(events.length) {assert(++n<5000);event(events.shift());}}
    function tick(n=1) {while(n--) {now+=33334n;event({service:4,op:0x8001,payload:u64(now)});drain();}}
    function buttons(buttons,connected=true) {const p=Buffer.alloc(32);p.writeUInt32LE(1);p.writeUInt32LE(1,4);p.writeUInt32LE(1,8);p.writeUInt16LE(10,12);p.writeBigUInt64LE(now,16);p[25]=connected?1:0;p.writeUInt32LE(buttons,28);event({service:3,op:0x8001,payload:p});drain();}
    function pointer(x,y,phase) {const p=Buffer.alloc(36);p.writeUInt32LE(1);p.writeUInt32LE(1,4);p.writeUInt32LE(1,8);p.writeUInt16LE(7,12);p.writeBigUInt64LE(now,16);p[25]=phase;p.writeInt32LE(x,28);p.writeInt32LE(y,32);event({service:3,op:0x8001,payload:p});drain();}
    function focus(foreground) {event({service:17,op:0x8005,payload:Buffer.from([foreground?1:0])});drain();}
    function press(mask) {buttons(mask);tick();buttons(0);tick();}
    function menu(selection) {press(128);for(let i=0;i<selection;++i)press(2);press(16);tick(100);}
    assert.equal(instance.exports.pxa_app_start(0,0),0);drain();tick(10);
    assert(logs.some(s=>s.includes('ready level=0')),'title failed to load');
    return {tick,buttons,pointer,focus,press,menu,logs,frame:()=>Buffer.from(lastFrame),stop:()=>instance.exports.pxa_app_stop(0),info:()=>({frames,submitted,accepted,nonzero,blocks,denied,frame_sha256:frameHash.copy().digest('hex'),pcm_sha256:pcmHash.copy().digest('hex'),linear_bytes:instance.exports.memory.buffer.byteLength})};
}
