from pathlib import Path
import re
root = Path(__file__).resolve().parent.parent
assets = root / 'graphics/titlescreen'
out = root / 'web/generated'
out.mkdir(exist_ok=True)
parts = []
def decompress(data):
    if data[0] != 0x10: raise ValueError('Expected GBA LZ77')
    size = int.from_bytes(data[1:4], 'little')
    result = bytearray()
    cursor = 4
    while len(result) < size:
        flags = data[cursor]; cursor += 1
        for bit in range(7, -1, -1):
            if len(result) >= size: break
            if flags & (1 << bit):
                a, b = data[cursor:cursor+2]; cursor += 2
                length = (a >> 4) + 3
                distance = ((a & 15) << 8 | b) + 1
                if distance > len(result): raise ValueError('Invalid LZ77 distance')
                for _ in range(length):
                    if len(result) >= size: break
                    result.append(result[-distance])
            else:
                result.append(data[cursor]); cursor += 1
    return bytes(result)

for name, filename in [('title_tiles', 'title_fire_emblem_logo.4bpp'), ('title_palette', 'title_fire_emblem_logo.gbapal'),
 ('bg1', 'title_main_background_1.4bpp'), ('bg2', 'title_main_background_2.4bpp'),
 ('bg_palette', 'title_main_background.gbapal'), ('dragon', 'title_dragon_foreground.4bpp'),
 ('dragon_palette', 'title_dragon_foreground.gbapal'), ('extras','title_logos.4bpp'),
 ('start_palette','title_press_start.gbapal'),('copyright_palette','title_copyright.gbapal'),('banner_palette','title_sacred_stones_banner.gbapal'),
 ('bg_map','title_main_background_tsa.bin'),('dragon_map','title_dragon_foreground_tsa.bin')]:
    data = (assets / filename).read_bytes()
    if name.endswith('_map'):
        data = decompress(data)
        if len(data) != 32*20*2: raise ValueError('Unexpected tilemap size: '+str(len(data)))
    parts.append('static const unsigned char ' + name + '[] = {' + ','.join(map(str, data)) + '};')
source = (root / 'src/titlescreen.c').read_text()
match = re.search(r'gSprite_Title_FireEmblemLogo\[\]\s*=\s*\{(.*?)\};', source, re.S)
if not match: raise RuntimeError('Original logo sprite layout not found')
parts.append('static const u16 title_sprite[] = {' + match.group(1) + '};')
for name, original in [('banner_sprite','SacredStonesBanner'),('copyright_sprite','CopyrightInfo'),('start_sprite','PressStart')]:
    match = re.search(r'gSprite_Title_' + original + r'\[\]\s*=\s*\{(.*?)\};', source, re.S)
    if not match: raise RuntimeError('Missing sprite layout: '+original)
    parts.append('static const u16 '+name+'[] = {'+match.group(1)+'};')
(out / 'title_assets.h').write_text('\n'.join(parts))
print('Graphics: original 4bpp logo tiles and palette loaded')
