from pathlib import Path
import re,hashlib,subprocess,json
root=Path(__file__).resolve().parent.parent;rom=(root/'baserom.gba').read_bytes();assert hashlib.sha1(rom).hexdigest()=='c25b145e37456171ada4b0d440bf88a19f4d509f'
symbols={}
for line in subprocess.check_output(['arm-none-eabi-nm','-S','fireemblem8.elf'],cwd=root,text=True).splitlines():
 p=line.split()
 if len(p)==4:symbols[p[3]]=(int(p[0],16),int(p[1],16))
source=(root/'src/bmreliance.c').read_text()
def fn(name):
 m=re.search(r'(?:static )?(?:const struct \w+\*|struct \w+\*|void|int|u8|s8) '+name+r'\([^\n]*\)\s*\{',source);assert m,name
 p=m.end();depth=1
 while depth:
  if source[p]=='{':depth+=1
  if source[p]=='}':depth-=1
  p+=1
 return source[m.start():p]
header=(root/'include/bmreliance.h').read_text();parts=re.findall(r'enum\s*\{.*?\};',header,re.S)+[re.search(r'struct SupportBonuses\s*\{.*?\};',header,re.S).group()]
(root/'web/generated/support_types.h').write_text('#ifndef FE8_SUPPORT_TYPES_H\n#define FE8_SUPPORT_TYPES_H\n'+'\n'.join(parts)+'\n#endif\n')
a,n=symbols['gAffinityBonuses'];data=rom[a-0x8000000:a-0x8000000+n];assert len(data)==64;data=b''.join(data[i:i+7] for i in range(0,len(data),8));assert data[-7]==0
parts=['static const struct SupportBonuses gAffinityBonuses[]={'+','.join('{'+','.join(map(str,data[i:i+7]))+'}' for i in range(0,len(data),7))+'};','static int sSupportMaxExpLookup[]={80,160,240,241};']
(root/'web/generated/support_assets.h').write_text('\n'.join(parts))
names=['GetUnitSupporterCount','GetUnitSupporterCharacter','GetUnitSupporterUnit','GetUnitSupportLevel','GetUnitTotalSupportLevel','UnitGainSupportExp','GetUnitSupporterInitialExp','GetUnitSupporterNum','ProcessTurnSupportExp','GetAffinityBonuses','ApplyAffinitySupportBonuses','InitSupportBonuses','GetUnitSupportBonuses']
functions=[fn(n) for n in names]
functions=[f.replace('int supportExp = unit->supports[num];','if(num<0||num>=7)return SUPPORT_LEVEL_NONE;\n    int supportExp = unit->supports[num];').replace('// return NULL; // BUG?','return gAffinityBonuses+(sizeof(gAffinityBonuses)/sizeof(*gAffinityBonuses))-1; // Native guard for unknown affinity.') for f in functions]
(root/'web/generated/support_routines.h').write_text('\n'.join(f[:f.index('{')].strip()+';' for f in functions)+'\n'+'\n'.join(functions))
(root/'web/generated/support_assets.json').write_text(json.dumps({'routines':names,'affinities':len(data)//7,'boundaries':['support conversations/menu and persistent usage stats pending'],'guards':['invalid support index returns level NONE','unknown affinity returns zero bonus']},indent=2))
print(f'Support: {len(functions)} original routines, relocated character links and affinity table')


