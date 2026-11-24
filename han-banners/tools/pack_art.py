#!/usr/bin/env python3
"""Pack original art and the licensed Z Labs pixel bitmap subset."""

import hashlib
import json
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageEnhance, ImageFilter, ImageFont, ImageOps
from fontTools.ttLib import TTFont
from ui_art import lacquer_panel, progress_strips

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


def tiled_backdrop(compact, im, columns, rows):
    tw,th=im.width//columns,im.height//rows
    assert tw<=SIZE and th<=SIZE and im.size==(tw*columns,th*rows)
    result=(compact[0],len(pages),columns,rows,tw,th)
    for y in range(0,im.height,th):
        for x in range(0,im.width,tw):
            whole(im.crop((x,y,x+tw,y+th)))
    return result


def scaled(im, maximum=256):
    im.thumbnail((maximum,maximum),Image.Resampling.LANCZOS)
    return im


def display_rgb(color):
    return (round((color>>11)*255/31),round(((color>>5)&63)*255/63),round((color&31)*255/31))


def indexed_palette(colors):
    palette=Image.new("P",(1,1))
    palette.putpalette([channel for color in colors for channel in display_rgb(color)])
    return palette


def extend_cover_palette(cover, colors):
    # Some sprite colors are identical after conversion to RGB565. Keep one
    # index per shared color and use the duplicates for difficult cover shades.
    duplicates=[i for i,color in enumerate(colors) if colors.index(color)!=i]
    remap=list(range(256))
    nearest=cover.quantize(palette=indexed_palette(colors),dither=Image.Dither.NONE).convert("RGB")
    error=ImageChops.difference(cover,nearest)
    pixels=getattr(Image.Image,"get_flattened_data",Image.Image.getdata)
    samples=[pixel for pixel,delta in zip(pixels(cover),pixels(error)) if max(delta)>6]
    if duplicates and samples:
        sample=Image.new("RGB",(len(samples),1))
        sample.putdata(samples)
        extra=sample.quantize(colors=len(duplicates),method=Image.Quantize.MEDIANCUT).getpalette()
        slots=iter(duplicates)
        for r,g,b in zip(extra[0::3],extra[1::3],extra[2::3]):
            color=(round(r*31/255)<<11)|(round(g*63/255)<<5)|round(b*31/255)
            if color in colors:
                continue
            i=next(slots)
            remap[i]=colors.index(colors[i])
            colors[i]=color
    return indexed_palette(colors),bytes(remap)


wide_cover_source=image("river-crossing-cover")
cover_wide=whole(scaled(wide_cover_source.copy()))
cover_square=whole(ImageOps.fit(image("square-cover"),(256,256),Image.Resampling.LANCZOS))
logo_source=image("logo")
bounds=logo_source.getchannel("A").point(lambda a:255 if a>=100 else 0).getbbox()
if bounds is None:
    raise ValueError("Logo lettering is missing")
logo_source=logo_source.crop(bounds)
logo=whole(scaled(logo_source.copy()))

outcomes=[]
for name in ("outcome-win","outcome-loss"):
    banner=image(name)
    bounds=banner.getchannel("A").point(lambda a:255 if a>=110 else 0).getbbox()
    if bounds is None:
        raise ValueError(f"Missing result lettering in {name}")
    outcomes.append(whole(scaled(banner.crop(bounds),192)))

def panel_bitmap(name,size):
    panel=image(name)
    bounds=panel.getchannel("A").point(lambda a:255 if a>=128 else 0).getbbox()
    if bounds is None:
        raise ValueError(f"Missing frame silhouette: {name}")
    return panel.crop(bounds).resize(size,Image.Resampling.LANCZOS)


def tint_lacquer(source,shadow,highlight):
    # Recolor the recessed field while preserving the brass bevel and corner work.
    mask=Image.new("L",source.size)
    ImageDraw.Draw(mask).rectangle((16,6,source.width-17,source.height-7),fill=240)
    mask=mask.filter(ImageFilter.GaussianBlur(3))
    lacquer=ImageOps.colorize(ImageOps.grayscale(source),shadow,highlight)
    panel=Image.composite(lacquer,source.convert("RGB"),mask)
    panel.putalpha(source.getchannel("A"))
    return panel


frame_page=new_page(256,166)
battle_frame=panel_bitmap("battle-bronze",(140,36))
battle_panels=[]
for i,pane in enumerate((tint_lacquer(battle_frame,"#28170e","#bf8050"),
                         tint_lacquer(battle_frame,"#111e1c","#73a290"),
                         tint_lacquer(battle_frame,"#291613","#bd7664"))):
    y=i*38
    pages[frame_page].paste(pane,(0,y))
    battle_panels.append((frame_page,0,y,*pane.size))
