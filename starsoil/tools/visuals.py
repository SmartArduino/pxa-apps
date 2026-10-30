"""Starsoil's hand-authored pixel materials, inventory silhouettes and creatures.

Shared ink, cool shadows, warm highlights. All artwork is painted at native
resolution; there is no antialiasing, runtime image download or sprite pack.
"""
from PIL import Image, ImageDraw
INK='#263342'
CREAM='#fff0c9'
GOLD='#eac47a'


def canvas(w=16,h=16):
    im=Image.new('RGBA',(w,h));return im,ImageDraw.Draw(im)


def shade(c,n):
    c=bytes.fromhex(c.lstrip('#')) if isinstance(c,str) else c
    return tuple(max(0,min(255,v+n)) for v in c[:3])


def round_contour(im,mask,t):
    """Connected sides stay square; only truly exposed outside corners curve."""
    d=ImageDraw.Draw(im)
    radius=2 if t in (9,10,17,18) else 3 if t==6 else 7 if t==8 else 5
    for cx,cy,a,b in ((0,0,1,8),(15,0,1,2),(0,15,4,8),(15,15,4,2)):
        if mask&a or mask&b:continue
        for yy in range(radius):
            for xx in range(radius):
                if (radius-xx-.5)**2+(radius-yy-.5)**2>radius*radius:
                    d.point((cx+xx if cx==0 else cx-xx,cy+yy if cy==0 else cy-yy),fill=(0,0,0,0))
    # The visible boundary follows the alpha contour, including the new arc.
    source=im.copy()
    for y in range(16):
        for x in range(16):
            p=source.getpixel((x,y))
            if not p[3]:continue
            if y>0 and not source.getpixel((x,y-1))[3]:d.point((x,y),fill=(*shade(p,23),255))
            if y<15 and not source.getpixel((x,y+1))[3]:d.point((x,y),fill=(*shade(p,-22),255))
    return im


def tree_log(mask):
    """Left/right forks and root flares are painted behind continuous bark."""
    im,d=canvas()
    paths=[]
    if mask&1:paths.append((0,5,4,7,8,11))
    if mask&2:paths.append((15,4,12,6,8,11))
    if mask&4:paths.extend(((2,15,5,13,8,9),(14,15,11,13,8,9)))
    for path in paths:
        d.line(path,fill='#4b3b3b',width=5)
        d.line(path,fill='#986c4c',width=3)
        d.line(tuple(v-1 if i%2 else v for i,v in enumerate(path)),fill='#c39563')
    d.rectangle((4,0,12,15),fill='#4b3b3b')
    d.rectangle((5,0,11,15),fill='#986c4c')
    d.line((6,0,6,15),fill='#c39563');d.line((11,0,11,15),fill='#69493f')
    d.line((9,0,8,4,10,7,8,10,9,15),fill='#6e4d40')
    d.line((5,12,7,10),fill='#dab27c');d.point((7,5),fill='#d2a06c')
    return im


