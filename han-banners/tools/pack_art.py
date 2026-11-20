#!/usr/bin/env python3
"""Pack original art and the licensed Z Labs pixel bitmap subset."""

import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance, ImageFont, ImageOps

ROOT = Path(__file__).resolve().parents[1]
FONT_ROOT = ROOT / "tools" / "fonts"
SIZE = 256
pages = []


def new_page(w, h):
    pages.append(Image.new("RGBA", (w, h)))
    return len(pages) - 1


def image(name):
    return Image.open(ROOT / "art" / (name + ".png")).convert("RGBA")


def whole(im):
    index = new_page(*im.size)
    pages[index].paste(im)
    return (index, 0, 0, *im.size)


def scaled(im, maximum=256):
    im.thumbnail((maximum,maximum),Image.Resampling.LANCZOS)
    return im


cover_wide=whole(scaled(image("cover")))
cover_square=whole(ImageOps.fit(image("square-cover"),(256,256),Image.Resampling.LANCZOS))
logo=image("logo")
bounds=logo.getchannel("A").point(lambda a:255 if a>=100 else 0).getbbox()
if bounds is None:
    raise ValueError("Logo lettering is missing")
logo=whole(scaled(logo.crop(bounds)))

outcomes=[]
for name in ("outcome-win","outcome-loss"):
    banner=image(name)
    bounds=banner.getchannel("A").point(lambda a:255 if a>=110 else 0).getbbox()
    if bounds is None:
        raise ValueError(f"Missing result lettering in {name}")
    outcomes.append(whole(scaled(banner.crop(bounds),192)))

ornament=image("ui-frame")
bounds=ornament.getchannel("A").point(lambda a:255 if a>=105 else 0).getbbox()
if bounds is None:
    raise ValueError("Missing frame border")
skin=whole(ornament.crop(bounds).resize((96,96),Image.Resampling.LANCZOS))

bar_source=image("hud-bars")
_,projection=bar_source.getchannel("A").point(lambda a:255 if a>=105 else 0).getprojection()
runs=[]
start=None
for row,covered in enumerate([*projection,0]):
    if covered and start is None:
        start=row
    elif not covered and start is not None:
        if row-start>=16:
            runs.append((start,row))
        start=None
if len(runs)!=3:
    raise ValueError("Expected three independent HUD bar strips")
bars_page=new_page(192,36)
hud_bars=[]
for i,(top,bottom) in enumerate(runs):
    strip=bar_source.crop((0,top,bar_source.width,bottom))
    bounds=strip.getchannel("A").point(lambda a:255 if a>=105 else 0).getbbox()
    strip=strip.crop(bounds).resize((192,12),Image.Resampling.LANCZOS)
    pages[bars_page].paste(strip,(0,i*12))
    hud_bars.append((bars_page,0,i*12,192,12))

battlefields = [whole(image(name).resize((256, 136), Image.Resampling.LANCZOS))
                for name in ("battle","hill-battle","river-battle")]
siege=whole(image("siege").resize((256,136),Image.Resampling.LANCZOS))
terrain = image("terrain").resize((512,416),Image.Resampling.LANCZOS)
map_tiles = [whole(terrain.crop((x,y,x+256,y+208))) for y in (0,208) for x in (0,256)]

