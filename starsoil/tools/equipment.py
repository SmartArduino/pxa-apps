"""Native 16px equipment, interpreted from Starsoil's original concept studies.

Handles keep the authored actor grip points. These grids are the production
source; the large concept sheets are never sampled by the game renderer.
"""
from PIL import Image, ImageDraw

INK = '#142836'
CREAM = '#f8edcf'
GOLD = '#d9ad66'


def star(d, x, y, c=GOLD):
    d.polygon([(x,y-3),(x+1,y-1),(x+3,y),(x+1,y+1),
               (x,y+3),(x-1,y+1),(x-3,y),(x-1,y-1)], fill=c)
    d.point((x,y), fill=CREAM)


def icon(kind, name, col):
    im=Image.new('RGBA',(16,16));d=ImageDraw.Draw(im);low=name.lower()
    wood='wood' in low
    crystal='crystal' in low
    iron='iron' in low
    base,light,shadow=('#b68b5a','#efd09a','#71503e') if wood else \
        ('#70d6bd','#e0ffeb','#348b98') if crystal else \
        ('#a6bcc8','#f1f3df','#5e788b') if iron else \
        ('#ce8e60','#f8cf91','#915646')
    if kind in ('Pick','Axe','Hammer','Wand','Sword'):
        if kind=='Sword':
            d.line((3,14,7,9),fill=INK,width=4)
            d.line((3,13,6,10),fill='#aa7953',width=2)
            d.point((4,12),fill='#ebc28a')
            tip=1 if wood else 0
            d.polygon([(5,9),(12,tip),(15,tip),(15,3),(8,12)],fill=INK)
            d.polygon([(6,9),(13,tip+1),(14,tip+1),(13,4),(8,10)],fill=base)
            d.line((7,8,13,tip+2),fill=light)
            d.line((9,8,13,4),fill=shadow)
            d.line((4,8,8,12),fill=INK,width=3)
            d.line((4,8,8,12),fill=GOLD)
            d.point((6,10),fill='#85d8bd' if crystal else CREAM)
            d.point((3,14),fill=GOLD)
        else:
            d.line((3,14,11,3),fill=INK,width=4)
            d.line((4,13,11,4),fill='#a97950',width=2)
            d.line((5,11,9,6),fill='#deb47c')
            d.point((5,11),fill=CREAM)
            d.line((4,13,5,13),fill=shadow)
            if kind=='Pick':
                d.polygon([(3,4),(6,1),(10,0),(13,1),(15,4),(15,7),
                           (12,5),(10,3),(7,3),(4,5)],fill=INK)
                d.line((4,3,7,2,10,1,12,2,14,5),fill=base,width=2)
                d.line((6,2,10,1,12,2),fill=light)
                d.line((12,3,14,5),fill=shadow)
                if crystal:
                    d.polygon([(8,1),(10,0),(12,2),(10,4)],fill=base)
                    d.line((9,1,10,2),fill=light)
                d.rectangle((9,4,10,5),fill=GOLD)
            elif kind=='Axe':
                d.polygon([(8,2),(12,0),(15,2),(15,6),(12,9),
                           (9,7),(9,4),(7,3)],fill=INK)
                d.polygon([(10,2),(12,1),(14,3),(14,6),(12,7),(10,6)],fill=base)
                d.line((14,3,14,5,12,7),fill=light)
                d.line((10,3,11,5),fill=shadow)
                d.point((10,2),fill=GOLD)
            elif kind=='Hammer':
                d.polygon([(5,1),(8,0),(15,4),(15,7),(12,9),(5,5)],fill=INK)
                d.polygon([(6,2),(8,1),(14,5),(12,7),(6,4)],fill=base)
                d.line((7,1,13,4),fill=light)
                d.line((12,7,14,5),fill=shadow)
            else:
                d.polygon([(7,3),(10,0),(14,0),(15,5),(12,9),(8,7)],fill=INK)
                star(d,11,4)
                d.line((8,6,10,8,13,7),fill='#8ccab7')
        return im
    if kind=='Bow':
        d.line((8,1,11,2,14,6,14,9,11,13,8,14),fill=INK,width=3)
        d.line((8,1,11,3,13,6,14,8,12,11,8,14),fill='#bc8d5d',width=2)
        d.line((8,2,7,8,8,13),fill=CREAM)
        d.line((3,8,14,8),fill='#c9d8c6')
        d.polygon([(12,6),(15,8),(12,10)],fill='#b6cfd0')
        d.point((11,3),fill=GOLD);d.point((11,12),fill=GOLD)
        return im
    if kind=='Potion' or name in ('Water flask','Lava flask'):
        d.rectangle((6,0,9,2),fill=INK);d.line((6,1,9,1),fill=GOLD)
        d.rectangle((5,3,10,4),fill=INK)
        d.polygon([(5,4),(10,4),(13,7),(13,12),(11,14),(4,14),
                   (2,12),(2,8)],fill=INK)
        d.polygon([(5,5),(10,5),(12,8),(12,11),(10,13),(5,13),
                   (3,11),(3,8)],fill='#86a4a9')
        d.polygon([(4,8),(11,8),(11,11),(9,12),(5,12),(4,11)],fill='#'+col)
        d.line((4,8,10,8),fill='#e9b7ad' if 'heal' in low else '#b5e8d2')
        d.line((4,6,4,9),fill=CREAM);d.point((5,5),fill=CREAM)
        d.point((8,12),fill='#'+col)
        return im
    if kind=='Boots' and 'cloud' in low:
        for x,y in ((1,2),(8,4)):
            d.polygon([(x,y),(x+4,y),(x+4,y+7),(x+6,y+8),
                       (x+6,y+10),(x,y+10)],fill=INK)
            d.rectangle((x+1,y+2,x+3,y+7),fill='#7696a4')
            d.line((x+1,y+9,x+5,y+9),fill=GOLD)
            d.line((x,y+1,x+4,y+1),fill=CREAM,width=2)
            d.point((x+2,y+2),fill='#b7d4d1')
        return im
    if kind=='Charm' and 'magnet' not in low:
        d.arc((3,0,12,9),165,370,fill=GOLD)
        d.polygon([(8,5),(13,9),(8,15),(3,9)],fill=INK)
        d.polygon([(8,6),(12,9),(8,14),(4,9)],fill=GOLD)
        d.polygon([(8,7),(10,9),(8,12),(6,9)],fill='#'+col)
        d.line((8,7,8,9),fill=CREAM)
        return im
    if name in ('Copper ore','Iron ore','Coal'):
        d.polygon([(1,10),(2,6),(5,5),(7,2),(11,2),(14,6),
                   (15,11),(12,14),(4,14)],fill=INK)
        d.polygon([(3,7),(7,5),(8,3),(11,4),(13,8),(12,12),(5,12)],fill='#556574')
        vein='#da9363' if name=='Copper ore' else '#b9d0d3' if name=='Iron ore' else '#303f50'
        for x,y in ((5,8),(9,5),(11,10)):
            d.polygon([(x-1,y),(x+1,y-1),(x+2,y+1),(x,y+2)],fill=vein)
        d.line((7,4,9,3),fill='#81949c')
        return im
    if name in ('Crystal','Ruby'):
        d.polygon([(2,9),(5,5),(7,7),(9,1),(13,4),(13,12),
                   (8,15),(4,14)],fill=INK)
        d.polygon([(8,6),(10,2),(12,4),(12,11),(8,14)],fill='#'+col)
        d.polygon([(8,6),(10,3),(10,10),(8,12)],fill='#d7ffdf' if name=='Crystal' else '#ffd3bd')
        d.polygon([(3,9),(5,6),(6,9),(6,12)],fill='#'+col)
        d.point((5,7),fill=CREAM)
        return im
    return None
