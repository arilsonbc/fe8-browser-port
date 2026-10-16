from pathlib import Path
import re,json,hashlib,subprocess
root=Path(__file__).resolve().parent.parent
rom=(root/'baserom.gba').read_bytes();assert hashlib.sha1(rom).hexdigest()=='c25b145e37456171ada4b0d440bf88a19f4d509f'
symbols={}
for line in subprocess.check_output(['arm-none-eabi-nm','-S','fireemblem8.elf'],cwd=root,text=True).splitlines():
 p=line.split()
 if len(p)>=3:symbols[p[-1]]=int(p[0],16)
def lz(address):
 p=address-0x8000000;assert rom[p]==16;size=int.from_bytes(rom[p+1:p+4],'little');p+=4;out=bytearray()
 while len(out)<size:
  flags=rom[p];p+=1
  for bit in range(7,-1,-1):
   if len(out)>=size:break
   if flags&(1<<bit):
    a,b=rom[p:p+2];p+=2;distance=((a&15)<<8|b)+1;assert distance<=len(out)
    for _ in range((a>>4)+3):
     if len(out)<size:out.append(out[-distance])
   else:out.append(rom[p]);p+=1
 return bytes(out)
parts=['#define EventCallGameOverExt_ADDRESS '+str(symbols['EventCallGameOverExt'])];manifest=[]
(root/'web/generated/gameover_address.h').write_text(parts[0])
for name,size in [('Img_GameOverText',0),('Img_ChapterIntroFog',0),('Pal_GameOverText1',32),('Pal_GameOverText2',32),('Tsa_GameOverFx',68)]:
 a=symbols[name];data=lz(a) if not size else rom[a-0x8000000:a-0x8000000+size]
 parts.append('static const u8 '+name+'[]={'+','.join(map(str,data))+'};');manifest.append(dict(name=name,address=a,size=len(data),sha256=hashlib.sha256(data).hexdigest()))
(root/'web/generated/gameover_assets.h').write_text('\n'.join(parts));(root/'web/generated/gameover_assets.json').write_text(json.dumps(manifest,indent=2))
def fn(file,name):
 source=(root/file).read_text();m=re.search(r'void '+name+r'\([^\n]*\)\s*\{',source);assert m,name;p=m.end();depth=1
 while depth:
  if source[p]=='{':depth+=1
  if source[p]=='}':depth-=1
  p+=1
 return source[m.start():p]
source=(root/'src/gameoverbgfx.c').read_text();parts=[]
for name in ['ProcGameOverScroll','ProcGameOverScreen']:parts.append(re.search(r'struct '+name+r'\s*\{.*?\};',source,re.S).group())
names=['GameOverScreen_RandomScroll_Init','GameOverScreen_RandomScroll_Loop','GameOverScreenHBlank','GameOverScreen_Init','GameOverScreen_LoopFadeIn','GameOverScreen_BeginIdle','GameOverScreen_LoopIdle','GameOverScreen_BeginFadeOut','GameOverScreen_LoopFadeOut','GameOverScreen_End','StartGameOverScreen']
functions=[fn('src/gameoverbgfx.c',n) for n in names]
for i,f in enumerate(functions):
 if names[i]=='GameOverScreen_Init':f=f.replace('    int i;','    int i;\n    screenStarted=1;nativeState=2;')
 if names[i]=='GameOverScreen_BeginIdle':f=f.replace('    proc->counter2 =','    nativeState=3;\n    proc->counter2 =')
 if names[i]=='GameOverScreen_BeginFadeOut':f=f.replace('    MaybeResetSomePal();','    nativeState=4;\n    MaybeResetSomePal();')
 if names[i]=='StartGameOverScreen':f=f.replace('PROC_TREE_3','PROC_TREE_5')
 functions[i]=f
parts += [f[:f.index('{')].strip()+';' for f in functions]
for name in ['ProcScr_GameOverScreen_RandomScroll','ProcScr_GameOverScreen']:
 s=re.search(r'struct ProcCmd CONST_DATA '+name+r'\[\] =.*?\};',source,re.S).group().replace('PROC_CALL(EndAllMus)','PROC_CALL(NativeEndAllMus)');parts.append(s)
parts+=functions
for name in ['PutScreenFogEffect','PutScreenFogEffectOverlayed']:parts.insert(0,fn('src/chapterintrofx.c',name))
(root/'web/generated/gameover_screen.h').write_text('\n'.join(parts))
pal=[fn('src/hardware.c',n) for n in ['MaybeResetSomePal','MaybeSmoothChangeSomePal']]
(root/'web/generated/gameover_palette.h').write_text('\n'.join(pal))
source=(root/'src/eventcall.c').read_text();parts=[]
names=['GameOver_FadeOutCurrentBgm','SkilGameOverForToturialExtraMap','PostGameOverHandler'];functions=[fn('src/eventcall.c',n) for n in names]
parts += [f[:f.index('{')].strip()+';' for f in functions]
parts.append(re.search(r'CONST_DATA struct ProcCmd ProcScr_BmGameOver\[\] =.*?\};',source,re.S).group().replace('PROC_CALL(StartSlowFadeToBlack)','PROC_CALL(NativeSlowFade)').replace('PROC_CALL(EndAllMus)','PROC_CALL(NativeEndAllMus)').replace('PROC_CALL(StartGameOverScreen)','PROC_CALL(NativeStartScreen)'))
parts+=functions
(root/'web/generated/gameover_flow.h').write_text('\n'.join(parts))
print('Game Over: five original resources, original screen/scroll/palette/flow routines and Proc scripts; hardware composition is native')