window=panel_bitmap("amber-lacquer",(114,80))
window=tint_lacquer(window,"#261a11","#b27a50")
windows=[]
for i,pane in enumerate((window,ImageEnhance.Brightness(window).enhance(1.26))):
    y=i*83
    pages[frame_page].paste(pane,(142,y))
    windows.append((frame_page,142,y,*pane.size))

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
bars_page=new_page(192,48)
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
field_sources = {
    "battle": .620,
    "hill-battle": .695,
    "riverbank-panorama": .490,
    "siege": .580,
}


def field_panorama(name, size):
    """Reserve the lower half of every battlefield for traversable ground."""
    source = image(name)
    width, height = size
    safe_row = round(source.height * field_sources[name])
    span = min(source.width, round(source.height * 1024 / 480))
    left = (source.width - span) // 2
    result = Image.new("RGBA", size)
    for top, bottom, dest, rows in ((0, safe_row, 0, height // 2),
                                     (safe_row, source.height, height // 2, height - height // 2)):
        band = source.crop((left, top, left + span, bottom))
        result.paste(band.resize((width, rows), Image.Resampling.LANCZOS), (0, dest))
    return result


terrain = image("terrain").resize((512,512),Image.Resampling.LANCZOS)
map_tiles = [whole(terrain.crop((x,y,x+256,y+256))) for y in (0,256) for x in (0,256)]

portraits = []
portrait_size = 50
portrait_sheets = (("portraits",3,2),("officers",4,3),("veterans",3,3),("wild-talents",3,2))
portrait_count = sum(columns*rows for _,columns,rows in portrait_sheets)


def portrait_cells(name, columns, rows, size=portrait_size):
    people=image(name)
    for i in range(columns*rows):
        x, y = i % columns, i // columns
        crop = people.crop((x*people.width//columns, y*people.height//rows,
                            (x+1)*people.width//columns, (y+1)*people.height//rows))
        yield ImageOps.fit(crop, (size,size), Image.Resampling.LANCZOS)


for name,columns,rows in portrait_sheets:
    for crop in portrait_cells(name,columns,rows):
        index=len(portraits)%20
        if index==0:
            rows_on_page=(min(20,portrait_count-len(portraits))+4)//5
            portrait_page=new_page(portrait_size*5,portrait_size*rows_on_page)
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
ui_page=new_page(240,256)


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
def troop_poses(name):
    sheet=image(name)
    rows=separated_poses(sheet,6,3) if name=="tactic-frames" else [
        [crop_cell(sheet,col,row,6,3) for col in range(6)] for row in range(3)]
    return [rows[i] for i in (0,2,1)]


def troop_frames(slot, poses, size, columns, start, side):
    ratio=min(round(size*38/42)/max(f.width for f in poses),
              round(size*37/42)/max(f.height for f in poses))
    frames=[]
    for i, source in enumerate(poses,start):
        w,h=max(1,round(source.width*ratio)),max(1,round(source.height*ratio))
        sprite=source.resize((w,h),Image.Resampling.LANCZOS)
        if side:
            sprite=opponent_sprite(sprite)
        x,y=i%columns*size,i//columns*size
        pages[slot].paste(sprite,(x+(size-w)//2,y+round(size*40/42)-h))
        frames.append((slot,x,y,size,size))
    return frames


def troop_sheet(rows):
    packed=[]
    for side in range(2):
        slot=new_page(252,126)
        packed.append([troop_frames(slot,poses,42,6,row*6,side) for row,poses in enumerate(rows)])
    return packed


troop_sources=troop_poses("soldiers")
tactic_sources=troop_poses("tactic-frames")
troops=troop_sheet(troop_sources)
tactics=troop_sheet(tactic_sources)
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

# Leave room in the compact renderer's asset cache for march sprites and audio.
compact_map_tiles = [
    whole(terrain.crop((x,y,x+256,y+256)).resize((208,208),Image.Resampling.LANCZOS))
    for y in (0,256) for x in (0,256)
]

source = "".join((ROOT / filename).read_text()
                 for filename in ("main.cpp","expansion_ui.inc","campaign.hpp","world.hpp",
                                  "chapters.hpp","history.hpp","battle.hpp","save.hpp"))
chars = sorted(set(map(chr, range(32,127))) | {c for c in source if ord(c)>127})
provenance=json.loads((FONT_ROOT / "upstream.json").read_text())
font_path=FONT_ROOT/provenance["font_file"]
license_path=FONT_ROOT/provenance["license_file"]
for file,key in ((font_path,"font_sha256"),(license_path,"license_sha256")):
    if hashlib.sha256(file.read_bytes()).hexdigest()!=provenance[key]:
        raise ValueError(f"Font source hash mismatch: {file}")
sans_spec=json.loads((FONT_ROOT / "sans.json").read_text())
for file_key,hash_key in (("font_file","font_sha256"),("license_file","license_sha256")):
    path=FONT_ROOT/sans_spec[file_key]
    if hashlib.sha256(path.read_bytes()).hexdigest()!=sans_spec[hash_key]:
        raise ValueError(f"Font source hash mismatch: {path}")
font_px=12
font=ImageFont.truetype(str(font_path),font_px)
with TTFont(font_path) as face:
    pixel_codes=face.getBestCmap()
sans_small=ImageFont.truetype(str(FONT_ROOT/sans_spec["font_file"]),font_px)
cols=SIZE//font_px
capacity=cols*cols
glyphs=[]
first_font_page=len(pages)
for i,char in enumerate(chars):
    if i%capacity==0:
        glyph_slot=new_page(cols*font_px,min(cols,(len(chars)-i+cols-1)//cols)*font_px)
    j=i%capacity
    x,y=(j%cols)*font_px,(j//cols)*font_px
    is_pixel=ord(char) in pixel_codes
    face=font if is_pixel else sans_small
    advance=max(1,round(face.getlength(char)))
    bitmap=Image.new("1",(advance,font_px))
    draw=ImageDraw.Draw(bitmap)
    draw.fontmode="1"
    draw.text((0,0) if is_pixel else (0,10),char,font=face,
              anchor="la" if is_pixel else "ls",fill=1)
    alpha=bitmap.convert("L")
    solid=Image.new("RGBA",bitmap.size,(255,255,255,255))
    solid.putalpha(alpha)
    pages[glyph_slot].paste(solid,(x,y))
    glyphs.append((advance,(glyph_slot,x,y,advance,font_px)))

compact_map_pages={tile[0] for tile in compact_map_tiles}
color_pages=[page for i,page in enumerate(pages[:first_font_page]) if i not in compact_map_pages]
sample = Image.new("RGB", (256*3, 256*((len(color_pages)+2)//3)), (23,25,23))
for i, page in enumerate(color_pages):
    # Keep the shared sprite palette stable when replacing the title painting.
    if i==cover_wide[0]:
        page=scaled(image("cover"))
    sample.paste(page, (256*(i%3), 256*(i//3)), page.getchannel("A"))
palette_source = sample.quantize(colors=255, method=Image.Quantize.MEDIANCUT)

# Keep the shared actor/UI palette stable while updating the playfield paintings.
for tile, name in zip((*battlefields, siege), field_sources):
    pages[tile[0]] = field_panorama(name, tile[3:])

# A typical battle with two portrait sheets has just enough texture slots for
# the detailed backgrounds when marching and tactical poses share each army page.
detail_troops=[]
for side in range(2):
    march_page=troops[side][0][0][0]
    tactic_page=tactics[side][0][0][0]
    combined=Image.new("RGBA",(252,252))
    combined.paste(pages[march_page],(0,0))
    combined.paste(pages[tactic_page],(0,126))
    pages[tactic_page]=combined
    detail_troops.append([[(tactic_page,x,y,w,h) for _,x,y,w,h in row] for row in troops[side]])
    tactics[side]=[[(tactic_page,x,y+126,w,h) for _,x,y,w,h in row] for row in tactics[side]]

# Add new portraits after sampling so their colors do not repack existing artwork.
historical_cells=list(portrait_cells("historical-talents",3,2))
historical_page=portraits[-1][0]
existing=len(portraits)%20
assert existing and existing+len(historical_cells)<=20
old=pages[historical_page]
rows=(existing+len(historical_cells)+4)//5
pages[historical_page]=Image.new("RGBA",(old.width,rows*portrait_size))
pages[historical_page].paste(old,(0,0))
for index,cell in enumerate(historical_cells,existing):
    x,y=portrait_size*(index%5),portrait_size*(index//5)
    pages[historical_page].paste(cell,(x,y))
    portraits.append((historical_page,x,y,portrait_size,portrait_size))

# Use the empty corners of the two existing UI atlases. Both are already
# resident in deployment and battle, including on the compact texture budget.
mixed=image("mixed-forces")
new_sprites=[]
for i in range(9):
    source_index=i%3 if i<6 else i-3
    sprite=crop_cell(mixed,source_index%3,source_index//3,3,2)
    if i<3:
        page,x,y,w,h=frame_page,46*i,116,46,46
    else:
        page,x,y,w,h=ui_page,144+36*((i-3)%2),143+31*((i-3)//2),36,31
    sprite.thumbnail((w-2,h-2),Image.Resampling.LANCZOS)
    if 3<=i<6:
        sprite=opponent_sprite(sprite)
    pages[page].paste(sprite,(x+(w-sprite.width)//2,y+h-2-sprite.height))
    new_sprites.append((page,x,y,w,h))
# Add the HUD assets after sampling the existing palette to keep the other art intact.
for i,tone in enumerate((
        ("#33221a","#4e3324","#38251d"),
        ("#1d322d","#28413a","#223631"),
        ("#392721","#50302b","#402620"))):
    pages[frame_page].paste(lacquer_panel(tone),(0,i*38))
hud_bars.clear()
track,*fills=progress_strips()
for i,strip in enumerate((track,*fills[:-1])):
    pages[bars_page].paste(strip,(0,i*12))
    hud_bars.append((bars_page,0,i*12,192,12))
march_bars=[]
for i,strip in enumerate((track,fills[3],fills[1])):
    # The map already binds this atlas. Preserve both caps and collapse the flat middle.
    x,y=144,98+i*12
    for a,b,offset in ((0,8,0),(88,104,8),(184,192,24)):
        pages[ui_page].paste(strip.crop((a,0,b,12)),(x+offset,y))
    march_bars.append((ui_page,x,y,32,12))
command_sheet=image("command-icons-v2")


def generated_icon(sheet, index, x, y, w, h, page=ui_page):
    icon=crop_cell(sheet,index%4,index//4,4,4)
    icon.thumbnail((w-2,h-2),Image.Resampling.LANCZOS)
    pages[page].paste(icon,(x+(w-icon.width)//2,y+(h-icon.height)//2))
    return (page,x,y,w,h)


for i in range(12):
    command_icons.append(generated_icon(command_sheet,i,144+i%3*24,i//3*24,24,24))
for index,prop in ((12,1),(14,0),(15,2)):
    _,x,y,w,h=map_props[prop]
    pages[ui_page].paste((0,0,0,0),(x,y,x+w,y+h))
    map_props[prop]=generated_icon(command_sheet,index,x,y,w,h)
map_props.append(generated_icon(command_sheet,13,180,108,36,27))

# Keep new HUD and menu pictograms in the already-bound UI texture.
status_sheet=image("status-icons")
status_icons=[None]*16
for position,index in enumerate((0,1,2,3,4,5,6,8,9,10,7,11,12,13,14,15)):
    rect=(216,position*24,24,24) if position<10 else ((position-10)*20,236,20,20)
    status_icons[index]=generated_icon(status_sheet,index,*rect)
command_icons.extend(status_icons)

# Flatten each gate and its footing into one map sprite. Ownership stripes
# remain live drawing commands so every faction keeps its exact UI color.
page,x,y,w,h=map_props[0]
gate=pages[page].crop((x,y,x+w,y+h)).resize((26,26),Image.Resampling.LANCZOS)
marker=Image.new("RGBA",(26,30))
marker.paste(gate,(0,0))
ImageDraw.Draw(marker).rectangle((2,24,25,29),fill=(13,20,22,255))
pages[page].paste((0,0,0,0),(x,y,x+w,y+h))
pages[page].paste(marker,(x,y))
map_props[0]=(page,x,y,*marker.size)

# Finer editions share the same palette and use the renderer's 256 px tiles.
first_detailed_page=len(pages)
detailed_backdrops=[]
wide_cover=ImageOps.fit(wide_cover_source,(768,480),Image.Resampling.LANCZOS)
palette=palette_source.getpalette()[:765]
colors=[((r>>3)<<11)|((g>>2)<<5)|(b>>3)
        for r,g,b in zip(palette[0::3],palette[1::3],palette[2::3])]
cover_palette,shared_color_indices=extend_cover_palette(wide_cover.convert("RGB"),colors)
cover_indices = {cover_wide[0]: pages[cover_wide[0]].convert("RGB").quantize(
    palette=cover_palette,dither=Image.Dither.FLOYDSTEINBERG)}
for compact,name,w,h,cols,rows in (
        (cover_wide,"river-crossing-cover",768,480,3,2),
        (cover_square,"square-cover",512,512,2,2),
        (war_hall,"war-hall",768,256,3,1),
        (city_courtyard,"city-courtyard",864,256,4,1),
        *((tile,name,1024,136,4,1) for tile,name in zip((*battlefields,siege),field_sources))):
    if compact==cover_wide:
        back=wide_cover
    elif compact in (war_hall,city_courtyard):
        back=ImageOps.fit(image(name),(w,480),Image.Resampling.LANCZOS)
        back=back.resize((w,h),Image.Resampling.LANCZOS)
    elif name in field_sources:
        back=field_panorama(name,(w,h))
    else:
        back=image(name).resize((w,h),Image.Resampling.LANCZOS)
    # Diffuse the title painting before splitting it to avoid lines at tile edges.
    indexed=back.convert("RGB").quantize(palette=cover_palette,
        dither=Image.Dither.FLOYDSTEINBERG) if compact==cover_wide else None
    tw,th=w//cols,h//rows
    detailed_backdrops.append((compact[0],len(pages),cols,rows,tw,th))
    for y in range(0,h,th):
        for x in range(0,w,tw):
            region=(x,y,x+tw,y+th)
            tile=whole(back.crop(region))
            if indexed is not None:
                cover_indices[tile[0]]=indexed.crop(region)
leader_portrait_page=new_page(3*portrait_size,portrait_size)
for i,face in enumerate(portrait_cells("field-leader-portraits",3,1)):
    x=i*portrait_size
    pages[leader_portrait_page].paste(face,(x,0))
    portraits.append((leader_portrait_page,x,0,portrait_size,portrait_size))

leader_poses=separated_poses(image("field-leaders"),4,3)
bars=pages[bars_page]

def leader_row(page, poses, size, offset, columns, start, side, recolor=False):
    width,height=size
    scale=min((width-2)/max(p.width for p in poses),(height-2)/max(p.height for p in poses))
    frames=[]
    for index,pose in enumerate(poses,start):
        w,h=round(pose.width*scale),round(pose.height*scale)
        sprite=pose.resize((w,h),Image.Resampling.LANCZOS)
        if side:
            sprite=opponent_sprite(sprite) if recolor else ImageOps.mirror(sprite)
        x,y=index%columns*width,offset+index//columns*height
        pages[page].paste(sprite,(x+(width-w)//2,y+height-1-h))
        frames.append((page,x,y,width,height))
    return frames


compact_bars_page=new_page(192,48+4*28)
compact_hud_bars=[(compact_bars_page,x,y,w,h) for _,x,y,w,h in hud_bars]
field_leaders=[]
compact_field_leaders=[]
for page,columns,w,h,target in ((bars_page,5,44,41,field_leaders),
                                 (compact_bars_page,6,32,28,compact_field_leaders)):
    pages[page]=Image.new("RGBA",(columns*w,48+((24+columns-1)//columns)*h))
    pages[page].paste(bars,(0,0))
    for side in range(2):
        target.append([leader_row(page,poses,(w,h),48,columns,12*side+4*row,side,True)
                       for row,poses in enumerate(leader_poses)])

first_signature_page=len(pages)
signature_leaders=[]
signature_sources=[]
for name in ("shu-commanders","three-kingdoms-commanders","veteran-commanders"):
    for poses in separated_poses(image(name),4,4):
        signature_sources.append(poses)
        page=new_page(4*40,2*40)
        signature_leaders.append([leader_row(page,poses,(40,40),0,4,4*side,side)
                                  for side in range(2)])

relic_page=new_page(192,192)
relics=image("relic-equipment")
for i in range(9):
    x,y=(i%3)*64,(i//3)*64
    sprite=crop_cell(relics,i%3,i//3,3,3)
    sprite.thumbnail((60,60),Image.Resampling.LANCZOS)
    pages[relic_page].paste(sprite,(x+(64-sprite.width)//2,y+(64-sprite.height)//2))
    equipment.append((relic_page,x,y,64,64))

# Keep ordinary portraits in small sheets during battle, so there is room for
# both armies, the backdrop, and the animated commander sets.
battle_portraits = []
for i, (page, px, py, w, h) in enumerate(portraits):
    position = i % 6
    if position == 0:
        rows = (min(6, len(portraits)-i)+2)//3
        battle_face_page = new_page(3*portrait_size, rows*portrait_size)
    x, y = (position%3)*portrait_size, (position//3)*portrait_size
    pages[battle_face_page].paste(pages[page].crop((px,py,px+w,py+h)), (x,y))
    battle_portraits.append((battle_face_page,x,y,w,h))

# The ruler cards occupy much more screen space than their tactical portraits.
detail_face_size = 120
portrait_detail_filter = ImageFilter.UnsharpMask(radius=0.65, percent=42, threshold=3)
ruler_portrait_page = new_page(2*detail_face_size, 2*detail_face_size)
ruler_portraits = []
for i, face in enumerate(portrait_cells("portraits", 3, 2, detail_face_size)):
    if i >= 3:
        break
    x, y = (i%2)*detail_face_size, (i//2)*detail_face_size
    pages[ruler_portrait_page].paste(face.filter(portrait_detail_filter), (x,y))
    ruler_portraits.append((ruler_portrait_page,x,y,detail_face_size,detail_face_size))

# Keep the higher resolution hall on the ruler screen, where the camera is static.
choice_cols, choice_rows = 3, 2
choice_tile_w, choice_tile_h = 256, 216
choice_hall = (war_hall[0],len(pages),choice_cols,choice_rows,choice_tile_w,choice_tile_h)
room = ImageOps.fit(image("war-hall"), (choice_cols*choice_tile_w,choice_rows*choice_tile_h),
                    Image.Resampling.LANCZOS)
for y in range(0,room.height,choice_tile_h):
    for x in range(0,room.width,choice_tile_w):
        whole(room.crop((x,y,x+choice_tile_w,y+choice_tile_h)))

# Match horizontal samples to the display and let the renderer step per row.
# Compact rows leave room for troop frames and portraits on the ESPP4-A.
field_cols, field_rows = 4, 1
field_tile_w, field_tile_h = 256, 176
native_fields = []
for compact, name in zip((*battlefields, siege), field_sources):
    native_fields.append((compact[0],len(pages),field_cols,field_rows,field_tile_w,field_tile_h))
    field_width = field_cols*field_tile_w
    panorama = field_panorama(name,(field_width,field_tile_h))
    for x in range(0,field_width,field_tile_w):
        whole(panorama.crop((x,0,x+field_tile_w,field_tile_h)))

first_detail_menu_page = len(pages)
logo_width = 448
logo_detail = logo_source.resize((logo_width,round(logo_width*logo_source.height/logo_source.width)),
                                 Image.Resampling.LANCZOS)
logo_tiles = [whole(logo_detail.crop((x,0,x+logo_width//2,logo_detail.height)))
              for x in (0,logo_width//2)]

detailed_portraits=[]
officer_face_size=96
for name,columns,rows in (*portrait_sheets,("historical-talents",3,2),("field-leader-portraits",3,1)):
    for face in portrait_cells(name,columns,rows,officer_face_size):
        position=len(detailed_portraits)%4
        if position==0:
            portrait_detail_page=new_page(2*officer_face_size,2*officer_face_size)
        x,y=(position%2)*officer_face_size,(position//2)*officer_face_size
        pages[portrait_detail_page].paste(face.filter(portrait_detail_filter),(x,y))
        detailed_portraits.append((portrait_detail_page,x,y,officer_face_size,officer_face_size))
assert len(detailed_portraits)==len(portraits)

first_detail_actor_page = len(pages)
large_troops=[]
large_tactics=[]
large_frame=64
for side in range(2):
    march, actions = [], []
    for ordinary, tactical in zip(troop_sources,tactic_sources):
        page=new_page(4*large_frame,3*large_frame)
        march.append(troop_frames(page,ordinary,large_frame,4,0,side))
        actions.append(troop_frames(page,tactical,large_frame,4,6,side))
    large_troops.append(march)
    large_tactics.append(actions)
large_leaders=[]
for poses in signature_sources:
    large_leaders.append([leader_row(new_page(4*large_frame,large_frame),poses,
                                     (large_frame,large_frame),0,4,0,side) for side in range(2)])

# The center is largely covered by the map and HUD. Keep the two visible
# margins at full vertical resolution and use a compact sample for the gaps.
map_hall_width,map_hall_height=896,480
hall=ImageOps.fit(image("war-hall"),(map_hall_width,map_hall_height),Image.Resampling.LANCZOS)
map_hall=tiled_backdrop(war_hall,hall.resize((224,120),Image.Resampling.LANCZOS),1,1)
hall_side_span,hall_side_texels=144,88
map_hall_sides=[]
for y in (0,240):
    strip=Image.new("RGBA",(2*hall_side_texels,240))
    for i,x in enumerate((0,map_hall_width-hall_side_span)):
        zone=hall.crop((x,y,x+hall_side_span,y+240))
        strip.paste(zone.resize((hall_side_texels,240),Image.Resampling.LANCZOS),(i*hall_side_texels,0))
    map_hall_sides.append(whole(strip))

armory_source=image("armory")
armory=whole(scaled(armory_source.copy()))
armory_width,armory_height=864,256
armory_tile_width=armory_width//4
armory_detail=(armory[0],len(pages),4,1,armory_tile_width,armory_height)
armory_large=ImageOps.fit(armory_source,(armory_width,480),Image.Resampling.LANCZOS)
armory_large=armory_large.resize((armory_width,armory_height),Image.Resampling.LANCZOS)
for x in range(0,armory_width,armory_tile_width):
    whole(armory_large.crop((x,0,x+armory_tile_width,armory_height)))

# Keep the larger menu icons out of the live battle and map working sets.
icon_detail_page=new_page(256,76)
detail_icons=status_icons.copy()
for index,x in zip((8,9,10,12,13,14),(0,44,88,132,172,212)):
    size=44 if index<=10 else 40
    detail_icons[index]=generated_icon(status_sheet,index,x,0,size,size,icon_detail_page)
for column,index in enumerate((0,1,3,4,5,6,7,15)):
    detail_icons[index]=generated_icon(status_sheet,index,column*32,44,32,32,icon_detail_page)

# Live battles retain the existing panorama as a lower-memory fallback.
sharp_fields=[]
for compact,name in zip((*battlefields,siege),field_sources):
    view = field_panorama(name,(1024,208))
    sharp_fields.append(tiled_backdrop(compact,view,4,1))

# Keep all six poses per side and the three short effects within one binding.
field_page = new_page(256,240)
mixed_motion = image("mixed-force-field-frames")
mixed_poses = [[crop_cell(mixed_motion,col,row,6,3) for col in range(6)] for row in range(3)]
mixed_frames = [
    [troop_frames(field_page,poses,40,6,(3*side+row)*6,side)
     for row,poses in enumerate(mixed_poses)]
    for side in range(2)
]
fx = image("field-effect-frames")
field_effects = []
for row in range(3):
    poses = [crop_cell(fx,col,row,4,3) for col in range(4)]
    ratio = 14 / max(max(pose.size) for pose in poses)
    effect = []
    for col,pose in enumerate(poses):
        w,h = (max(1,round(d*ratio)) for d in pose.size)
        x,y = 240,16*(4*row+col)
        top = 15-h if row==0 else (16-h)//2
        pages[field_page].paste(pose.resize((w,h),Image.Resampling.LANCZOS),(x+(16-w)//2,y+top))
        effect.append((field_page,x,y,16,16))
    field_effects.append(effect)

zhao_sheet = image("zhao-yun-field-frames")
zhao_poses = [pose for row in separated_poses(zhao_sheet,3,2) for pose in row]
zhao_page = new_page(4*48,3*48)
zhao_frames = [leader_row(zhao_page,zhao_poses,(48,48),0,4,6*side,side) for side in range(2)]

# Record the lowest rasterized pixel of each frame. Runtime placement uses the
# packed transparency threshold, so a pose change cannot lift the contact point.
field_contacts = set(new_sprites)
for bank in (troops, detail_troops, tactics, large_troops, large_tactics, field_leaders,
             compact_field_leaders, signature_leaders, large_leaders, mixed_frames):
    for side in bank:
        for poses in side:
            field_contacts.update(poses)
for frames in (*zhao_frames,*duels):
    field_contacts.update(frames)
foot_insets = {}
for tile in field_contacts:
    page,x,y,w,h = tile
    alpha = pages[page].crop((x,y,x+w,y+h)).getchannel("A")
    visible = alpha.point(lambda a:255 if a>=105 else 0).getbbox()
    if visible is None:
        raise ValueError(f"Empty battlefield frame {tile}")
    foot_insets[tile] = h - visible[3]

colors.insert(0,0)

output = ROOT / "resources"
output.mkdir(exist_ok=True)
spec = []
for i, page in enumerate(pages):
    alpha = page.getchannel("A").tobytes()
    if first_font_page <= i < first_detailed_page:
        packed = alpha
    else:
        if i in cover_indices:
            data=cover_indices[i].tobytes()
        else:
            indexed=page.convert("RGB").quantize(palette=palette_source,dither=Image.Dither.NONE)
            data=indexed.tobytes().translate(shared_color_indices)
        packed = bytes(p + 1 if a >= 105 else 0 for p, a in zip(data, alpha))
    (output / f"atlas-{i}.index8").write_bytes(packed)
    # These internal names also occupy space in the 16 KiB package manifest.
    spec.append(dict(path=f"assets/{i:x}.pxr", kind="index8",
                     source=f"resources/atlas-{i}.index8", width=page.width, height=page.height))
for old in output.glob("atlas-*.index8"):
    if int(old.stem.split("-")[-1])>=len(pages):
        old.unlink()
(ROOT / "resources.json").write_text(json.dumps({"assets":spec}, indent=2) + "\n")


def tile_initializer(t):
    inset = foot_insets.get(t,0)
    return "{" + ",".join(map(str,(*t,inset) if inset else t)) + "}"


lines = [
    "// Generated by tools/pack_art.py.",
    "#pragma once", "#include <array>", "#include <cstdint>",
    "namespace han::art {",
    "struct Tile {",
    "  uint8_t page,footInset; uint16_t x,y,w,h;",
    "  constexpr Tile(uint8_t page=0,uint16_t x=0,uint16_t y=0,uint16_t w=0,uint16_t h=0,uint8_t inset=0)",
    "      : page(page),footInset(inset),x(x),y(y),w(w),h(h) {}",
    "};",
    "struct Glyph {uint32_t code; uint8_t advance; Tile tile;};",
    f"inline constexpr int fontPixels = {font_px};",
    f"inline constexpr int fallbackGlyph = {chars.index('?')};",
    f"inline constexpr int pageCount = {len(pages)};",
    f"inline constexpr int firstFontPage = {first_font_page};",
    f"inline constexpr int firstDetailedPage = {first_detailed_page};",
    f"inline constexpr int pastDetailedPage = {leader_portrait_page};",
    f"inline constexpr int firstSignaturePage = {first_signature_page};",
    f"inline constexpr int firstDetailMenuPage = {first_detail_menu_page};",
    f"inline constexpr int firstDetailActorPage = {first_detail_actor_page};",
    "struct Backdrop {uint8_t compact,first,columns,rows; uint16_t w,h;};",
    "inline constexpr Backdrop detailedBackdrops[] = {" +
    ",".join("{" + ",".join(map(str,tile)) + "}" for tile in detailed_backdrops) + "};",
    "inline constexpr Backdrop choiceHall = {" + ",".join(map(str,choice_hall)) + "};",
    "inline constexpr Backdrop mapHall = {" + ",".join(map(str,map_hall)) + "};",
    f"inline constexpr int mapHallWidth = {map_hall_width};",
    f"inline constexpr int mapHallSideSpan = {hall_side_span};",
    f"inline constexpr int mapHallSideTexels = {hall_side_texels};",
    "inline constexpr Backdrop armoryDetail = {" + ",".join(map(str,armory_detail)) + "};",
    "inline constexpr Backdrop nativeFields[] = {" +
    ",".join("{" + ",".join(map(str,tile)) + "}" for tile in native_fields) + "};",
    "inline constexpr Backdrop sharpFields[] = {" +
    ",".join("{" + ",".join(map(str,tile)) + "}" for tile in sharp_fields) + "};",
    "inline constexpr std::array<uint16_t,256> palette = {" + ",".join(map(str,colors)) + "};",
]
for name,t in (("coverWide",cover_wide),("coverSquare",cover_square),("logo",logo),("siege",siege),
               ("warHall",war_hall),("cityCourtyard",city_courtyard),("armory",armory)):
    lines.append(f"inline constexpr Tile {name} = {tile_initializer(t)};")
for name,tiles in (("mapTiles",map_tiles),("compactMapTiles",compact_map_tiles),("battlefields",battlefields),
                   ("logoTiles",logo_tiles),("portraits",portraits),("battlePortraits",battle_portraits),
                   ("detailedPortraits",detailed_portraits),
                   ("rulerPortraits",ruler_portraits),("equipment",equipment),
                   ("outcomes",outcomes),("hudBars",hud_bars),("compactHudBars",compact_hud_bars),
                   ("marchBars",march_bars),("commandIcons",command_icons),("detailIcons",detail_icons),
                   ("mapProps",map_props),("mapHallSides",map_hall_sides),("plaques",plaques),("battlePanels",battle_panels),
                   ("windows",windows),("mixedForces",new_sprites)):
    lines.append(f"inline constexpr Tile {name}[{len(tiles)}] = {{" +
                 ",".join(map(tile_initializer,tiles)) + "};")
for name,dimension,banks in (("troops","[2][3][6]",troops),
                             ("mixedTroopFrames","[2][3][6]",mixed_frames),
                             ("detailTroops","[2][3][6]",detail_troops),
                             ("tacticTroops","[2][3][6]",tactics),
                             ("largeTroops","[2][3][6]",large_troops),
                             ("largeTactics","[2][3][6]",large_tactics),
                             ("marchers","[2][2][3]",marchers),
                             ("fieldLeaders","[2][3][4]",field_leaders),
                             ("compactFieldLeaders","[2][3][4]",compact_field_leaders),
                             ("signatureLeaders",f"[{len(signature_leaders)}][2][4]",signature_leaders),
                             ("largeLeaders",f"[{len(large_leaders)}][2][4]",large_leaders)):
    lines.append(f"inline constexpr Tile {name}{dimension} = {{")
    for army in banks:
        lines.append("  {")
        for row in army:
            lines.append("    {" + ",".join(map(tile_initializer,row)) + "},")
        lines.append("  },")
    lines.append("};")
for name,rows in (("battleEffects",field_effects),("zhaoYunFrames",zhao_frames)):
    lines.append(f"inline constexpr Tile {name}[{len(rows)}][{len(rows[0])}] = {{")
    for row in rows:
        lines.append("  {" + ",".join(map(tile_initializer,row)) + "},")
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