def tile(t,mask,fallback):
    if not t:return canvas()[0]
    im,d=canvas()
    colors={1:'8e604e',2:'77af59',3:'6e7e91',4:'d9b87a',5:'d7e9e7',6:'78b9cb',9:'a27655',10:'8798a2',17:'564a70',18:'41455f',19:'658367'}
    if t in colors or 11<=t<=16:
        base='6e7e91' if 11<=t<=16 else '8e604e' if t==2 else colors[t]
        d.rectangle((0,0,15,15),fill='#'+base)
        if t in (3,10,17,18,19) or 11<=t<=16:
            # Interlocking slabs: broken, stepped clusters rather than speckle.
            for x,y,w in ((0,3,5),(8,1,6),(4,8,7),(0,13,4),(12,12,4)):
                d.line((x,y,x+w-1,y),fill=shade(base,16))
                d.line((x,y+1,x+w-2,y+1),fill=shade(base,5))
                d.line((x+1,y+3,x+w-1,y+3),fill=shade(base,-15))
                d.point((x+w-1,y+2),fill=shade(base,-20))
        elif t==9:
            for y in (3,9,15):
                d.line((0,y,15,y),fill=shade(base,-28));d.line((0,y-1,15,y-1),fill=shade(base,20))
            for x,y in ((5,0),(11,4),(3,10)):
                d.line((x,y,x,y+2),fill=shade(base,-18));d.point((x+1,y+1),fill=shade(base,28))
        elif t==6:
            for x,y in ((1,8),(6,14),(8,5)):
                d.line((x,y,min(15,x+6),max(0,y-6)),fill='#a9dce0')
                d.point((x,y+1),fill='#5699b9')
        elif t in (4,5):
            for x,y in ((1,3),(7,1),(4,10),(11,8),(13,14)):
                d.line((x,y,x+2,y),fill=shade(base,17));d.point((x+1,y+1),fill=shade(base,-13))
        else:
            for x,y in ((1,2),(8,5),(3,10),(11,13)):
                d.polygon([(x,y),(x+3,y),(x+4,y+2),(x+1,y+3)],fill=shade(base,-13))
                d.line((x,y,x+2,y),fill=shade(base,16))
            d.line((12,1,10,3,11,5),fill=shade(base,-22))
        if 11<=t<=16:
            c=['d39570','b6ccd1','ecc974','45445e','71dccc','d87a9c'][t-11]
            for x,y in ((3,3),(10,5),(6,11)):
                if t in (15,16):
                    d.polygon([(x,y-2),(x+3,y),(x+2,y+4),(x-1,y+2)],fill=shade(c,-43))
                    d.polygon([(x,y-1),(x+2,y),(x+1,y+3),(x-1,y+1)],fill='#'+c)
                    d.line((x,y,x,y+2),fill=shade(c,62))
                else:
                    d.polygon([(x-1,y),(x+2,y-1),(x+3,y+1),(x+1,y+3),(x-1,y+2)],fill=shade(c,-30))
                    d.line((x,y,x+1,y),fill=shade(c,48));d.line((x,y+1,x+2,y+1),fill='#'+c)
        if not mask&1:
            d.line((0,0,15,0),fill=shade(base,30));d.line((0,1,15,1),fill=shade(base,12))
        if not mask&2:d.line((15,0,15,15),fill=shade(base,-26))
        if not mask&4:d.line((0,15,15,15),fill=shade(base,-31))
        if not mask&8:d.line((0,0,0,15),fill=shade(base,19))
        if t in (2,19) and not mask&1:
            grass='77af59' if t==2 else '83a77b'
            d.line((0,0,15,0),fill=shade(grass,37));d.line((0,1,15,1),fill='#'+grass)
            for x,h in ((0,2),(2,4),(5,3),(8,2),(11,4),(14,3)):
                d.rectangle((x,2,min(15,x+1),h),fill='#'+grass)
                d.point((x,h+1),fill=shade(grass,-28))
        return round_contour(im,mask,t)
    if t==7:
        return tree_log(0)
    if t==8:
        x0=0 if mask&8 else 1;x1=15 if mask&2 else 14;y0=0 if mask&1 else 1;y1=15 if mask&4 else 14
        d.rounded_rectangle((x0,y0,x1,y1),radius=3,fill='#385c4b')
        # A round corner is valid only when BOTH incident sides are exposed.
        # Restore the small corner square whenever either side is connected.
        for right,bottom,a,b in ((False,False,1,8),(True,False,1,2),
                                (False,True,4,8),(True,True,4,2)):
            if mask&a or mask&b:
                x=x1-3 if right else x0;y=y1-3 if bottom else y0
                d.rectangle((x,y,x+3,y+3),fill='#385c4b')
        # Broad overlapping leaf clusters, with fewer and softer highlights.
        # The previous six identical bright tufts made every tile stand out.
        for x,y in ((1,4),(9,1),(6,10)):
            d.polygon([(x,y+1),(x+2,y),(min(15,x+5),y+1),
                       (min(15,x+6),y+3),(min(15,x+3),min(15,y+5)),
                       (x,min(15,y+4))],fill='#527c54')
            d.line((x+1,y+1,min(15,x+3),y+1),fill='#7b995a')
            d.line((x+1,y+2,min(15,x+4),y+2),fill='#688956')
        # Only truly exposed corners curve. Joined corners remain filled.
        return round_contour(im,mask,8)
    if t in (20,21):
        c='4c8da9' if t==20 else 'cf754b'
        d.rectangle((0,2,15,15),fill='#'+c)
        d.line((0,2,15,2),fill='#a3ded5' if t==20 else '#ffcf7c')
        d.line((1,5,6,5),fill=shade(c,25));d.line((9,10,15,10),fill=shade(c,17));d.line((0,14,7,14),fill=shade(c,-20))
        return im
    if t in (29,31):
        d.line((8,7,8,15),fill='#497961');d.line((8,12,4,10),fill='#80ad68');d.line((8,10,12,8),fill='#80ad68')
        petal='#e9a6b9' if t==29 else '#ead58a'
        for x,y in ((6,4),(10,4),(8,2),(8,6)):
            d.rectangle((x-1,y-1,x+1,y+1),fill=shade(petal,-16));d.point((x,y-1),fill=shade(petal,28))
        d.rectangle((7,3,9,5),fill=GOLD);d.point((8,3),fill=CREAM);return im
    if t==30:
        d.rectangle((6,9,9,14),fill='#ad9d87');d.line((7,10,7,14),fill='#e8d3ae')
        d.polygon([(1,8),(3,5),(6,3),(10,3),(13,6),(14,9),(2,9)],fill=INK)
        d.polygon([(3,7),(5,4),(9,4),(12,7),(12,8),(3,8)],fill='#cb7280')
        d.line((4,5,6,4),fill='#e7a59e');d.point((6,6),fill=CREAM);d.point((10,7),fill=CREAM);return im
    if t==32:
        d.rounded_rectangle((5,0,10,15),radius=2,fill='#3e6c5a');d.line((7,1,7,15),fill='#88ab69')
        d.line((2,6,2,10,5,10),fill='#577f5d',width=2);d.line((13,2,13,7,10,7),fill='#577f5d',width=2)
        for y in (3,8,13):d.point((9,y),fill='#c8c88b')
        d.point((6,1),fill='#a5bc77');return im
    if t==25:
        d.rectangle((0,6,15,8),fill=INK);d.line((1,6,14,6),fill='#deb57b');d.line((1,7,14,7),fill='#a67c50')
        for x in (2,12):d.rectangle((x,9,x+1,15),fill='#6f5343');d.point((x,10),fill='#c79f6c')
        d.line((4,11,11,11),fill='#7f604b');d.line((3,4,7,3),fill='#c7d2c3');d.rectangle((10,3,12,5),fill='#a4b9bd');return im
    if t==27:
        d.polygon([(0,4),(15,4),(13,7),(10,8),(9,11),(12,14),(12,15),(3,15),(3,14),(6,11),(5,8),(1,7)],fill=INK)
        d.line((1,4,14,4),fill='#e1e8d1');d.line((2,5,12,5),fill='#a4bfc5');d.line((5,6,11,6),fill='#6d8a9e')
        d.line((7,9,7,12),fill='#b2c9c5');d.line((4,14,11,14),fill='#8ca2ad');return im
    if t in (28,37):
        d.polygon([(1,5),(3,3),(12,3),(14,5),(14,14),(1,14)],fill=INK)
        d.rectangle((2,5,13,13),fill='#906443');d.line((3,4,12,4),fill='#d7ab69')
        d.rectangle((3,6,12,8),fill='#b58450');d.line((2,9,13,9),fill='#533e37')
        for x in (3,12):d.line((x,5,x,13),fill=GOLD)
        d.rectangle((7,8,9,11),fill='#f7d993');d.point((8,9),fill='#654b3c')
        d.line((4,12,6,12),fill='#ba905b');return im
    return fallback(t,mask)


