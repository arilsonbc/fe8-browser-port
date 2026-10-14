from pathlib import Path
import re, json, hashlib, subprocess

root = Path(__file__).resolve().parent.parent
rom = (root/'baserom.gba').read_bytes()
assert hashlib.sha1(rom).hexdigest() == 'c25b145e37456171ada4b0d440bf88a19f4d509f'
symbols = {}
for line in subprocess.check_output(['arm-none-eabi-nm','-S','fireemblem8.elf'], cwd=root, text=True).splitlines():
    p = line.split()
    if len(p) == 4: symbols[p[3]] = (int(p[0],16), int(p[1],16))
    elif len(p)==3: symbols[p[2]]=(int(p[0],16),0)
def raw(address, size):
    offset = address-0x8000000
    assert 0 <= offset <= len(rom)-size
    return rom[offset:offset+size]
def lz(address):
    p=address-0x8000000
    if rom[p]==0:
        return raw(address+4,int.from_bytes(rom[p+1:p+4],'little')-4)
    assert rom[p]==16, hex(address)
    size=int.from_bytes(rom[p+1:p+4],'little'); p+=4; out=bytearray()
    while len(out)<size:
        flags=rom[p]; p+=1
        for bit in range(7,-1,-1):
            if len(out)>=size: break
            if flags & (1<<bit):
                a,b=rom[p:p+2];p+=2; distance=((a&15)<<8|b)+1
                assert distance<=len(out)
                for _ in range((a>>4)+3):
                    if len(out)<size: out.append(out[-distance])
            else: out.append(rom[p]);p+=1
    return bytes(out)
def array(name, data, word=False):
    values = [int.from_bytes(data[p:p+2],'little') for p in range(0,len(data),2)] if word else list(data)
    return 'static const '+('u16' if word else 'u8')+' '+name+'[]={'+','.join(map(str,values))+'};'
textdefs=(root/'texts/textdefs.txt').read_text()
faceIds={m[0]:int(m[1],16)+(int(m[2],16)<<8)-0x100 for m in re.findall(r'\[(FID_\w+)\] = (0x\w+), (0x\w+)',textdefs)}
positionNames=['FarLeft','Left','MidLeft','MidRight','Right','FarRight','FarFarLeft','FarFarRight']
positions={name:i for i,name in enumerate(positionNames)}
messages=json.loads((root/'web/event-texts.js').read_text().split('globalThis.FE8EventTexts=')[1].split(';\n')[0])
sources={int(m.group(1),16):m.group(2).strip() for m in re.finditer(r'^## MSG_([\dA-F]+)\n(.*?)(?=^## |\Z)',(root/'texts/texts.txt').read_text(),re.M|re.S)}
pages=[]; usedFaces=set(); ignored=set()
for key, expected in messages.items():
    faces=[0]*8; smiles=[0]*8; active=-1; pageIndex=0
    for chunk in re.split(r'\[A\]|(?=\[BreakTalk\])',sources[int(key)]):
        for token in re.findall(r'\[([^]]*)\]',chunk):
            if token.startswith('Open') and token[4:] in positions: active=positions[token[4:]]
            elif token.startswith('Move') and token[4:] in positions and active>=0:
                destination=positions[token[4:]];faces[destination]=faces[active];smiles[destination]=smiles[active];faces[active]=0;active=destination
            elif token in faceIds and active>=0:
                faces[active]=255 if token=='FID_Active' else faceIds[token]
                if token!='FID_Active': usedFaces.add(faceIds[token])
            elif token=='ClearFace' and active>=0: faces[active]=0
            elif token=='ToggleSmile' and active>=0: smiles[active]^=1
            elif token not in ['A','LF','X','CR','LoadFace','.','..','...','ToggleMouthMove','CloseSpeechSlow','CloseSpeechFast','SendToBack','BreakTalk']: ignored.add(token)
        clean=re.sub(r'\[[^]]*\]','',chunk.replace('[LF]','\n'))
        clean=re.sub(r'\n{2,}','\n',clean).strip()
        if not clean: continue
        assert clean==expected[pageIndex], (key,pageIndex)
        pages.append({'id':int(key),'page':pageIndex,'text':clean,'active':active,'faces':faces[:],'smiles':smiles[:]})
        pageIndex+=1
    assert pageIndex==len(expected)
