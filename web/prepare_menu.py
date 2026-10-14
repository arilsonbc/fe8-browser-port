from pathlib import Path
import re,hashlib,json
root=Path(__file__).resolve().parent.parent
for p in (root/'web').rglob('*'):
    if p.is_file():p.read_bytes()
rom=(root/'baserom.gba').read_bytes()
if hashlib.sha1(rom).hexdigest()!='c25b145e37456171ada4b0d440bf88a19f4d509f':raise RuntimeError('Unexpected base ROM')
def lz(data):
    size=int.from_bytes(data[1:4],'little');out=bytearray();p=4
    while len(out)<size:
        flags=data[p];p+=1
        for bit in range(7,-1,-1):
            if len(out)>=size:break
            if flags&(1<<bit):
                a,b=data[p:p+2];p+=2;n=(a>>4)+3;d=((a&15)<<8|b)+1
                if d>len(out):raise ValueError('Invalid LZ77')
                for _ in range(n):
                    if len(out)<size:out.append(out[-d])
            else:out.append(data[p]);p+=1
    return bytes(out)
assets={}
for file in ['data/data_A21658.s','data/data_A01CC4.s','data/trig_rodata.s','data/data_5B65C0.s']:
    source=(root/file).read_text()
    for m in re.finditer(r'(\w+):[^\n]*\n\s*\.incbin "baserom.gba", (0x[\da-fA-F]+), (0x[\da-fA-F]+)',source):
        name,offset,length=m.groups();assets[name]=rom[int(offset,16):int(offset,16)+int(length,16)]
assets['gPal_08A07AD8']=rom[0xA07AD8:0xA07AD8+0x180]
assets['gPal_08A07C58']=rom[0xA07C58:0xA07C58+0x180]
names=['Img_SaveMenuBG','Pal_SaveMenuBG','Tsa_SaveMenuBG','Img_MainMenuBgFog','Pal_MainMenuBgFog','Tsa_MainMenuBgFog','Img_SaveScreenSprits','Pal_SaveScreenSprits','Pal_08A28088','Img_GameMainMenuObjs','Img_DifficultyMenuObjs','Pal_DifficultyMenuObjs','gUnknown_08A29498','gUnknown_08A29558','Pal_08A295B4','gSinLookup','gPal_08A07AD8','gPal_08A07C58','gGfx_08A09E4C','gUiFrameImage','gUiFramePaletteA']
# Original save-title bitmap table, 88 entries declared in chap_title_data.c.
maptext=(root/'fireemblem8.map').read_text()
address=int(re.search(r'(0x[\da-f]+)\s+chap_title_data\b',maptext).group(1),16)-0x8000000
chap_titles=[]
count=len(re.findall(r'\{chap_title_', (root/'src/chap_title_data.c').read_text()))
for i in range(count):
 ptr=int.from_bytes(rom[address+i*12:address+i*12+4],'little')-0x8000000
 data=lz(rom[ptr:])
 if len(data)!=2048:raise RuntimeError('Unexpected save title size '+str(i))
 chap_titles.append(data)
