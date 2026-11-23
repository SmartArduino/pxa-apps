"""Select map-grid berths on shores connected to navigable water."""
from collections import deque


def neighbours(x, y):
    return ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1))


def edge_water(bounds, wet):
    left, top, right, bottom = bounds
    edges = [(x, y) for x in range(left, right) for y in (top, bottom - 1)]
    edges.extend((x, y) for y in range(top, bottom) for x in (left, right - 1))
    reached = {point for point in edges if wet(*point)}
    pending = deque(reached)
    while pending:
        x, y = pending.popleft()
        for u, v in neighbours(x, y):
            if left <= u < right and top <= v < bottom and (u, v) not in reached and wet(u, v):
                reached.add((u, v))
                pending.append((u, v))
    return reached


def shoreline_berth(x, y, reach, size, wet, connected, aspect, safe):
    width, height = size
    candidates = []
    for v in range(max(0, int(y) - reach), min(height, int(y) + reach + 1)):
        for u in range(max(0, int(x) - reach), min(width, int(x) + reach + 1)):
            if (u, v) not in connected or not safe(u, v):
                continue
            if any(0 <= a < width and 0 <= b < height and not wet(a, b) for a, b in neighbours(u, v)):
                candidates.append(((aspect * (u + .5 - x)) ** 2 + (v + .5 - y) ** 2, u, v))
    if not candidates:
        raise ValueError(f"No reachable shoreline at {x:.1f}, {y:.1f}")
    _, u, v = min(candidates)
    return u, v


def settlement_mark(x, y, size, wet, aspect):
    """Keep a coastal settlement symbol on land when its grid cell straddles the shore."""
    width, height = size
    u, v = int(x), int(y)
    if not (0 <= u < width and 0 <= v < height and wet(u, v)):
        return x, y
    land = [((aspect * (a + .5 - x)) ** 2 + (b + .5 - y) ** 2, a, b)
            for a in range(max(0, u - 1), min(width, u + 2))
            for b in range(max(0, v - 1), min(height, v + 2)) if not wet(a, b)]
    if not land:
        return x, y
    _, a, b = min(land)
    return a + .5, b + .5
