from pathlib import Path
import re,hashlib,subprocess,json
root=Path(__file__).resolve().parent.parent;rom=(root/'baserom.gba').read_bytes();assert hashlib.sha1(rom).hexdigest()=='c25b145e37456171ada4b0d440bf88a19f4d509f';syms={}
for line in subprocess.check_output(['arm-none-eabi-nm','-S','fireemblem8.elf'],cwd=root,text=True).splitlines():
 p=line.split()
 if len(p)==4:syms[p[3]]=(int(p[0],16),int(p[1],16))
a,n=syms['gDefeatTalkList'];data=rom[a-0x8000000:a-0x8000000+n];entries=[];rows=[]
for i in range(0,n,12):
 r=data[i:i+12];assert len(r)==12;v=[int.from_bytes(r[:2],'little'),r[2],r[3],int.from_bytes(r[4:6],'little'),int.from_bytes(r[6:8],'little'),int.from_bytes(r[8:12],'little')];entries.append('{'+','.join(map(str,v))+'}');rows.append(dict(zip(['pid','route','chapter','flag','msg','event'],v)))
 if v[0]==65535:break
assert rows[-1]['pid']==65535
parts=['struct DefeatTalkEnt {u16 pid;u8 route,chapter;u16 flag,msg;u32 event;};','static struct DefeatTalkEnt gDefeatTalkList[]={'+','.join(entries)+'};'];fs=[];source=(root/'src/eventinfo.c').read_text()
for name in ['GetDefeatTalkEntry','CheckBattleDefeatTalk','DisplayDefeatTalkForPid']:
 m=re.search(r'(?:struct DefeatTalkEnt\*|s8|void) '+name+r'\([^\n]*\)\s*\{',source);assert m,name;i=m.end();d=1
 while d:
  if source[i]=='{':d+=1
  if source[i]=='}':d-=1
  i+=1
 f=source[m.start():i].replace('EventEngine_CreateBattle((u16 *)ent->event);','NativeDeathEvent(ent->event);');fs.append(f)
(root/'web/generated/death_assets.h').write_text('\n'.join(parts));(root/'web/generated/death_routines.h').write_text('\n'.join(f[:f.index('{')].strip()+';' for f in fs)+'\n'+'\n'.join(fs));(root/'web/generated/death_assets.json').write_text(json.dumps(rows,indent=2));print('Death: original defeat table and three original selection/display routines; prologue messages relocated')
