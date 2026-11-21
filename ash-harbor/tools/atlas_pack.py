"""Pack native-size UI tiles with a transparent pixel between neighboring tiles."""
from PIL import Image


def pack_tiles(items):
    bins, refs = [], {}
    for name, image in sorted(items, key=lambda item: (-item[1].height, -item[1].width)):
        width, height = image.width + 1, image.height + 1
        assert width <= 256 and height <= 256, name
        chosen = None
        for index, bin in enumerate(bins):
            for row in bin['rows']:
                if row[1] >= height and row[2] + width <= 256:
                    chosen = index, row
                    break
            if chosen:
                break
        if chosen is None:
            for index, bin in enumerate(bins):
                if bin['bottom'] + height <= 256:
                    row = [bin['bottom'], height, 0]
                    bin['rows'].append(row)
                    bin['bottom'] += height
                    chosen = index, row
                    break
        if chosen is None:
            row = [0, height, 0]
            bins.append({'image': Image.new('RGBA', (256, 256)), 'rows': [row], 'bottom': height, 'right': 0})
            chosen = len(bins) - 1, row
        index, row = chosen
        x, y = row[2], row[0]
        bins[index]['image'].paste(image, (x, y))
        refs[name] = [index, x, y, image.width, image.height]
        row[2] += width
        bins[index]['right'] = max(bins[index]['right'], row[2])
    return [bin['image'].crop((0, 0, bin['right'], bin['bottom'])) for bin in bins], refs
