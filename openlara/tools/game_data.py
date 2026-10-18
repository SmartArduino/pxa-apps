"""Shared TR1 catalog and fixed-format data checks for import and validation."""
from pathlib import Path
import hashlib
import re
import struct

ROOT = Path(__file__).resolve().parents[1]
LEVEL_CAPACITY = 6 * 1024 * 1024
COUNTS = ('tiles', 'rooms', 'models', 'meshes', 'static_meshes', 'sprite_sequences',
          'sound_sources', 'boxes', 'textures', 'sprites', 'items', 'cameras',
          'camera_frames', 'sound_offsets')
SECTIONS = ('palette', 'lightmap', 'tiles', 'rooms', 'floors', 'meshes', 'mesh_offsets',
            'anims', 'states', 'ranges', 'commands', 'nodes', 'frames', 'models',
            'static_meshes', 'textures', 'sprites', 'sprite_sequences', 'cameras',
            'sound_sources', 'boxes', 'overlaps', 'zone00', 'zone01', 'zone02',
            'zone10', 'zone11', 'zone12', 'animated_textures', 'items',
            'camera_frames', 'sound_map', 'sound_infos', 'sound_data', 'sound_offsets')


def catalog():
    entries = []
    for match in re.finditer(r'OL_LEVEL\((\d+),\s*"([^"]+)",\s*"([^"]+)",\s*(\d+)\)',
                             (ROOT / 'levels.def').read_text()):
        ident, file, name, kind = match.groups()
        entries.append(dict(id=int(ident), file=file, name=name, kind=int(kind)))
    if [e['id'] for e in entries] != list(range(21)):
        raise ValueError('invalid TR1 level catalog')
    return entries


def fingerprint(path):
    digest=hashlib.sha256()
    with Path(path).open('rb') as source:
        while chunk:=source.read(1024*1024):
            digest.update(chunk)
    return dict(bytes=Path(path).stat().st_size,sha256=digest.hexdigest())


def inspect_pkd(path):
    data = Path(path).read_bytes()
    if len(data) < 172 or data[:4] != b'GBA ':
        raise ValueError(f'{path}: invalid fixed PKD header')
    if len(data) > LEVEL_CAPACITY:
        raise ValueError(f'{path}: {len(data)} bytes exceeds the {LEVEL_CAPACITY}-byte level arena')
    counts = dict(zip(COUNTS, struct.unpack_from('<14H', data, 4)))
    offsets = dict(zip(SECTIONS, struct.unpack_from('<35I', data, 32)))
    for name, value in offsets.items():
        if value < 172 or value > len(data) or value % 4:
            raise ValueError(f'{path}: invalid {name} offset {value}')
    for name, limit in dict(rooms=139, models=191, meshes=512, static_meshes=50,
                            boxes=1024, textures=1536, sprites=180, items=255,
                            cameras=64, sound_offsets=256).items():
        if counts[name] > limit:
            raise ValueError(f'{path}: {name}={counts[name]} exceeds {limit}')
    if offsets['items'] + counts['items'] * 12 > len(data):
        raise ValueError(f'{path}: truncated item records')
    types = {}
    for i in range(counts['items']):
        item_type, room = struct.unpack_from('<BB', data, offsets['items'] + 12 * i)
        if room >= counts['rooms']:
            raise ValueError(f'{path}: item {i} has invalid room {room}')
        types[str(item_type)] = types.get(str(item_type), 0) + 1
    return {**fingerprint(path), 'counts': counts, 'item_types': types}


def inspect_phd(path):
    """Walk the original PC layout before publishing media or allocating a Guest level."""
    data = Path(path).read_bytes()
    if len(data) > LEVEL_CAPACITY:
        raise ValueError(f'{path}: PHD exceeds the {LEVEL_CAPACITY}-byte input limit')
    pos = 0
    counts = {}

    def take(n):
        nonlocal pos
        if n < 0 or n > len(data)-pos:
            raise ValueError(f'{path}: truncated PHD at {pos}, need {n} bytes')
        start = pos
        pos += n
        return start

    def number(fmt='<I'):
        return struct.unpack_from(fmt, data, take(struct.calcsize(fmt)))[0]

    def array(name, width, fmt='<I', limit=None):
        n = number(fmt)
        if limit is not None and n > limit:
            raise ValueError(f'{path}: {name}={n} exceeds {limit}')
        start = take(n*width)
        counts[name] = n
        return start, n

    if number() != 0x20:
        raise ValueError(f'{path}: expected original PC TR1 .PHD format (0x20)')
    array('tiles', 65536, limit=256)
    take(4)
    rooms = number('<H')
    if not 1 <= rooms <= 256:
        raise ValueError(f'{path}: invalid room count {rooms}')
    counts['rooms'] = rooms
    for i in range(rooms):
        info = struct.unpack_from('<5i', data, take(20))
        start = pos
        array('room_vertices', 8, '<H')
        array('room_quads', 10, '<H')
        array('room_triangles', 8, '<H')
        array('room_sprites', 4, '<H')
        if info[4]*2 != pos-start:
            raise ValueError(f'{path}: inconsistent room {i} geometry length')
        array('room_portals', 32, '<H')
        z, x = number('<H'), number('<H')
        if not x or not z or x*z > 65536:
            raise ValueError(f'{path}: invalid room {i} sectors')
        take(x*z*8+2)
        array('room_lights', 18, '<H')
        array('room_meshes', 18, '<H')
        alternate = number('<h')
        if alternate != -1 and not 0 <= alternate < rooms:
            raise ValueError(f'{path}: invalid alternate room {alternate}')
        take(2)
    for name, width in [('floors',2),('mesh_words',2),('mesh_offsets',4),('anims',32),
                        ('states',6),('ranges',8),('commands',2),('nodes',4),('frame_words',2),
                        ('models',18),('static_meshes',32),('textures',20),('sprites',16),
                        ('sprite_sequences',8)]:
        array(name,width)
    demo = len(data) in (316138, 2873406)
    if demo:
        take(768)
    array('cameras',16)
    array('sound_sources',16)
    _, boxes = array('boxes',20)
    array('overlaps',2)
    take(boxes*6*2)
    array('animated_texture_words',2)
    start, n = array('items',22,limit=1024)
    types = {}
    for i in range(n):
        ident, room = struct.unpack_from('<Hh',data,start+i*22)
        if not 0 <= room < rooms or ident >= 256:
            raise ValueError(f'{path}: invalid item {i}')
        types[str(ident)] = types.get(str(ident),0)+1
    take(8192)
    if not demo:
        take(768)
    array('camera_frames',16,'<H')
    array('demo_bytes',1,'<H')
    take(256*2)
    array('sound_infos',8)
    array('sound_bytes',1)
    array('sound_offsets',4)
    if pos != len(data):
        raise ValueError(f'{path}: {len(data)-pos} unexpected trailing bytes')
    return {**fingerprint(path),'counts':{k:v for k,v in counts.items() if not k.startswith('room_')},
            'item_types':types,'pc_demo_layout':demo}
