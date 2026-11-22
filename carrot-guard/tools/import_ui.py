"""Slice generated UI assets, retaining glyph padding and scalable skin frames."""
from pathlib import Path
from PIL import Image


def cells(path, columns, rows):
    atlas = Image.open(path).convert('RGBA')
    for row in range(rows):
        for column in range(columns):
            yield atlas.crop((round(column * atlas.width / columns),
                              round(row * atlas.height / rows),
                              round((column + 1) * atlas.width / columns),
                              round((row + 1) * atlas.height / rows)))


def digit_cells(path):
    """Generated atlas rows can drift; split in real empty space, not ink."""
    atlas = Image.open(path).convert('RGBA')
    alpha = atlas.getchannel('A')
    pixels = alpha.load()
    bands, start = [], None
    for y in range(atlas.height):
        ink = sum(pixels[x, y] > 192 for x in range(atlas.width)) > 12
        if ink and start is None:
            start = y
        elif not ink and start is not None:
            bands.append((start, y))
            start = None
    if start is not None:
        bands.append((start, atlas.height))
    if len(bands) != 4:
        raise ValueError(f'Digit atlas needs four complete ink rows, got {bands}')
    cuts = [0] + [(a[1] + b[0]) // 2 for a, b in zip(bands, bands[1:])] + [atlas.height]
    for top, bottom in zip(cuts, cuts[1:]):
        for column in range(4):
            yield atlas.crop((round(column * atlas.width / 4), top,
                              round((column + 1) * atlas.width / 4), bottom))


def trim(image):
    box = image.getchannel('A').point(lambda a: 255 if a > 64 else 0).getbbox()
    if not box:
        raise ValueError('Empty generated UI cell')
    pad = max(3, round(max(image.size) * .012))
    return image.crop((max(0, box[0] - pad), max(0, box[1] - pad),
                       min(image.width, box[2] + pad), min(image.height, box[3] + pad)))


def guide_tracking(image, count, word_break=None):
    """Reposition generated glyphs; keep their original pixels and baselines."""
    alpha = image.getchannel('A')
    ink = alpha.point(lambda a: 255 if a > 160 else 0)
    box = ink.getbbox()
    if not box:
        return image
    left, _, right, _ = box
    advance = (right - left) / count
    columns = [sum(ink.crop((x, 0, x + 1, image.height)).getdata()) for x in range(image.width)]
    cuts = [0]
    for index in range(1, count):
        ideal = left + advance * index
        start, end = max(cuts[-1] + 1, round(ideal - advance * .22)), min(image.width - 1, round(ideal + advance * .22))
        cut = min(range(start, end + 1), key=lambda x: (columns[x], abs(x - ideal)))
        cuts.append(cut)
    cuts.append(image.width)
    parts = []
    for start, end in zip(cuts, cuts[1:]):
        part = image.crop((start, 0, end, image.height))
        bounds = part.getchannel('A').point(lambda a: 255 if a > 64 else 0).getbbox()
        if bounds:
            parts.append(part.crop((max(0, bounds[0] - 1), 0, min(part.width, bounds[2] + 1), part.height)))
    gap = max(2, round((box[3] - box[1]) * .055))
    word_gap = round((box[3] - box[1]) * .20) if word_break else gap
    width = sum(p.width for p in parts) + gap * (len(parts) - 1) + (word_gap - gap if word_break else 0)
    output = Image.new('RGBA', (width, image.height))
    x = 0
    for index, part in enumerate(parts):
        output.alpha_composite(part, (x, 0))
        x += part.width + (word_gap if index + 1 == word_break else gap)
    return output


def import_ui(root):
    art, output = root / 'art', root / 'assets'
    result = {'labels': {}, 'digits': None, 'skins': []}
    label_sets = [
        ('ui-controls-v2.png', 4, 5, [
            'start', 'levels', 'help', 'resume', 'retry', 'map', 'upgrade', 'sell',
            'wave', 'freeze', 'prepare', 'win', 'lose', 'sound', 'pause', 'next',
            'max', 'poor', 'clear', 'close']),
        ('ui-help-v2.png', 3, 2, [
            'hint_build', 'hint_coins', 'hint_upgrade', 'hint_clear', 'hint_combo', 'hint_goal']),
        ('ui-tower-names-v3.png', 3, 1, ['pea_name', 'ice_name', 'sun_name'])]
    for source, columns, rows, names in label_sets:
        if not (art / source).exists():
            continue
        for name, image in zip(names, cells(art / source, columns, rows)):
            if name == 'close':continue # Unused source-atlas sample, not a runtime label.
            image = trim(image)
            if source == 'ui-help-v2.png':
                image = guide_tracking(image, 7 if name == 'hint_coins' else 8,
                                       4 if name == 'hint_combo' else None)
            # Retain enough detail for 24–42px UI while bounding live decoded
            # guide/menu textures alongside the battlefield and turret frames.
            image.thumbnail((400, 64), Image.Resampling.LANCZOS)
            image.save(output / (name + '.png'), optimize=True)
            result['labels'][name] = (name, *image.size)

    if (art / 'ui-digits-v2.png').exists():
        result['digits'] = []
        widths = [64] * 10 + [40, 64, 64, 56, 32, 24]
        images = [trim(image) for image in digit_cells(art / 'ui-digits-v2.png')]
        cap = min(60, int(min(56 * image.height / image.width for image in images[:10])))
        for index, image in enumerate(images):
            width = widths[index]
            if index < 10:
                image = image.resize((round(image.width * cap / image.height), cap), Image.Resampling.LANCZOS)
            else:
                image.thumbnail((width - 8, 60), Image.Resampling.LANCZOS)
            glyph = Image.new('RGBA', (width, 80))
            top = round(40 - image.height / 2) if index in (11, 12, 13) else 72 - image.height
            glyph.alpha_composite(image, ((width - image.width) // 2, top))
            glyph.save(output / f'n{index}.png', optimize=True)
            result['digits'].append((f'n{index}', width, 80))

    if (art / 'ui-kit-v1.png').exists():
        folder = output / 'ui'
        folder.mkdir(exist_ok=True)
        for index, image in enumerate(cells(art / 'ui-kit-v1.png', 4, 2)):
            image = trim(image)
            if index >= 6:
                image.thumbnail((72, 72), Image.Resampling.LANCZOS)
                image.save(output / ('star_on.png' if index == 6 else 'star_off.png'), optimize=True)
                continue
            image.thumbnail((128, 44) if index < 5 else (96, 96), Image.Resampling.LANCZOS)
            image.save(folder / f's{index}.png', optimize=True)
            edge = max(6, min(image.size) // 4)
            result['skins'].append((f'ui/s{index}', *image.size, edge))
    return result