def icon(kind,col,t,name,tile_fn):
    im,d=canvas();low=name.lower();c='#'+col
    if kind in ('Block','Furniture') and t and name not in ('Water flask','Lava flask'):return tile_fn(t,0)
    if name in ('Wildflower','Daybloom'):return tile_fn(29 if name=='Wildflower' else 31,0)
    if name=='Arrows':
        d.line((3,13,12,3),fill='#bdad85',width=2);d.polygon([(12,1),(14,1),(14,4),(11,4)],fill='#d2e1df')
        d.line((2,10,5,13),fill='#79a79d');d.line((1,12,3,14),fill=CREAM);return im
    if 'ingot' in low:
        d.polygon([(1,8),(4,4),(11,4),(15,8),(13,13),(2,13)],fill=INK)
        d.polygon([(3,7),(5,5),(10,5),(13,8),(7,9)],fill=shade(col,42))
        d.polygon([(2,9),(7,10),(13,9),(12,12),(3,12)],fill=c)
        d.line((8,10,12,10),fill=shade(col,-36));d.point((4,6),fill=CREAM);return im
    if kind in ('Pick','Axe','Wand','Sword'):
        # Grip coordinates stay authored and opaque in every material variant.
        if kind=='Sword':
            d.line((3,13,6,10),fill=INK,width=4);d.line((3,13,6,10),fill='#b77f54',width=2)
            if 'crystal' in low:
                d.polygon([(5,9),(12,0),(15,0),(15,3),(8,12)],fill=INK)
                d.polygon([(6,9),(13,1),(14,1),(13,4),(8,10)],fill=c)
                d.line((7,8,13,2),fill='#ddffed');d.point((11,6),fill='#4d9cb6')
            elif 'wood' in low:
                d.polygon([(5,9),(11,2),(13,1),(13,4),(7,11)],fill=INK)
                d.line((6,9,12,2),fill=c,width=2);d.line((7,8,11,3),fill='#e6c28e')
            else:
                d.polygon([(5,9),(12,1),(14,1),(14,4),(8,12)],fill=INK)
                d.polygon([(6,9),(12,2),(13,2),(13,4),(8,10)],fill=c)
                d.line((7,8,12,3),fill=shade(col,55));d.point((9,8),fill=shade(col,-34))
            d.line((4,8,8,12),fill=GOLD,width=2);d.point((5,10),fill='#fff1bd');d.point((4,12),fill='#bc8556')
        else:
            d.line((3,14,11,3),fill=INK,width=4);d.line((4,13,11,4),fill='#b9875d',width=2)
            d.point((5,11),fill='#e1ba7f');d.line((4,13,5,13),fill='#634638')
            if kind=='Pick':
                if 'crystal' in low:
                    d.polygon([(4,3),(7,0),(12,1),(15,5),(13,6),(11,3),(7,3)],fill=INK)
                    d.line((5,2,8,1,12,2,14,5),fill=c,width=2);d.line((7,1,10,1),fill='#e4ffef');d.point((14,4),fill='#e4ffef')
                else:
                    d.line((4,3,7,1,11,1,14,4,14,6),fill=INK,width=3)
                    d.line((5,3,7,2,11,2,13,4,14,5),fill=c,width=2)
                    d.line((7,1,10,1),fill=shade(col,47));d.point((13,4),fill=shade(col,-27))
            elif kind=='Axe':
                d.polygon([(8,1),(12,0),(15,2),(15,6),(12,9),(9,7),(9,4),(7,3)],fill=INK)
                d.polygon([(10,2),(13,1),(14,3),(14,6),(12,7),(10,6)],fill=c)
                d.line((14,3,14,5,12,7),fill=shade(col,52));d.point((11,3),fill=shade(col,-31))
            else:
                d.polygon([(7,3),(10,0),(13,0),(15,3),(15,5),(12,8),(9,7)],fill=INK)
                d.polygon([(9,3),(11,1),(13,3),(12,6),(10,5)],fill=c)
                d.line((11,2,11,4),fill=CREAM);d.line((8,5,10,7,13,6),fill=GOLD)
        return im
    if kind=='Bow':
        d.line((8,1,11,2,14,6,14,9,11,13,8,14),fill=INK,width=3)
        d.line((8,1,11,3,13,6,14,8,12,11,8,14),fill=c,width=2)
        d.line((8,2,7,8,8,13),fill='#ded7ad')
        d.line((3,8,14,8),fill='#d6c9a5');d.point((14,8),fill=GOLD)
        d.point((11,3),fill='#ebc38a');return im
    if kind=='Head':
        if 'straw' in low:
            d.polygon([(5,2),(10,2),(12,6),(15,7),(14,9),(1,9),(0,7),(3,6)],fill=INK)
            d.rectangle((4,4,11,7),fill=c);d.line((2,8,13,8),fill='#f5dea0');d.line((5,6,10,6),fill='#a47e53')
        else:
            d.polygon([(4,3),(7,1),(11,2),(14,5),(14,6),(9,6),(7,9),(7,13),(3,12),(2,7)],fill=INK)
            d.polygon([(5,4),(8,2),(11,3),(13,5),(8,5),(6,9),(5,11),(3,9)],fill=c)
            d.line((4,5,6,3,9,3),fill=shade(col,52));d.line((3,9,5,11),fill=shade(col,-28))
            if 'crystal' in low:d.polygon([(6,4),(7,0),(9,2),(8,5)],fill='#d5ffe9')
            elif 'iron' in low:d.line((8,2,8,5),fill=CREAM)
            else:d.point((5,8),fill=GOLD)
        return im
    if kind in ('Body','Shirt'):
        robe='robe' in low
        d.polygon([(4,2),(7,3),(10,2),(14,5),(14,8),(11,8),(12 if robe else 11,14),(3,14),(4,8),(1,8),(1,5)],fill=INK)
        d.polygon([(4,4),(7,5),(10,4),(12,6),(10,8),(10,12),(5,12),(5,7),(3,7)],fill=c)
        d.line((5,5,5,10),fill=shade(col,45));d.line((9,7,9,11),fill=shade(col,-32))
        if kind=='Body':
            d.line((5,6,10,6),fill=shade(col,53));d.line((6,8,9,8),fill=shade(col,-34));d.line((4,12,10,12),fill=GOLD)
            if 'crystal' in low:d.polygon([(8,6),(10,8),(8,10),(6,8)],fill='#c5ffeb')
            elif 'copper' in low:
                for x in (5,9):d.point((x,9),fill=GOLD)
        else:
            d.line((5,4,7,5,10,3),fill=CREAM);d.line((4,6,9,11),fill='#ba9768')
            d.rectangle((4,12,10,13),fill=shade(col,-40));d.point((8,12),fill=GOLD)
        return im
    if kind=='Legs':
        d.polygon([(3,2),(12,2),(12,14),(9,14),(8,7),(7,7),(6,14),(3,14)],fill=INK)
        d.rectangle((4,3,11,5),fill=c);d.line((4,6,4,12),fill=c,width=2);d.line((10,6,10,12),fill=shade(col,-20),width=2)
        d.line((4,8,5,8),fill=shade(col,50));d.line((9,8,10,8),fill=shade(col,32));return im
    if kind=='Cape':
        d.polygon([(6,2),(10,2),(12,6),(14,13),(10,14),(6,13),(2,14),(4,7)],fill=INK)
        d.polygon([(6,3),(9,3),(11,8),(12,12),(8,12),(4,13),(5,7)],fill=c)
        d.line((6,5,5,10),fill=shade(col,36));d.line((9,6,10,11),fill=shade(col,-29));d.line((4,12,11,12),fill=GOLD);d.point((8,3),fill=CREAM);return im
    if kind=='Boots':
        for x,y in ((1,2),(8,4)):
            d.polygon([(x,y),(x+4,y),(x+4,y+7),(x+6,y+8),(x+6,y+10),(x,y+10)],fill=INK)
            d.rectangle((x+1,y+1,x+3,y+7),fill=c);d.line((x+1,y+9,x+5,y+9),fill=c);d.point((x+2,y+2),fill=CREAM)
        return im
    if kind=='Charm':
        d.arc((3,1,12,12),170,365,fill=GOLD)
        if 'magnet' in low:
            d.line((5,8,5,12,10,12,10,8),fill=INK,width=4);d.line((5,8,5,11,10,11,10,8),fill=c,width=2);d.point((5,8),fill='#df8e82');d.point((10,8),fill='#a7d8e2')
        else:
            d.polygon([(8,6),(12,10),(8,15),(4,10)],fill=INK);d.polygon([(8,7),(11,10),(8,13),(5,10)],fill=c);d.line((8,8,8,10),fill=CREAM)
        return im
    if kind=='Potion' or name in ('Water flask','Lava flask'):
        d.rectangle((6,1,9,3),fill=INK);d.line((6,2,9,2),fill='#c9a374')
        d.polygon([(5,4),(10,4),(13,8),(12,14),(4,14),(3,8)],fill=INK)
        d.polygon([(6,4),(9,4),(11,8),(11,12),(5,12),(4,8)],fill='#97b9bd')
        d.rectangle((5,8,10,12),fill=c);d.line((5,9,10,9),fill=shade(col,30));d.line((5,6,5,8),fill=CREAM)
        d.point((8,12),fill=shade(col,-18));return im
    if kind=='Food':
        if 'mushroom' in low:return tile_fn(30,0)
        for x,y in ((5,8),(10,7),(8,12)):
            d.ellipse((x-3,y-3,x+2,y+2),fill=INK);d.ellipse((x-2,y-2,x+1,y+1),fill=c);d.point((x-1,y-1),fill=shade(col,55))
        d.line((6,4,8,2,11,3),fill='#7fad6b');return im
    if 'silk' in low:
        d.rounded_rectangle((2,3,13,13),radius=2,fill=INK);d.rectangle((3,4,12,12),fill=c)
        d.line((4,5,11,5),fill=CREAM);d.line((4,8,11,8),fill='#bdad9a');d.line((5,11,11,11),fill='#b5a58e');return im
    if 'crystal' in low or 'ruby' in low or name=='Star heart':
        d.polygon([(8,1),(13,5),(12,11),(7,15),(2,10),(3,5)],fill=INK)
        d.polygon([(8,2),(11,5),(9,10),(7,13),(4,9),(5,5)],fill=c)
        d.polygon([(8,2),(8,8),(4,9),(5,5)],fill=shade(col,48));d.line((9,8,8,12),fill=shade(col,-38))
        if name=='Star heart':d.line((6,7,10,7),fill=CREAM);d.line((8,5,8,10),fill=CREAM)
        return im
    if 'stardust' in low:
        for x,y in ((4,4),(11,3),(8,10),(3,13),(13,12)):
            d.line((x-1,y,x+1,y),fill=c);d.line((x,y-1,x,y+1),fill=CREAM)
        return im
    if name=='Glass':
        d.polygon([(3,4),(11,1),(13,11),(5,14)],fill=INK);d.polygon([(4,5),(10,3),(11,10),(6,12)],fill=c)
        d.line((5,6,9,5),fill=CREAM);d.line((7,10,10,7),fill='#cfede2');return im
    # Loose minerals and gel have distinct facets, with a dark connecting bed.
    if name=='Slime gel':
        d.polygon([(2,10),(4,5),(7,3),(11,4),(14,10),(12,13),(4,13)],fill=INK)
        d.ellipse((4,5,12,12),fill=c);d.line((6,6,9,6),fill=shade(col,57));return im
    for x,y in ((4,8),(9,4),(10,10)):
        d.polygon([(x-2,y-1),(x+1,y-3),(x+4,y),(x+3,y+4),(x-1,y+4)],fill=INK)
        d.polygon([(x-1,y),(x+1,y-2),(x+3,y),(x+2,y+2),(x,y+3)],fill=c)
        d.line((x,y,x+1,y-1),fill=shade(col,52));d.point((x+2,y+2),fill=shade(col,-28))
    return im


