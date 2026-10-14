from pathlib import Path
import re,hashlib,subprocess
root=Path(__file__).resolve().parent.parent;rom=(root/'baserom.gba').read_bytes();assert hashlib.sha1(rom).hexdigest()=='c25b145e37456171ada4b0d440bf88a19f4d509f'
syms={}
for line in subprocess.check_output(['arm-none-eabi-nm','-S','fireemblem8.elf'],cwd=root,text=True).splitlines():
 p=line.split()
 if len(p)==4:syms[p[3]]=(int(p[0],16),int(p[1],16))
def fn(file,name):
 s=(root/file).read_text();m=re.search(r'(?:inline )?(?:void|bool|s8|u8|u32|int) '+name+r'\([^\n]*\)\s*\{',s);assert m,name;i=m.end();d=1
 while d:
  if s[i]=='{':d+=1
  if s[i]=='}':d-=1
  i+=1
 return s[m.start():i].replace('inline ','')
def output(name,functions): (root/'web/generated'/name).write_text('\n'.join(f[:f.index('{')].strip()+';' for f in functions)+'\n'+'\n'.join(functions))
parts=[]
for name in ['TerrainTable_HealAmount','TerrainTable_HealsStatus']:
 a,n=syms[name];data=rom[a-0x8000000:a-0x8000000+n];parts+=['static const s8 '+name+'[]={'+','.join(str(int.from_bytes(bytes([v]),'little',signed=True)) for v in data)+'};']
(root/'web/generated/recovery_assets.h').write_text('\n'.join(parts))
fs=[fn('src/bmmap.c',n) for n in ['GetTerrainHealAmount','GetTerrainHealsStatus']]+[fn('src/bmunit.c','AddUnitHp'),fn('src/bmitem.c','GetUnitItemHealAmount'),fn('src/bmtarget.c','MakeTerrainHealTargetList')]
fs[-1]=fs[-1].replace('        terrainId =','        if(unit->xPos<0||unit->yPos<0||unit->xPos>=fe8_map_info(1)||unit->yPos>=fe8_map_info(2))continue;\n        terrainId =')
output('recovery_routines.h',fs)
names=[('src/cp_utility.c','AiFindSafestReachableLocation'),('src/cp_utility.c','AiIsInByteList'),('src/cp_utility.c','AiCountNearbyEnemyUnits'),('src/cp_0803E2F4.c','AiTryGetNearestHealPoint'),('src/cp_0803E2F4.c','AiTryHealSelf'),('src/cp_0803E2F4.c','UpdateAllPhaseHealingAIStatus'),('src/cp_0803E2F4.c','sub_803EEB0')];fs=[]
for file,name in names:
 f=fn(file,name)
 if name=='AiCountNearbyEnemyUnits':
  start=f.index('    it--;');end=f.index('        if (gBmMapUnit',start)
  f=f[:start]+'''    for(;it->x!=9999;it++) {
        if(x+it->x<0||y+it->y<0||x+it->x>=gBmMapSize.x||y+it->y>=gBmMapSize.y)continue;
''' +f[end:]
 if name=='UpdateAllPhaseHealingAIStatus':f=f.replace('FACTION_ID_BLUE','0').replace('FACTION_ID_GREEN','1').replace('FACTION_ID_RED','2')
 if name=='sub_803EEB0':f=f.replace('    BmMapFill(gBmMapMovement, 0);','    SetWorkingBmMap(gBmMapMovement);\n    BmMapFill(gBmMapMovement, 0);')
 fs.append(f)
# Use the verified native offset table with the same semantic entries.
a,n=syms['sRange3OffsetLut'];data=rom[a-0x8000000:a-0x8000000+n];assert n%4==0
(root/'web/generated/ai_heal_assets.h').write_text('static const u8 sTerrainList_Fort[]={TERRAIN_FORT,0};\nstatic struct Vec2 sRange3OffsetLut[]={'+','.join('{'+str(int.from_bytes(data[i:i+2],"little",signed=True))+','+str(int.from_bytes(data[i+2:i+4],"little",signed=True))+'}' for i in range(0,n,4))+'};')
output('ai_heal_routines.h',fs)
print('Recovery: five original terrain/HP/item routines and seven original healing AI routines; bounds guarded')
