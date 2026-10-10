#!/usr/bin/env python3
"""Rebuild byte-exact PXR resources from the maintained C++ artwork source.

Art/font attribution and GPL-3.0 license are retained in LICENSE and README.md.
No quantization or resampling is performed.
"""
import argparse,re,json
from pathlib import Path

def export(source, output):
    specs=[('assets.c',name,width) for name,width in [
        ('pd_tile_atlas',256),('pd_wall_atlas',256),('pd_sprite_atlas',256),
        ('pd_ui_atlas',256),('pd_title_atlas',256),('pd_fire_atlas',192)]]
    specs += [('font_data.c',name,width) for name,width in [
        ('pd_font_ascii',128),('pd_font_cjk',256),('pd_font_cjk_extra',256)]]
    output.mkdir(parents=True,exist_ok=True)
    declarations=[]
    for file,name,width in specs:
        candidate=source/(file+'pp')
        content=(candidate if candidate.exists() else source/file).read_text()
        body=re.search(r'const uint8_t '+name+r'\[[^;=]+\]\s*=\s*\{([^}]+)\}',content).group(1)
        payload=bytes(int(n,0) for n in re.findall(r'0[xX][0-9a-fA-F]+|\b\d+\b',body))
        assert len(payload)%width==0 and 0<len(payload)//width<=256,(name,len(payload))
        (output/(name+'.index8')).write_bytes(payload)
        declarations.append({'path':'assets/raster/'+name+'.pxr','kind':'index8','source':'resources/'+name+'.index8','width':width,'height':len(payload)//width})
        print(name,width,len(payload)//width,len(payload))
    (output.parent/'resources.json').write_text(json.dumps({'assets':declarations},indent=2)+'\n')
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[1])
    args=parser.parse_args();export(args.source,Path(__file__).resolve().parents[1]/'resources')
