"""Extract only the original sound bank; pointers are bounded data keys, never code."""
from pathlib import Path
import hashlib, json, re, struct, subprocess
root = Path(__file__).resolve().parent.parent
rom = (root / 'baserom.gba').read_bytes()
assert hashlib.sha1(rom).hexdigest() == 'c25b145e37456171ada4b0d440bf88a19f4d509f'
symbols = {}
for line in subprocess.check_output(['arm-none-eabi-nm', '-S', 'fireemblem8.elf'], cwd=root, text=True).splitlines():
    p = line.split()
    if len(p) in (3, 4): symbols[p[-1]] = int(p[0], 16)
def u32(a): return struct.unpack_from('<I', rom, a - 0x8000000)[0]
table = symbols['gSongTable']
labels = re.findall(r'^\s*song (\w+), (\d+), (\d+)', (root/'sound/song_table.s').read_text(), re.M)
songs = []
end = 0
for i, (label, player, _) in enumerate(labels):
    a = u32(table+i*8)
    assert a == symbols[label]
    n = rom[a-0x8000000]
    assert n <= 16
    songs.append(dict(id=i, name=label, address=a, player=int(player), tracks=n))
    end = max(end, a+8+4*n)
base = symbols['gScaleTable']
assert base < table < end < len(rom)+0x8000000
bank = rom[base-0x8000000:end-0x8000000]
meta = dict(base=base, size=len(bank), sha256=hashlib.sha256(bank).hexdigest(), songs=songs,
            tables={k:symbols[k] for k in ['gClockTable','gScaleTable','gFreqTable','gCgbScaleTable','gCgbFreqTable','gNoiseTable']})
(root/'web/dist/audio-bank.bin').write_bytes(bank)
(root/'web/dist/audio-bank.json').write_text(json.dumps(meta,separators=(',',':')))
(root/'web/generated/audio_assets.json').write_text(json.dumps(meta,indent=2))
# Keep all five original event handlers, including skip rules and yield semantics.
source = (root/'src/eventscr.c').read_text()
functions=[]
for name in ['Event12_StartBGM','Event13_BgmFadeIn','Event14_BgmOverideRestore','Event15_BgmVolume','Event16_']:
    m=re.search(r'u8 '+name+r'\([^\n]*\)\s*\{',source); assert m,name
    i=m.end(); depth=1
    while depth:
        if source[i]=='{':depth+=1
        if source[i]=='}':depth-=1
        i+=1
    functions.append(source[m.start():i])
(root/'web/generated/audio_events.h').write_text('// Original event audio handlers, platform wrappers supplied by events.c.\n'+'\n'.join(functions))
print(f'Audio: {len(songs)} original song-table entries, {len(bank):,} sound-bank bytes, five original event handlers')
chapter = symbols['gChapterDataTable']
rows=[]
for i in range(79):
    a=chapter+i*148
    ids=struct.unpack_from('<11H',rom,a-0x8000000+0x16)
    threshold=struct.unpack_from('<b',rom,a-0x8000000+0x86)[0]
    rows.append('{ {'+','.join(map(str,ids))+'},'+str(threshold)+'}')
(root/'web/generated/audio_chapters.h').write_text('struct AudioChapter {u16 mapBgmIds[11];s8 victorySongEnemyThreshold;};\nstatic const struct AudioChapter audioChapters[]={'+','.join(rows)+'};\n')
source=(root/'src/bm.c').read_text();functions=[]
for name in ['CheckAltBgm','GetCurrentMapMusicIndex']:
    m=re.search(r'(?:static inline )?int '+name+r'\([^\n]*\)\s*\{',source);assert m,name
    i=m.end();depth=1
    while depth:
        if source[i]=='{':depth+=1
        if source[i]=='}':depth-=1
        i+=1
    f=source[m.start():i]
    if name=='GetCurrentMapMusicIndex': f=f[:-1]+'    return -1; // Guarded unreachable invalid faction in native backend.\n}'
    functions.append(f)
# Original switch has no invalid-faction return; guard it in the public wrapper.
(root/'web/generated/audio_map_routines.h').write_text('\n'.join(functions))
