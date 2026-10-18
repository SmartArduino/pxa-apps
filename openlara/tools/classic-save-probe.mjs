// Test-side decoding of the original OLS2 checkpoint, using the source PHD
// entity index. Entity zero can be a trap; it must not stand in for Lara.
import assert from 'node:assert/strict';

export function checkpoint(file,lara) {
    assert.equal(file.subarray(0,4).toString(),'OLPX');
    const data=file.subarray(16);assert.equal(data.subarray(0,4).toString(),'OLS2');
    for(let p=4;p<data.length;) {
        const n=data.readUInt32LE(p);p+=4;const slot=data.subarray(p,p+n);p+=n;
        if(!(slot.readUInt32LE()>>>31))continue;
        const items=slot.readUInt32LE(32),first=36+items*4+292+4,
            entities=slot.readUInt32LE(first-4);
        assert(lara.index<entities,'Lara checkpoint record missing');
        let cursor=first;
        for(let i=0;i<entities;++i) {
            const extra=slot.readUInt16LE(cursor+26),end=cursor+28+extra;
            assert(end<=slot.length,'invalid entity record');
            if(i===lara.index) {
                assert.equal(extra,48,'selected checkpoint entity is not Lara');
                const position=lara.position.map((v,j)=>slot.readInt32LE(cursor+j*4)^v),
                    rotation=slot.readUInt16LE(cursor+12)^lara.rotation,
                    room=slot.readUInt16LE(cursor+24)^lara.room;
                return {slot,pose:slot.subarray(cursor,cursor+14),position,rotation,room,
                    inventory:slot.subarray(36,36+items*4)};
            }
            cursor=end;
        }
    }
    assert.fail('checkpoint missing');
}

export function telemetry(logs) {
    for(let i=logs.length-1;i>=0;--i) {
        const match=/pos=(-?\d+),(-?\d+),(-?\d+) yaw=(-?\d+)/.exec(logs[i]);
        if(match)return match.slice(1).map(Number);
    }
    assert.fail('no player telemetry');
}

export function assertRestored(pose,saved) {
    assert.deepEqual(pose.slice(0,3),saved.position,'Lara position was not restored');
    assert(Math.abs((pose[3]-saved.rotation+32768+65536)%65536-32768)<=1,'Lara rotation was not restored');
}
