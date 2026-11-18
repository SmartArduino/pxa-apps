# SPDX-License-Identifier: MIT
"""Bounded literal/back-reference stream shared by build-time art encoders."""
def pack(data):
    data=bytes(data)
    out=bytearray();literal=bytearray();seen={};at=0
    def flush():
        if literal:out.append(len(literal)-1);out.extend(literal);literal.clear()
    while at<len(data):
        key=data[at:at+4];previous=seen.get(key) if len(key)==4 else None;seen[key]=at
        count=0
        if previous is not None and at-previous<=65535:
            while count<131 and at+count<len(data) and data[previous+count]==data[at+count]:count+=1
        if count>=4:
            flush();distance=at-previous;out.extend((128+count-4,distance&255,distance>>8))
            for pos in range(at+1,at+count):
                if pos+4<=len(data):seen[data[pos:pos+4]]=pos
            at+=count
        else:
            literal.append(data[at]);at+=1
            if len(literal)==128:flush()
    flush();return bytes(out)

def unpack(data,size):
    out=bytearray();at=0
    while at<len(data):
        tag=data[at];at+=1
        if tag&128:
            if at+2>len(data):raise ValueError('truncated distance')
            distance=data[at]+data[at+1]*256;at+=2;count=(tag&127)+4
            if not distance or distance>len(out):raise ValueError('invalid distance')
            if len(out)+count>size:raise ValueError('output overflow')
            for _ in range(count):out.append(out[-distance])
        else:
            count=tag+1
            if at+count>len(data) or len(out)+count>size:raise ValueError('truncated literal')
            out.extend(data[at:at+count]);at+=count
    if len(out)!=size:raise ValueError('output size mismatch')
    return bytes(out)
