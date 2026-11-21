"""Extract complete silhouettes from a generated sheet without borrowing neighbors."""
from PIL import Image
import math


def fighter_frames(image, columns=8, rows=4, scale=.5):
    image = image.convert('RGBA')
    width, height = image.size
    alpha = image.getchannel('A').tobytes()
    visited = bytearray(len(alpha))
    components = []
    for seed, value in enumerate(alpha):
        if value < 96 or visited[seed]:
            continue
        visited[seed] = 1
        pending = [seed]
        pixels = []
        left = right = seed % width
        top = bottom = seed // width
        while pending:
            index = pending.pop()
            pixels.append(index)
            x, y = index % width, index // width
            left, right = min(left, x), max(right, x)
            top, bottom = min(top, y), max(bottom, y)
            for other in (index - width, index + width,
                          index - 1 if x else -1,
                          index + 1 if x + 1 < width else -1):
                if 0 <= other < len(alpha) and not visited[other] and alpha[other] >= 96:
                    visited[other] = 1
                    pending.append(other)
        components.append((pixels, (left, top, right + 1, bottom + 1)))
    count=columns*rows
    cw, ch = width / columns, height / rows
    bodies = []
    for frame in range(count):
        candidates = [c for c in components if
                      int((c[1][0] + c[1][2]) / 2) // cw == frame % columns and
                      int((c[1][1] + c[1][3]) / 2) // ch == frame // columns]
        body = max(candidates, key=lambda c: len(c[0]))
        assert len(body[0]) > 1000, ('Missing complete fighter', frame)
        bodies.append(body)
    owned = [[] for _ in bodies]
    body_owners = {id(body[0]): i for i, body in enumerate(bodies)}
    for pixels, box in components:
        def distance(body):
            b = body[1]
            return max(0, b[0] - box[2], box[0] - b[2]) ** 2 + max(0, b[1] - box[3], box[1] - b[3]) ** 2
        owner = body_owners.get(id(pixels))
        if owner is None:
            owner = min(range(count), key=lambda i: (distance(bodies[i]),
                        abs(sum(bodies[i][1][::2])-sum(box[::2])) + abs(sum(bodies[i][1][1::2])-sum(box[1::2]))))
        if distance(bodies[owner]) <= 16:
            owned[owner].extend(pixels)
    frames, offsets = [], []
    for frame, pixels in enumerate(owned):
        mask = bytearray(len(alpha))
        for index in pixels:
            mask[index] = alpha[index]
        selected = image.copy()
        selected.putalpha(Image.frombytes('L', image.size, bytes(mask)))
        box = selected.getbbox()
        # Sample on the original cell's native pixel grid, including any overhang.
        ox, oy = frame % columns * cw, frame // columns * ch
        left, top = math.floor((box[0]-ox)*scale), math.floor((box[1]-oy)*scale)
        right, bottom = math.ceil((box[2]-ox)*scale), math.ceil((box[3]-oy)*scale)
        extent = (ox+left/scale, oy+top/scale, ox+right/scale, oy+bottom/scale)
        frames.append(selected.transform((right-left, bottom-top), Image.Transform.EXTENT, extent, Image.Resampling.NEAREST))
        offsets.append((left, top))
    return frames, offsets