parts=['// Verified FE8U portraits, talk glyphs and page control state. Generated.']
faceManifest=[]; table=symbols['portrait_data'][0]
for fid in sorted(usedFaces):
    row=raw(table+(fid-1)*28,28)
    imageAddr=int.from_bytes(row[:4],'little'); palAddr=int.from_bytes(row[8:12],'little'); mouthAddr=int.from_bytes(row[12:16],'little')
    image=lz(imageAddr); palette=raw(palAddr,32); mouth=raw(mouthAddr,48*32)
    assert len(image)>=0x800
    parts += [array('faceImage_'+str(fid),image), array('facePal_'+str(fid),palette,True),array('faceMouth_'+str(fid),mouth)]
    faceManifest.append({'id':fid,'imageAddress':imageAddr,'paletteAddress':palAddr,'mouthAddress':mouthAddr,'bytes':len(image),'sha256':hashlib.sha256(image).hexdigest(),'xMouth':row[20],'yMouth':row[21]})
parts.append('static const struct PortraitAsset portraits[]={'+','.join('{'+f'{f["id"]},faceImage_{f["id"]},{f["bytes"]},facePal_{f["id"]},faceMouth_{f["id"]},{f["xMouth"]},{f["yMouth"]}'+'}' for f in faceManifest)+'};')
parts.append('static const struct DialoguePage dialoguePages[]={'+','.join('{'+str(p['id'])+','+str(p['page'])+','+json.dumps(p['text'])+','+str(p['active'])+',{'+','.join(map(str,p['faces']))+'},{'+','.join(map(str,p['smiles']))+'}}' for p in pages)+'};')
glyphBase=symbols['TextGlyphs_Talk'][0];glyphs=[]
parts.append('static const struct {u8 width;u32 rows[16];} talkGlyphs[256]={')
for code in range(256):
    ptr=int.from_bytes(raw(glyphBase+code*4,4),'little')
    if not ptr: glyphs.append(None);continue
    data=raw(ptr,72);width=data[5];assert width<=16
    rows=[int.from_bytes(data[p:p+4],'little') for p in range(8,72,4)]
    parts.append('['+str(code)+']={'+str(width)+',{'+','.join(map(str,rows))+'}},')
    glyphs.append({'address':ptr,'width':width})
parts.append('};')
parts += [array('talkBubble',lz(symbols['Img_TalkBubble'][0])),array('talkBubblePal',raw(symbols['Pal_TalkBubble'][0],32),True),array('talkTextPal',raw(symbols['Pal_Text'][0]+32,32),True)]
lut=raw(symbols['gUnknown_0858829C'][0]+512,512)
parts.append('static const u8 talkInk[]={'+','.join(str(int.from_bytes(lut[v*2:v*2+2],'little')&15) for v in range(4))+'};')
source=(root/'src/scene.c').read_text();functions=[]
for name in ['PutTalkBubbleTm','PutTalkBubbleTail']:
    m=re.search(r'void '+name+r'\([^\n]*\)\s*\{',source);p=m.end();depth=1
    while depth:
        if source[p]=='{':depth+=1
        if source[p]=='}':depth-=1
        p+=1
    functions.append(source[m.start():p])
(root/'web/generated/dialogue_assets.h').write_text('\n'.join(parts))
(root/'web/generated/dialogue_routines.h').write_text('\n'.join(functions))
(root/'web/generated/dialogue_assets.json').write_text(json.dumps({'portraits':faceManifest,'pages':pages,'glyphs':glyphs,'ignoredPresentationTokens':sorted(ignored)},indent=2))
print(f'Dialogue: {len(pages)} pages, {len(usedFaces)} portraits, original talk font and bubble routines; deferred tokens: {sorted(ignored)}')