def creature(kind,frame):
    im,d=canvas(24,24);b=(0,1,1,0,-1,0)[frame]
    if kind==0:
        squash=(0,1,2,1,0,-1)[frame];top=11+squash
        d.polygon([(3,21),(2,top+5),(4,top+1),(8,top),(15,top),(19,top+2),(21,top+6),(20,22),(5,22)],fill=INK)
        d.rounded_rectangle((4,top+2,19,21),radius=4,fill='#4d9b92')
        d.polygon([(6,top+2),(13,top+1),(17,top+3),(17,top+6),(6,top+6)],fill='#8dd5b4')
        d.line((7,top+2,11,top+2),fill='#d9f4cb')
        for x in (10,16):d.line((x,top+5,x,top+7),fill='#233a48');d.point((x,top+5),fill='#fff1c8')
        d.line((13,top+9,14,top+9),fill='#a9e7ba');d.point((6+frame,20),fill='#b0ead0')
    elif kind==1:
        flap=(0,2,4,6,3,1)[frame]
        for flip in (False,True):
            points=[(10,12),(4,4+flap),(1,3+flap),(1,11+flap),(4,10+flap),(6,15+flap//2),(9,15)]
            if flip:points=[(23-x,y) for x,y in points]
            d.polygon(points,fill=INK);p=[(9,12),(3,6+flap),(3,10+flap),(7,12+flap//2),(9,14)]
            if flip:p=[(23-x,y) for x,y in p]
            d.polygon(p,fill='#79628c');d.line(p[:2],fill='#b08ca6')
        d.polygon([(9,13),(9,6),(11,8),(14,8),(16,6),(16,16),(14,19),(11,19)],fill=INK)
        d.rectangle((10,10,15,16),fill='#574963')
        for x in (11,14):d.point((x,12),fill='#f1bc88')
        d.point((12,16),fill='#f6e5ca');d.point((14,16),fill='#f6e5ca')
    elif kind==2:
        step=(-2,-1,1,2,1,-1)[frame]
        for x,dx in ((8,step),(15,-step)):
            d.line((x,17,x+dx,22),fill=INK,width=4);d.line((x,17,x+dx,21),fill='#977957',width=2)
        d.line((7,12,3,17+b),fill=INK,width=3);d.line((17,12,21,17-b),fill=INK,width=3)
        d.rounded_rectangle((6,5+b,18,18),radius=3,fill=INK);d.rectangle((8,8+b,16,17),fill='#886b53')
        d.line((9,12,9,16),fill='#ba9568');d.line((14,10,14,16),fill='#5e4d49')
        d.polygon([(4,8+b),(6,4+b),(10,3+b),(13,5+b),(17,3+b),(20,8+b),(16,10+b),(9,9+b)],fill='#487857')
        d.line((7,5+b,10,4+b,12,6+b),fill='#9ab967');d.point((17,5+b),fill='#bbcd77')
        d.line((10,2+b,8,b),fill='#759c62');d.line((14,3+b,17,1+b),fill='#87ac68')
        for x in (10,15):d.point((x,11+b),fill='#f6d492')
    elif kind==3:
        # A faceted shelled crawler, not a recolored forest slime.
        for x in (5,10,16,20):d.line((x,18,x+(frame%3)-1,22),fill='#445571',width=2)
        d.polygon([(3,18),(4,12+b),(8,8+b),(16,8+b),(21,14),(21,20),(7,21)],fill=INK)
        d.polygon([(5,17),(7,12+b),(12,10+b),(17,12+b),(18,19),(9,19)],fill='#7287af')
        for x,y in ((7,12+b),(12,9+b),(16,13+b)):
            d.polygon([(x-2,y+2),(x,y-5),(x+3,y),(x+2,y+5)],fill='#8bbdcf')
            d.line((x,y-3,x,y+2),fill='#dcf8e4')
        d.rectangle((18,15,22,19),fill='#536c85');d.point((21,16),fill='#f6cfa2')
    else:
        # Armoured stone guardian, floating crystal core, articulated limbs.
        for x,sign in ((3,-1),(19,1)):
            sway=(0,1,2,1,0,-1)[frame]*sign
            d.polygon([(x,10+b),(x+3,9+b),(x+3,16+b+sway),(x+sign,18+b+sway),(x-1,16+b+sway)],fill=INK)
            d.rectangle((x,11+b,x+2,15+b+sway),fill='#6b8c91');d.point((x,11+b),fill='#bbdac5')
        for x in (7,15):
            d.rectangle((x,18,x+3,23),fill=INK);d.rectangle((x+1,19,x+2,21),fill='#819f9c');d.point((x+1,22),fill=GOLD)
        d.polygon([(6,8+b),(10,6+b),(16,7+b),(19,10+b),(17,19),(8,19),(5,14)],fill=INK)
        d.polygon([(7,9+b),(10,8+b),(16,9+b),(17,14),(15,17),(9,17),(7,14)],fill='#567980')
        d.polygon([(12,10+b),(16,13+b),(12,17+b),(9,13+b)],fill='#67d3bd')
        d.line((12,11+b,12,15+b),fill='#e9ffce');d.point((14,13+b),fill='#b6f9d5')
        d.polygon([(7,7+b),(7,3+b),(10,1+b),(15,2+b),(18,5+b),(16,9+b),(9,9+b)],fill=INK)
        d.line((9,4+b,15,4+b),fill='#9bbeb4');d.line((9,6+b,15,6+b),fill='#d9efbb')
        d.point((12,7+b),fill='#273f50');d.line((10,2+b,11,b),fill='#7fdbbc');d.point((14,1+b),fill=GOLD)
    return im


def foliage(mask,snow=False):
    im=tile(8,mask,lambda t,m:None)
    colors={'#385c4b':'#5a7e8d' if snow else '#356858','#527c54':'#86a5b0' if snow else '#50896b',
            '#7b995a':'#e2ebdf' if snow else '#91b36b','#688956':'#b6cfcc' if snow else '#6e9d6c'}
    mapping={shade(k,n):shade(v,n) for k,v in colors.items() for n in (0,23,-22)}
    im.putdata([(*mapping.get(tuple(p[:3]),p[:3]),p[3]) for p in im.getdata()])
    d=ImageDraw.Draw(im)
    if snow and not mask&1:
        d.line((3,1,12,1),fill='#edf2e2');d.line((1,2,14,2),fill='#dce9de')
        d.line((2,3,6,3),fill='#edf2e2');d.point((10,3),fill='#edf2e2')
    if not snow and not mask&2:
        d.point((12,5),fill='#dca2b0');d.point((13,4),fill='#f2c9bf')
    return im
