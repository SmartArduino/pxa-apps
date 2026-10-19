"""Small, deterministic network and native decoder fixtures (Pillow for tests only)."""
from io import BytesIO
from pathlib import Path
from functools import cache
from PIL import Image, ImageDraw


@cache
def projection_image(step=3):
    """Non-scannable 180px marker; base64 approaches the 4096-char DOM limit."""
    import random, struct, zlib
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack(
            '>I', zlib.crc32(kind + data) & 0xffffffff)
    rng = random.Random(0x505841)
    colors = [(32, 64, 192), (240, 128, 32), (240, 240, 240), (48, 48, 48)]
    cells = (180 + step - 1) // step
    grid = [[rng.choice(colors) for _ in range(cells)] for _ in range(cells)]
    raw = b''.join(b'\0' + b''.join(bytes(grid[y // step][x // step])
                   for x in range(180)) for y in range(180))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB',
            180, 180, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


@cache
def fixtures():
    image = Image.new('RGBA', (320, 180))
    for y in range(image.height):
        for x in range(image.width):
            image.putpixel((x, y), (30+x*120//320, 95+y*100//180, 210-y*60//180, 255))
    draw = ImageDraw.Draw(image)
    draw.ellipse((215, 18, 274, 77), fill=(255, 222, 116, 255))
    draw.polygon([(0, 180), (95, 55), (200, 180)], fill=(30, 100, 110, 255))
    draw.polygon([(100, 180), (230, 90), (320, 180)], fill=(52, 141, 112, 255))
    draw.rounded_rectangle((112, 65, 194, 120), radius=12, fill=(255, 255, 255, 0))
    result = {}
    for name, picture, fmt, options in [
        ('alpha.png', image, 'PNG', {}),
        ('palette.png', image.convert('P', palette=Image.Palette.ADAPTIVE, colors=32), 'PNG', {}),
        ('gray.png', image.convert('L'), 'PNG', {}),
        ('photo.jpg', image.convert('RGB'), 'JPEG', {'quality': 88}),
        ('gray.jpg', image.convert('L'), 'JPEG', {'quality': 88}),
        ('progressive.jpg', image.convert('RGB'), 'JPEG', {'progressive': True}),
    ]:
        data = BytesIO(); picture.save(data, format=fmt, **options)
        result[name] = data.getvalue()
        if fmt == 'PNG': result[name+'.rgba'] = picture.convert('RGBA').tobytes()
        if fmt == 'JPEG':
            # An independent reference catches channel order errors in the native decoder.
            result[name+'.rgb'] = Image.open(BytesIO(data.getvalue())).convert('RGB').resize(
                (64, 36), Image.Resampling.BOX).tobytes()
    tall = Image.new('RGBA', (320, 900), (33, 143, 174, 255))
    ImageDraw.Draw(tall).rectangle((0, 300, 319, 599), fill=(233, 172, 63, 255))
    data = BytesIO(); tall.save(data, format='PNG'); result['tall.png'] = data.getvalue()
    result['broken.png'] = result['alpha.png'][:-16]
    result['unsupported.svg'] = b'<svg xmlns="http://www.w3.org/2000/svg" width="10" height="10"/>'
    frames = []
    for color in [(220, 45, 65), (35, 105, 220), (30, 165, 90)]:
        picture = Image.new('RGB', (160, 96), color)
        ImageDraw.Draw(picture).rectangle((12, 12, 45, 45), fill='white')
        frames.append(picture)
    data = BytesIO()
    frames[0].save(data, format='GIF', save_all=True, append_images=frames[1:],
                   duration=[220, 340, 460], loop=0, disposal=2)
    result['animation.gif'] = result['animation.bin'] = data.getvalue()
    result['broken.gif'] = data.getvalue()[:-10]
    data = BytesIO(); frames[0].save(data, format='GIF'); result['still.gif'] = data.getvalue()
    alpha_frames = []
    for index in range(4):
        picture = Image.new('RGBA', (128, 96), (0, 0, 0, 0))
        ImageDraw.Draw(picture).rectangle((12 + index * 18, 20, 35 + index * 18, 65),
                                         fill=(240, 100, 30, 255))
        alpha_frames.append(picture)
    data = BytesIO()
    alpha_frames[0].save(data, format='GIF', save_all=True, append_images=alpha_frames[1:],
                        duration=300, loop=0, disposal=[1, 2, 3, 1], transparency=0)
    result['alpha.gif'] = data.getvalue()
    return result


if __name__ == '__main__':
    import sys
    directory = Path(sys.argv[1]); directory.mkdir(parents=True, exist_ok=True)
    for name, data in fixtures().items(): (directory/name).write_bytes(data)