assets['empty_title']=chap_titles[0x54];names.append('empty_title')
chapter_address=int(re.search(r'(0x[\da-f]+)\s+gChapterDataTable\b',maptext).group(1),16)-0x8000000
chapter_titles=[rom[chapter_address+i*148+14] for i in range(79)]
(root/'web/generated/chapter_save_titles.h').write_text('static const u8 chapterTitles[]={'+','.join(map(str,chapter_titles))+'};\nstatic const u8 saveTitleBitmaps[][2048]={'+','.join('{'+','.join(map(str,d))+'}' for d in chap_titles)+'};')
(root/'web/generated/save_chapter_ids.h').write_text('static const u8 saveChapterTitleIds[]={'+','.join(map(str,chapter_titles))+'};')
parts=['// Original menu assets from the verified US ROM. Generated; do not edit.']
manifest={}
for name in names:
    data=assets[name]
    if name.startswith('Img_') or name in ['Tsa_MainMenuBgFog','gUnknown_08A29558','gGfx_08A09E4C','gUiFrameImage']:
        if data[0]!=0x10:raise ValueError('LZ77 expected '+name)
        data=lz(data)
    palette=name.startswith(('Pal_','gPal_','gUiFramePalette')) or name in ['gUnknown_08A29498','gSinLookup']
    values=list(data) if not palette else [int.from_bytes(data[i:i+2],'little',signed=name=='gSinLookup') for i in range(0,len(data),2)]
    parts.append('static const '+('s16' if name=='gSinLookup' else 'u16' if palette else 'u8')+' '+name+'[]={'+','.join(map(str,values))+'};')
    manifest[name]={'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
(root/'web/generated/menu_assets.h').write_text('\n'.join(parts))
(root/'web/generated/menu_assets.json').write_text(json.dumps(manifest,indent=2))
def function(file,name):
    s=(root/file).read_text();m=re.search(r'(?:void|bool|u8|s8|int) '+name+r'\([^\n]*\)\s*\{',s)
    if not m:raise RuntimeError(name+' missing')
    pos=m.end();depth=1
    while depth:
        if s[pos]=='{':depth+=1
        if s[pos]=='}':depth-=1
        pos+=1
    return s[m.start():pos]
def struct(file,name):
    return re.search(r'struct '+name+r'\s*\{.*?\};',(root/file).read_text(),re.S).group()
header=(root/'include/savemenu.h').read_text()
types=['#ifndef FE8_MENU_TYPES_H','#define FE8_MENU_TYPES_H','#include "../palette.h"','#include "proc.h"','typedef uint64_t u64;','typedef u16 bool16;']
types+=re.findall(r'enum(?: \w+)?\s*\{.*?\};',header,re.S)[:4]
for name in ['Text','KeyStatusBuffer','SaveDrawProc','SaveMenuProc','SaveDrawCursorProc','Unknown020007E0','DifficultyMenuSpritesProc','DifficultyMenuProc']:
    file='include/fontgrp.h' if name=='Text' else 'include/hardware.h' if name=='KeyStatusBuffer' else 'include/savemenu.h';types.append(struct(file,name))
types+=['enum {A_BUTTON=1,B_BUTTON=2,START_BUTTON=8,DPAD_RIGHT=16,DPAD_LEFT=32,DPAD_UP=64,DPAD_DOWN=128};']
types+=re.findall(r'^#define OAM[^\n]*',(root/'include/ctc.h').read_text(),re.M)
types+=['#define SAVE_MENU_PARENT(proc) ((struct SaveMenuProc *)(proc->proc_parent))','#define PAL_OBJ_COLOR(p,c) gPaletteBuffer[256+(p)*16+(c)]','#define SIN(angle) (gSinLookup[(angle)&255])','#define BG_2 2','#define DISPLAY_HEIGHT 160','#define CHAR_BIT 8','#endif']
(root/'web/generated/menu_types.h').write_text('\n'.join(types))
layouts=[]
for file in ['src/savedraw.c','src/savemenu_data.c','src/difficultymenu.c']:
    s=(root/file).read_text()
    layouts+=re.findall(r'u16 (?:\* )?CONST_DATA (?:gSprite\w*|Sprite\w*)\[\]\s*=\s*\{.*?\};',s,re.S)
layouts+=['static const u8 gUnknown_08A20B14[]={0,1,2,3,3,2,1,0};','static const struct {int a,b;} gUnknown_08A209FC[]={{0,32},{0,64},{0,96}};']
(root/'web/generated/menu_layouts.h').write_text('\n'.join(layouts))
# Original routines operate on the real process structs; platform boundaries live in menu.c.
groups={
 'src/savemenu.c':['SaveMenuGetBitfile','SaveMenuGetBitfileByMask','BitfileToIndex','SameMenu_CtrlLoop','ExecSaveMenuMiscOption','SaveMenu_SaveSlotSelectLoop','SaveMenuScrollSlot','SaveMenuScrollBackToMain','sub_80A9A68','sub_80A9AB0','sub_80A9B90','SaveMenu_PostDifficultHandler','SaveMenuCopyPalette'],
 'src/savedraw.c':['AddMainMenuOption','AddExtraMenuOption','InitSaveMenuChoice','SaveMenuModifySaveSlot','SaveMenuTryMoveSaveSlotCursor','sub_80AABC4','sub_80AAC40','sub_80AAE90','sub_80AADE0','sub_80AA7EC','sub_80AAF6C','SaveDrawCursor_Init','SaveDrawCursor_Loop','sub_80AB4F4','SaveDraw_ScrollFogBG','sub_80AAB78','sub_80AB548','sub_80AB56C'],
 'src/difficultymenu.c':['DifficultySelect_Loop_KeyHandler','DrawDifficultySprite_Init','DrawDifficultyMenuCursorMaybe','sub_80AC4F8','DrawDifficultySprites_Loop','sub_80AC680']}
functions=[function(file,name) for file,names_ in groups.items() for name in names_]
prototypes=[s[:s.index('{')].strip()+';' for s in functions]
(root/'web/generated/menu_routines.h').write_text('\n'.join(prototypes)+'\n'+'\n'.join(functions))
print('Menu: verified assets, layouts, types and '+str(len(functions))+' original routines extracted')


# Original shared palette-archive fade implementation and the menu's sysutil processes.
fade=struct('include/bmlib.h','PalFadeSt')+'\n'+struct('include/sysutil.h','ProcFadeInOut')+'\n'
fade+='''static struct PalFadeSt sPalFadeSt[32];
static struct PalFadeSt *GetPalFadeSt(void){return sPalFadeSt;}
static void EnablePaletteSync(void){}
static void EnableAllGfx(ProcPtr p){(void)p;SetBlendDarken(0);SetDispEnable(1,1,1,1,1);}
'''
fadefuncs=[function('src/bmlib.c',name) for name in ['SetPalFadeStClkEnd1','SetPalFadeStClkEnd2','SetPalFadeStClkEnd3','ArchiveCurrentPalettes','WriteFadedPaletteFromArchive']]
fadefuncs+=[function('src/sysutil.c',name) for name in ['FadeInOut_Init','FadeIn_Loop','FadeOut_Loop','FadeInOut_DisableGfx']]
fade+='\n'.join(f[:f.index('{')].strip()+';' for f in fadefuncs)+'\n'+'\n'.join(fadefuncs)
s=(root/'src/sysutil.c').read_text();m=re.search(r'CONST_DATA struct ProcCmd ProcScr_BmFadeIN\[\].*?\};',s,re.S);fade+=m.group()+'\n'
m=re.search(r'CONST_DATA struct ProcCmd ProcScr_BmFadeOUT\[\].*?\};',s,re.S);fade+=m.group()+'\n'
for name in ['FadeInExists','FadeOutExists','NewFadeIn','NewFadeOut']:fade+=function('src/sysutil.c',name)+'\n'
(root/'web/generated/menu_fades.c').write_text('#include "../palette.h"\n#include "proc.h"\n'+fade)
# Bitmap glyphs and selected descriptions are extracted from repository originals.
s=(root/'data/fonts/glyphs_1.h').read_text();table=re.search(r'TextGlyphs_System\[\]\s*=\s*\{(.*?)\};',s,re.S).group(1)
refs=re.findall(r'&\w+|NULL',table);glyphs={}
for m in re.finditer(r'struct Glyph (\w+)\s*=\s*\{(.*?)\n\};',s,re.S):
 body=m.group(2);width=int(re.search(r'\.width\s*=\s*(\d+)',body).group(1));rows=re.findall(r'0x[\da-fA-F]+',re.search(r'\.bitmap\s*=\s*\{(.*?)\}',body,re.S).group(1));glyphs[m.group(1)]=(width,rows)
parts=['static const struct {u8 width;u32 rows[16];} menuGlyphs[256]={']
for i,ref in enumerate(refs[:256]):
 if ref=='NULL':continue
 width,rows=glyphs[ref[1:]];parts.append('['+str(i)+']={'+str(width)+',{'+','.join(rows)+'}},')
parts.append('};');descriptions=[];texts=(root/'texts/texts.txt').read_text()
for msg in ['149','14A','14B']:
 desc=re.search(r'## MSG_'+msg+r'\n(.*?)(?:\n##|\Z)',texts,re.S).group(1).strip().replace('[LF]','\n').replace('[.]','').replace('[X]','')
 # Source line endings duplicate [LF]; preserve the explicit game line breaks.
 desc=desc.replace('\n\n','\n');descriptions.append(desc)
parts.append('static const char *difficultyDescriptions[]={'+','.join(json.dumps(d) for d in descriptions)+'};')
parts.append('static const u16 menuTextPalette[]={'+','.join(str(int.from_bytes(rom[i:i+2],'little')) for i in range(0x59EF00,0x59EF20,2))+'};')
(root/'web/generated/menu_font.h').write_text('\n'.join(parts))


(root/"web/generated/save_palette_routines.h").write_text(function("src/chapter_title.c","sub_80895B4").replace("u16 * pal;","const u16 * pal;")+"\n"+function("src/difficultymenu.c","SaveMenuInitSlotPalette")+"\n"+function("src/difficultymenu.c","SaveDrawSetDifficultSlotPalette").replace("u16 * r6;","const u16 * r6;").replace("u16 * ip;","const u16 * ip;").replace("u16 * pickle","const u16 * pickle").replace("u16 * ketchup","const u16 * ketchup"))