portraits = []
portrait_size = 50
for name,columns,rows in (("portraits",3,2),("officers",4,3),("veterans",3,3)):
    people=image(name)
    for i in range(columns*rows):
        x, y = i % columns, i // columns
        crop = people.crop((x*people.width//columns, y*people.height//rows,
                            (x+1)*people.width//columns, (y+1)*people.height//rows))
        crop = ImageOps.fit(crop, (portrait_size,portrait_size), Image.Resampling.LANCZOS)
        index=len(portraits)%20
        if index==0:
            portrait_page=new_page(portrait_size*5,portrait_size*(4 if not portraits else 2))
        x,y=portrait_size*(index%5),portrait_size*(index//5)
        pages[portrait_page].paste(crop, (x,y))
        portraits.append((portrait_page,x,y,portrait_size,portrait_size))

def crop_cell(sheet, column, row, columns, rows):
    cell=sheet.crop((round(column*sheet.width/columns),round(row*sheet.height/rows),
                     round((column+1)*sheet.width/columns),round((row+1)*sheet.height/rows)))
    bounds=cell.getchannel("A").point(lambda a:255 if a>=128 else 0).getbbox()
    if bounds is None:
        raise ValueError(f"Missing transparent atlas cell {row}/{column}")
    return cell.crop(bounds)


war_hall=whole(scaled(image("war-hall")))
city_courtyard=whole(scaled(image("city-courtyard")))

# These UI tiles travel together so the battle scene uses one texture slot for
# its buttons, icons and action states.
ui_page=new_page(144,236)


def pack_icons(sheet, columns, rows, top, cell_size):
    tiles=[]
    for i in range(columns*rows):
        icon=scaled(crop_cell(sheet,i%columns,i//columns,columns,rows),cell_size-2)
        x,y=i%4*cell_size,top+i//4*cell_size
        pages[ui_page].paste(icon,(x+(cell_size-icon.width)//2,y+(cell_size-icon.height)//2))
        tiles.append((ui_page,x,y,cell_size,cell_size))
    return tiles


command_icons=pack_icons(image("strategy-icons"),4,3,0,36)
prop_sheet=image("cartography-props")
# Remove the faint gray matte around the generated prop silhouettes.
prop_sheet.putalpha(prop_sheet.getchannel("A").point(
    lambda a:max(0,min(255,round((a-136)*255/105)))))
map_props=pack_icons(prop_sheet,2,2,108,36)

wood=image("command-plaque")
bounds=wood.getchannel("A").point(lambda a:255 if a>=105 else 0).getbbox()
if bounds is None:
    raise ValueError("Missing command plaque")
wood=wood.crop(bounds).resize((144,30),Image.Resampling.LANCZOS)
plaques=[]
for i,(brightness,saturation) in enumerate(((1,1),(1.52,1.12),(.64,.58))):
    strip=ImageEnhance.Brightness(ImageEnhance.Color(wood).enhance(saturation)).enhance(brightness)
    y=144+i*31
    pages[ui_page].paste(strip,(0,y))
    plaques.append((ui_page,0,y,144,30))


def separated_poses(sheet, columns, rows):
    # Generated weapon tips can cross the nominal atlas cells. Pull complete
    # silhouettes by alpha connectivity before normalizing their foot line.
    width,height=sheet.size
    alpha=sheet.getchannel("A").tobytes()
    pending=bytearray(1 if n>=128 else 0 for n in alpha)
    regions=[]
    for seed in range(len(pending)):
        if not pending[seed]:
            continue
        pending[seed]=0
        component=[seed]
        queue=[seed]
        x0=x1=seed%width
        y0=y1=seed//width
        while queue:
            k=queue.pop()
            x,y=k%width,k//width
            x0,x1=min(x0,x),max(x1,x)
            y0,y1=min(y0,y),max(y1,y)
            for py in range(max(0,y-1),min(height,y+2)):
                for px in range(max(0,x-1),min(width,x+2)):
                    j=py*width+px
                    if pending[j]:
                        pending[j]=0
                        component.append(j)
                        queue.append(j)
        if len(component)<1024:
            continue
        w,h=x1-x0+1,y1-y0+1
        coverage=bytearray(w*h)
        for k in component:
            coverage[(k//width-y0)*w+k%width-x0]=alpha[k]
        sprite=sheet.crop((x0,y0,x1+1,y1+1))
        sprite.putalpha(Image.frombytes("L",(w,h),bytes(coverage)))
        regions.append((x0,((y0+y1)*rows)//(height*2),sprite))
    groups=[sorted((r for r in regions if r[1]==i),key=lambda r:r[0]) for i in range(rows)]
    if any(len(group)!=columns for group in groups):
        raise ValueError(f"Expected {columns} x {rows} connected animation poses")
    return [[sprite for _,_,sprite in group] for group in groups]


def opponent_sprite(source):
    source=ImageOps.mirror(source)
    pixels=source.load()
    for y in range(source.height):
        for x in range(source.width):
            r,g,b,a=pixels[x,y]
            if (b>=r-4 and g>r*.98) or (g>r*.95 and g>b*1.2):
                pixels[x,y]=min(255,int(g*1.48+24)),int(r*.66+8),int(b*.65+6),a
    return source


# The art master is arranged as spear / cavalry / bow. The game shares
# the spear / bow / cavalry order between its UI and rules.
def troop_sheet(name):
    sheet=image(name)
    packed=[]
    rows=separated_poses(sheet,6,3) if name=="tactic-frames" else [
        [crop_cell(sheet,col,row,6,3) for col in range(6)] for row in range(3)]
    for side in range(2):
        slot=new_page(252,126)
        stances=[]
        for row,source_row in enumerate((0,2,1)):
            poses=rows[source_row]
            ratio=min(38/max(f.width for f in poses),37/max(f.height for f in poses))
            frames=[]
            for col,source in enumerate(poses):
                w,h=max(1,round(source.width*ratio)),max(1,round(source.height*ratio))
                sprite=source.resize((w,h),Image.Resampling.LANCZOS)
                if side:
                    sprite=opponent_sprite(sprite)
                x,y=col*42,row*42
                pages[slot].paste(sprite,(x+(42-w)//2,y+40-h))
                frames.append((slot,x,y,42,42))
            stances.append(frames)
        packed.append(stances)
    return packed


troops=troop_sheet("soldiers")
tactics=troop_sheet("tactic-frames")
duel_sheet=image("duel-frames")
duel_poses=separated_poses(duel_sheet,6,1)[0]
duel_scale=min(61/max(p.width for p in duel_poses),58/max(p.height for p in duel_poses))
duels=[]
for side in range(2):
    slot=new_page(192,128)
    frames=[]
    for i,pose in enumerate(duel_poses):
        w,h=max(1,round(pose.width*duel_scale)),max(1,round(pose.height*duel_scale))
        sprite=pose.resize((w,h),Image.Resampling.LANCZOS)
        if side:
            sprite=opponent_sprite(sprite)
        x,y=(i%3)*64,(i//3)*64
        pages[slot].paste(sprite,(x+(64-w)//2,y+62-h))
        frames.append((slot,x,y,64,64))
    duels.append(frames)

items=image("gear")
equipment_page=new_page(128,128)
equipment=[]
for i in range(4):
    icon=scaled(crop_cell(items,i%2,i//2,2,2),56)
    x,y=(i%2)*64,(i//2)*64
    pages[equipment_page].paste(icon,(x+(64-icon.width)//2,y+(64-icon.height)//2))
    equipment.append((equipment_page,x,y,64,64))

marchers = [[],[]]
march_page = new_page(192,64)
for enemy in range(2):
    for flip in range(2):
        strip=[]
        for frame in range(3):
            page,x,y,w,h=troops[enemy][2][frame]
            sprite=pages[page].crop((x,y,x+w,y+h))
            if bool(enemy)!=bool(flip):
                sprite=ImageOps.mirror(sprite)
            sprite=sprite.resize((32,28),Image.Resampling.LANCZOS)
            x0,y0=96*flip+frame*32,enemy*28
            pages[march_page].paste(sprite,(x0,y0))
            strip.append((march_page,x0,y0,32,28))
        marchers[enemy].append(strip)

source = "".join((ROOT / filename).read_text()
                 for filename in ("main.cpp","expansion_ui.inc","campaign.hpp","world.hpp","chapters.hpp","battle.hpp","save.hpp"))
chars = sorted(set(map(chr, range(32,127))) | {c for c in source if ord(c)>127})
provenance=json.loads((FONT_ROOT / "upstream.json").read_text())
font_path=FONT_ROOT/provenance["font_file"]
license_path=FONT_ROOT/provenance["license_file"]
for file,key in ((font_path,"font_sha256"),(license_path,"license_sha256")):
    if hashlib.sha256(file.read_bytes()).hexdigest()!=provenance[key]:
        raise ValueError(f"Font source hash mismatch: {file}")
font_px=12
font=ImageFont.truetype(str(font_path),font_px)
cols=SIZE//font_px
capacity=cols*cols
glyphs=[]
first_font_page=len(pages)
for i,char in enumerate(chars):
    if i%capacity==0:
        glyph_slot=new_page(cols*font_px,min(cols,(len(chars)-i+cols-1)//cols)*font_px)
    j=i%capacity
    x,y=(j%cols)*font_px,(j//cols)*font_px
    advance=max(1,round(font.getlength(char)))
    bitmap=Image.new("1",(advance,font_px))
    draw=ImageDraw.Draw(bitmap)
    draw.fontmode="1"
    draw.text((0,0),char,font=font,anchor="la",fill=1)
    alpha=bitmap.convert("L")
    solid=Image.new("RGBA",bitmap.size,(255,255,255,255))
    solid.putalpha(alpha)
    pages[glyph_slot].paste(solid,(x,y))
    glyphs.append((advance,(glyph_slot,x,y,advance,font_px)))

sans_spec=json.loads((FONT_ROOT / "sans.json").read_text())
for file_key,hash_key in (("font_file","font_sha256"),("license_file","license_sha256")):
    path=FONT_ROOT/sans_spec[file_key]
    if hashlib.sha256(path.read_bytes()).hexdigest()!=sans_spec[hash_key]:
        raise ValueError(f"Font source hash mismatch: {path}")
color_pages=pages[:first_font_page]
sample = Image.new("RGB", (256*3, 256*((len(color_pages)+2)//3)), (23,25,23))
for i, page in enumerate(color_pages):
    sample.paste(page, (256*(i%3), 256*(i//3)), page.getchannel("A"))
palette_source = sample.quantize(colors=255, method=Image.Quantize.MEDIANCUT)
palette = palette_source.getpalette()[:765]
colors = [0] + [((r>>3)<<11) | ((g>>2)<<5) | (b>>3)
                for r,g,b in zip(palette[0::3], palette[1::3], palette[2::3])]

output = ROOT / "resources"
output.mkdir(exist_ok=True)
spec = []
for i, page in enumerate(pages):
    data = page.convert("RGB").quantize(palette=palette_source, dither=Image.Dither.NONE).tobytes()
    alpha = page.getchannel("A").tobytes()
    if i >= first_font_page:
        packed = alpha
    else:
        packed = bytes(p + 1 if a >= 105 else 0 for p, a in zip(data, alpha))
    (output / f"atlas-{i}.index8").write_bytes(packed)
    spec.append(dict(path=f"assets/atlas-{i}.pxr", kind="index8",
                     source=f"resources/atlas-{i}.index8", width=page.width, height=page.height))
for old in output.glob("atlas-*.index8"):
    if int(old.stem.split("-")[-1])>=len(pages):
        old.unlink()
(ROOT / "resources.json").write_text(json.dumps({"assets":spec}, indent=2) + "\n")


def tile_initializer(t):
    return "{" + ",".join(map(str,t)) + "}"


lines = [
    "// Generated by tools/pack_art.py.",
    "#pragma once", "#include <array>", "#include <cstdint>",
    "namespace han::art {",
    "struct Tile {uint8_t page; uint16_t x,y,w,h;};",
    "struct Glyph {uint32_t code; uint8_t advance; Tile tile;};",
    f"inline constexpr int fontPixels = {font_px};",
    f"inline constexpr int fallbackGlyph = {chars.index('?')};",
    f"inline constexpr int pageCount = {len(pages)};",
    f"inline constexpr int firstFontPage = {first_font_page};",
    "inline constexpr std::array<uint16_t,256> palette = {" + ",".join(map(str,colors)) + "};",
]
for name,t in (("coverWide",cover_wide),("coverSquare",cover_square),("logo",logo),("siege",siege),
               ("skin",skin),("warHall",war_hall),("cityCourtyard",city_courtyard)):
    lines.append(f"inline constexpr Tile {name} = {tile_initializer(t)};")
for name,tiles in (("mapTiles",map_tiles),("battlefields",battlefields),("portraits",portraits),("equipment",equipment),
                   ("outcomes",outcomes),("hudBars",hud_bars),("commandIcons",command_icons),
                   ("mapProps",map_props),("plaques",plaques)):
    lines.append(f"inline constexpr Tile {name}[{len(tiles)}] = {{" +
                 ",".join(map(tile_initializer,tiles)) + "};")
for name,dimension,banks in (("troops","[2][3][6]",troops),
                             ("tacticTroops","[2][3][6]",tactics),
                             ("marchers","[2][2][3]",marchers)):
    lines.append(f"inline constexpr Tile {name}{dimension} = {{")
    for army in banks:
        lines.append("  {")
        for row in army:
            lines.append("    {" + ",".join(map(tile_initializer,row)) + "},")
        lines.append("  },")
    lines.append("};")
lines.append("inline constexpr Tile duelFrames[2][6] = {")
for row in duels:
    lines.append("  {" + ",".join(map(tile_initializer,row)) + "},")
lines += ["};", f"inline constexpr std::array<Glyph,{len(chars)}> glyphs = {{{{"]
lines.extend(f"  {{{ord(char)},{advance},{tile_initializer(tile)}}},"
             for char,(advance,tile) in zip(chars,glyphs))
lines += ["}};", "}"]
(ROOT / "art.hpp").write_text("\n".join(lines) + "\n")

icon = ImageOps.fit(image("app-icon"),(128,128),Image.Resampling.LANCZOS)
icon.convert("RGB").save(ROOT / "assets" / "icon.png", optimize=True)
