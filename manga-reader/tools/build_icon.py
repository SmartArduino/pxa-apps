"""Original comic-book icon; Pillow is only used during authoring."""
from pathlib import Path
from PIL import Image, ImageDraw
image=Image.new('RGB',(512,512),'#f6f5f0')
d=ImageDraw.Draw(image)
d.rounded_rectangle((48,48,464,464),radius=90,fill='#27664d')
d.rounded_rectangle((117,117,395,388),radius=18,fill='#fffefa')
d.rounded_rectangle((138,140,246,248),radius=7,fill='#9bc4b1')
d.rounded_rectangle((263,140,374,248),radius=7,fill='#d3dfca')
d.rounded_rectangle((138,265,374,362),radius=7,fill='#dce7e1')
d.ellipse((164,161,221,202),fill='#fffefa')
d.polygon([(202,194),(216,216),(184,199)],fill='#fffefa')
d.line((143,408,365,408),fill='#9bc4b1',width=10)
image.save(Path(__file__).resolve().parents[1]/'assets/icon.png',optimize=True)
