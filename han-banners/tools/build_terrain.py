#!/usr/bin/env python3
"""Make the campaign relief map from public-domain Natural Earth data."""
import argparse
import io
from pathlib import Path
import struct
import zipfile

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
WIDTH, HEIGHT = 1024, 1024
WEST, NORTH, EAST, SOUTH = 101.5, 41.0, 123.5, 17.2


def project(lon,lat):
    return ((lon-WEST)*WIDTH/(EAST-WEST), (NORTH-lat)*HEIGHT/(NORTH-SOUTH))


def from_zip(path, suffix):
    with zipfile.ZipFile(path) as archive:
        name = next(n for n in archive.namelist() if n.lower().endswith(suffix))
        return archive.read(name)


def rivers(shp):
    """Read only the PolyLine records and their geographic coordinates."""
    offset = 100
    while offset+8 <= len(shp):
        _, words = struct.unpack_from(">2i",shp,offset)
        shape = memoryview(shp)[offset+8:offset+8+words*2]
        offset += 8+words*2
        if len(shape) < 44 or struct.unpack_from("<i",shape)[0]!=3:
            continue
        l,b,r,t,parts,points = struct.unpack_from("<4d2i",shape,4)
        if l>EAST or r<WEST or b>NORTH or t<SOUTH:
            continue
        starts = struct.unpack_from(f"<{parts}i",shape,44)+(points,)
        coords = list(struct.iter_unpack("<dd",shape[44+parts*4:44+parts*4+points*16]))
        for begin,end in zip(starts,starts[1:]):
            if end-begin > 2:
                yield [project(*coord) for coord in coords[begin:end]]


def make(relief,rivers_zip):
    # Natural Earth's global raster is an equally spaced longitude/latitude grid.
    with Image.open(io.BytesIO(from_zip(relief,".tif"))) as world:
        world_width,world_height=world.size
        crop=((WEST+180)*world_width/360,(90-NORTH)*world_height/180,
              (EAST+180)*world_width/360,(90-SOUTH)*world_height/180)
        view=world.resize((WIDTH,HEIGHT),Image.Resampling.LANCZOS,box=crop).convert("RGB")
    pixels=np.asarray(view,dtype=np.float32)/255
    r,g,b=pixels.transpose(2,0,1)
    ocean=np.clip((b-r-.025)*9,0,1)*np.clip((b-g+.035)*8,0,1)
    light=np.clip(.33*r+.51*g+.16*b,0,1)
    warm=np.clip((r-g)*3+.30,0,1)
    noise=np.random.default_rng(208).normal(0,.009,(HEIGHT,WIDTH))
    land=np.dstack((.19+light*.31+warm*.14,.22+light*.37,.14+light*.29-warm*.01))
    deep=np.dstack((.08+light*.11,.18+light*.18,.22+light*.19))
    output=np.clip((land*(1-ocean[...,None])+deep*ocean[...,None]+noise[...,None])*255,0,255)
    plate=Image.fromarray(output.astype("uint8"))
    hydro=Image.new("RGBA",(WIDTH,HEIGHT))
    draw=ImageDraw.Draw(hydro)
    for path in rivers(from_zip(rivers_zip,".shp")):
        if len(path)>25:
            draw.line(path,fill=(105,157,162,200),width=3,joint="curve")
        else:
            draw.line(path,fill=(87,144,147,139),width=2)
    plate=Image.alpha_composite(plate.convert("RGBA"),hydro)
    # A subtle old-map grid stays geographically registered with the relief.
    grid=Image.new("RGBA",(WIDTH,HEIGHT))
    draw=ImageDraw.Draw(grid)
    for lon in range(104,124,4):
        x,_=project(lon,SOUTH)
        draw.line((x,0,x,HEIGHT),fill=(226,204,146,24))
    for lat in range(24,41,4):
        _,y=project(WEST,lat)
        draw.line((0,y,WIDTH,y),fill=(226,204,146,24))
    return Image.alpha_composite(plate,grid).convert("RGB")


if __name__=="__main__":
    args=argparse.ArgumentParser(description=__doc__)
    args.add_argument("relief",type=Path)
    args.add_argument("rivers",type=Path)
    ns=args.parse_args()
    make(ns.relief,ns.rivers).save(ROOT/"art/terrain.png",optimize=True)
