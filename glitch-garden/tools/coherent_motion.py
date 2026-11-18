# SPDX-License-Identifier: MIT
"""Bake continuous cutout motion from one generated character texture.

All poses use the same paint and outline. A premultiplied, supersampled mesh
keeps the texture attached to the body; no frame blending or redraw is used.
The ground is the deformation origin, including the jump's landing pose.
"""
import math
from PIL import Image

SCALE = 4
GROUND = 50.5
ENEMIES = {'dustimp', 'copperbeetle', 'armorsnail', 'swiftmoth'}


def anchor_texture(cell):
    image = Image.new('RGBA', (256, 256))
    image.alpha_composite(cell.resize((176, 176), Image.Resampling.LANCZOS), (40, 40))
    alpha = image.getchannel('A')
    values = list(alpha.getdata()); mass = sum(values)
    center = sum((i % 256) * a for i, a in enumerate(values)) / max(1, mass)
    box = alpha.point(lambda a: 255 if a >= 128 else 0).getbbox()
    foot = box[3] - 1
    matrix = (1, 0, center - 32 * SCALE, 0, 1, foot - GROUND * SCALE)
    return image.convert('RGBa').transform((256, 256), Image.Transform.AFFINE,
                                         matrix, Image.Resampling.BICUBIC)


def parameters(name, state, phase):
    """body lean, breath, squash, lift, opacity (source-canvas pixels)."""
    angle = math.tau * phase
    enemy = name in ENEMIES
    if state == 'idle':
        sway = (.30 if name == 'armorsnail' else .55 if enemy else .75)
        breath = .35 if enemy else .28
        return sway * math.sin(angle), breath * (1 - math.cos(angle)), 1., 0., 1.
    if state == 'action':
        pulse = math.sin(math.pi * phase) ** 2
        # The root stays planted while the head recoils and returns to rest.
        lean = (-2.0 if not enemy else -1.5) * math.sin(angle) * pulse
        if name in {'dewflower', 'cloverling'}: lean *= .55
        return lean, -.6 * pulse, 1 - .035 * pulse, 0., 1.
    if state == 'hurt':
        pulse = math.sin(math.pi * phase) ** 2
        return (1.5 if enemy else -1.5) * pulse, -.3 * pulse, 1 - .04 * pulse, 0., 1.
    if state == 'jump':
        arc = math.sin(math.pi * phase)
        return 1.5 * math.sin(math.tau * phase), 0., 1 + .06 * arc, 12 * arc, 1.
    # A single finite collapse, rather than looping/re-enlarging a small pose.
    ease = phase * phase * (3 - 2 * phase)
    return 3 * ease, 0., max(.12, 1 - .88 * ease), 0., (1 - ease) ** .7


def pose(texture, name, state, phase):
    lean, breath, squash, lift, opacity = parameters(name, state, phase)
    def source(x, y):
        x /= SCALE; y /= SCALE
        original_y = GROUND + (y + lift - GROUND) / squash
        h = max(0., min(1., (GROUND - original_y) / 32))
        # A smooth root-to-head bend; both displacement and slope are zero at
        # the ground, so AA does not move the planted leaf/foot boundary.
        weight = h * h * (3 - 2 * h)
        original_y -= breath * weight
        original_x = x - lean * weight
        if state == 'idle' and name in {'dustimp', 'copperbeetle'}:
            # Independently swing the two feet; one is planted while the other
            # lifts. The torso's small double beat follows their weight shift.
            leg = max(0., min(1., (original_y - 43) / 5))
            leg = leg * leg * (3 - 2 * leg)
            side = math.tanh((x - 32) / 2)
            step = math.sin(math.tau * phase) * side
            original_x -= 1.25 * step * leg
            original_y += 1.6 * max(0., step) * leg
            original_y += .35 * (1 - math.cos(2 * math.tau * phase)) * (1 - leg)
        elif state == 'idle' and name == 'armorsnail':
            foot = max(0., min(1., (original_y - 44) / 5))
            original_y += .8 * (1 + math.sin((x - 15) * math.tau / 18 - math.tau * phase)) * foot
        elif name == 'swiftmoth' and state in {'idle', 'action'}:
            # Keep the face/body stable; flap the wing fan about its left hinge.
            wing = max(0., min(1., (x - 30) / 10))
            wing = wing * wing * (3 - 2 * wing)
            wing *= max(0., min(1., (original_y - 28) / 5)) * max(0., min(1., (48 - original_y) / 5))
            fold = .5 - .5 * math.cos(math.tau * phase)
            original_x += (x - 30) * .3 * fold * wing
            original_y += (original_y - 34) * .12 * fold * wing
        return (original_x * SCALE, original_y * SCALE)
    mesh = []
    for y in range(0, 256, 16):
        for x in range(0, 256, 16):
            quad = (*source(x, y), *source(x, y + 16),
                    *source(x + 16, y + 16), *source(x + 16, y))
            mesh.append(((x, y, x + 16, y + 16), quad))
    result = texture.transform((256, 256), Image.Transform.MESH, mesh,
                               Image.Resampling.BICUBIC)
    result = result.resize((64, 64), Image.Resampling.LANCZOS).convert('RGBA')
    if opacity < 1:
        result.putalpha(result.getchannel('A').point(lambda a: round(a * opacity)))
    return result


def clip(texture, name, state):
    # Cycles exclude the duplicate endpoint; finite clips include their landing
    # or transparent endpoint. Attack/hurt use the same rest texture as idle.
    denominator = 11 if state in {'jump', 'defeat'} else 12
    frames = [pose(texture, name, state, i / denominator) for i in range(12)]
    boxes = [im.getchannel('A').getbbox() for im in frames]
    boxes = [box for box in boxes if box]
    box = (max(0, min(b[0] for b in boxes) - 1), max(0, min(b[1] for b in boxes) - 1),
           min(64, max(b[2] for b in boxes) + 1), min(64, max(b[3] for b in boxes) + 1))
    return frames, box
