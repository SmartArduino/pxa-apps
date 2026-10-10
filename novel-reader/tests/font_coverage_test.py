"""Check font coverage and actual ink, beyond lossless text pagination."""
import argparse
import json
from pathlib import Path
import unicodedata

from fontTools.ttLib import TTFont
from PIL import ImageFont

root = next(p for p in Path(__file__).resolve().parents if (p/'tools/app.sh').exists())
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--font',type=Path,default=root/'factory/base/system/fonts/noto_sans_cjk_common.ttf')
parser.add_argument('--book',type=Path)
args = parser.parse_args()
opening = '第一回 靈根育孕源流出 心性修持大道生 混沌未分天地亂，茫茫渺渺無人見。自從盤古破鴻濛，開闢從茲清濁辨。'
text = opening + (args.book.read_text(encoding='utf-8') if args.book else '')
characters = sorted(c for c in set(text) if not c.isspace() and
                    unicodedata.category(c) not in ('Cc','Cf','Mn'))
cmap = TTFont(args.font).getBestCmap()
missing = [c for c in characters if ord(c) not in cmap]
assert not missing, 'Missing glyphs: ' + ' '.join(f'{c}(U+{ord(c):04X})' for c in missing)
for size in (12,16,28,56):
    font = ImageFont.truetype(str(args.font),size)
    blank = [c for c in characters if font.getmask(c).getbbox() is None]
    assert not blank, (size,'Mapped but blank glyphs',blank)
    # Uncovered characters must have a visible missing-glyph mark instead of
    # being silently swallowed by an empty .notdef outline.
    assert 0x2b820 not in cmap
    assert font.getmask(chr(0x2b820)).getbbox() is not None
print(json.dumps({'characters':len(characters),'sizes':[12,16,28,56],
                  'missing':0,'blank':0,'visible_missing_glyph':True},ensure_ascii=False))
