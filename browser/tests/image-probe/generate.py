#!/usr/bin/env python3
"""Generate original diagnostic JPEGs. No downloaded artwork is bundled."""
from io import BytesIO
from pathlib import Path
import hashlib, json, struct
from PIL import Image

out=Path(__file__).resolve().parent/'assets';out.mkdir(exist_ok=True)
rows=[]
for name,width,height,padded in [('common',600,1200,True),('large',1199,2533,False)]:
    image=Image.new('RGB',(width,height))
    image.putdata([(25+x*190//width,20+y*210//height,205-y*130//height) for y in range(height) for x in range(width)])
    encoded=BytesIO();image.save(encoded,format='JPEG',quality=88);data=encoded.getvalue()
    # Stay below 256 KiB: the old geometric buffer otherwise exceeds the
    # default 512 KiB temporary budget while retaining old + new storage.
    if padded:data=data[:2]+(b'\xff\xe1'+struct.pack('>H',50002)+b'\0'*50000)*4+data[2:]
    (out/(name+'.bin')).write_bytes(data)
    rows.append(dict(name=name,width=width,height=height,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
(out/'manifest.json').write_text(json.dumps(rows,indent=2)+'\n')
print(json.dumps(rows,indent=2))
