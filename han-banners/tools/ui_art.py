"""Small bitmap props and stretchable strips for the lacquered game HUD."""

from PIL import Image, ImageDraw


def lacquer_panel(tone):
    shadow, face, well = tone
    panel = Image.new("RGBA", (140, 36))
    draw = ImageDraw.Draw(panel)
    draw.rounded_rectangle((0, 0, 139, 35), radius=4, fill="#302018")
    draw.rounded_rectangle((1, 1, 138, 34), radius=3, fill="#a77045", outline="#dfb376")
    draw.rectangle((4, 4, 135, 31), fill=shadow, outline="#513421")
    draw.rectangle((6, 6, 133, 29), fill=face)
    for y in range(8, 29):
        draw.line((7, y, 132, y), fill=well if y >= 18 else face)
    for x, dx in ((4, 1), (135, -1)):
        for y, dy in ((4, 1), (31, -1)):
            draw.line([(x, y+4*dy), (x, y), (x+5*dx, y)], fill="#d0a26d")
            draw.point((x+2*dx, y+2*dy), fill="#9d6b40")
    return panel


def progress_strips():
    track = Image.new("RGBA", (192, 12))
    draw = ImageDraw.Draw(track)
    draw.rounded_rectangle((0, 0, 191, 11), radius=4, fill="#37271d", outline="#98734c")
    draw.rounded_rectangle((2, 2, 189, 9), radius=3, fill="#201b17", outline="#50392a")
    draw.line((8, 1, 183, 1), fill="#e4ba81")
    draw.line((8, 10, 183, 10), fill="#69492f")
    yield track
    for highlight, face, shade in (
        ("#c4ebbc", "#4eae81", "#27604b"),
        ("#f8b688", "#c66c4c", "#74352b"),
        ("#f7df9c", "#c79247", "#855331"),
        ("#c4e7ef", "#6aa6b4", "#3d6671"),
    ):
        strip = Image.new("RGBA", (192, 12))
        draw = ImageDraw.Draw(strip)
        draw.rounded_rectangle((0, 0, 191, 11), radius=3, fill=face, outline=shade)
        draw.line((5, 2, 186, 2), fill=highlight, width=2)
        draw.rectangle((4, 8, 187, 10), fill=shade)
        yield strip
