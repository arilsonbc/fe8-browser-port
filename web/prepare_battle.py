from pathlib import Path
import re, hashlib, subprocess, json
root=Path(__file__).resolve().parent.parent
rom=(root/'baserom.gba').read_bytes();assert hashlib.sha1(rom).hexdigest()=='c25b145e37456171ada4b0d440bf88a19f4d509f'
symbols={}
for line in subprocess.check_output(['arm-none-eabi-nm','-S','fireemblem8.elf'],cwd=root,text=True).splitlines():
 p=line.split()
 if len(p)==4:symbols[p[3]]=(int(p[0],16),int(p[1],16))
 elif len(p)==3:symbols[p[2]]=(int(p[0],16),0)
def raw(a,n):
 p=a-0x8000000;assert 0<=p<=len(rom)-n;return rom[p:p+n]
def struct(file,name):return re.search(r'struct '+name+r'\s*\{.*?\};',(root/file).read_text(),re.S).group()
def fn(file,name):
 source=(root/file).read_text();m=re.search(r'(?:(?:static )?inline )?(?:const struct \w+\*|const u8\*|void|bool|s8|u8|u16|u32|int) '+name+r'\([^\n]*\)\s*\{',source);assert m,name
 p=m.end();depth=1
 while depth:
  if source[p]=='{':depth+=1
  if source[p]=='}':depth-=1
  p+=1
 return source[m.start():p].replace('static inline ','static ').replace('inline ','')
header=['#ifndef FE8_BATTLE_TYPES_H','#define FE8_BATTLE_TYPES_H','#include "unit_types.h"','#include "constants/items.h"']
for name in ['BattleUnit','BattleStats','BattleHit']:header.append(struct('include/bmbattle.h',name))
for name in ['ItemStatBonuses','ItemData']:header.append(struct('include/bmitem.h',name))
for file in ['include/bmbattle.h','include/bmitem.h']:header+=re.findall(r'enum\s*\{.*?\};',(root/file).read_text(),re.S)
header+=re.findall(r'enum\s*\{[^}]*UNIT_STATUS_NONE[^}]*\};',(root/'include/bmunit.h').read_text(),re.S)
header+=['#define ITEM_INDEX(item) ((item)&255)','_Static_assert(sizeof(struct BattleHit)==4,"Battle hit ABI");','_Static_assert(sizeof(struct BattleUnit)==128,"Battle unit ABI");','#endif']
(root/'web/generated/battle_types.h').write_text('\n'.join(header))
assets=[];a,n=symbols['gItemData'];items=raw(a,n);effective={};bonus={}
def effect(ptr):
 if not ptr:return 'NULL'
 if ptr not in effective:
  data=bytearray();p=ptr
  for _ in range(128):
   v=raw(p,1)[0];data.append(v);p+=1
   if not v:break
  assert data[-1]==0
  name=next((name for name,(address,_) in symbols.items() if address==ptr and name.startswith('ItemEffectiveness_')),'battleEffect_'+str(len(effective)))
  assets.append('static const u8 '+name+'[]={'+','.join(map(str,data))+'};');effective[ptr]=name
 return effective[ptr]
for name in ['ItemEffectiveness_Flier','ItemEffectiveness_FlierAndMonsters','ItemEffectiveness_Monsters']:effect(symbols[name][0])
entries=[]
fields=[('nameTextId',0,2),('descTextId',2,2),('useDescTextId',4,2),('number',6,1),('weaponType',7,1),('attributes',8,4),('maxUses',20,1),('might',21,1),('hit',22,1),('weight',23,1),('crit',24,1),('encodedRange',25,1),('costPerUse',26,2),('weaponRank',28,1),('iconId',29,1),('useEffectId',30,1),('weaponEffectId',31,1),('weaponExp',32,1)]
for i in range(len(items)//36):
 row=items[i*36:i*36+36];values=['.'+name+'='+str(int.from_bytes(row[p:p+size],'little')) for name,p,size in fields]
 ptr=int.from_bytes(row[12:16],'little')
 if ptr:
  if ptr not in bonus:
   name='battleBonus_'+str(len(bonus));data=raw(ptr,9);assets.append('static const struct ItemStatBonuses '+name+'={'+','.join(str(int.from_bytes(bytes([v]),'little',signed=True)) for v in data)+'};');bonus[ptr]=name
  values.append('.pStatBonuses=&'+bonus[ptr])
 values.append('.pEffectiveness='+effect(int.from_bytes(row[16:20],'little')))
 entries.append('['+str(i)+']={'+','.join(values)+'}')
assets.append('static const struct ItemData gItemData[256]={'+','.join(entries)+'};')
source=(root/'src/bmbattle.c').read_text();assets.append(struct('src/bmbattle.c','WeaponTriangleRule'))
assets.append(re.search(r'static CONST_DATA struct WeaponTriangleRule sWeaponTriangleRules\[\] =.*?\};',source,re.S).group().replace('CONST_DATA','const'))
(root/'web/generated/battle_assets.h').write_text('\n'.join(assets))
itemFunctions=['EquipUnitItemSlot','GetItemData','GetItemIndex','GetItemType','GetItemAttributes','GetItemMight','GetItemHit','GetItemWeight','GetItemCrit','GetItemMinRange','GetItemMaxRange','GetItemAwardedExp','GetItemRequiredExp','GetItemCostPerUse','GetItemUses','GetItemCost','GetItemEffectiveness','GetItemStatBonuses','GetItemWeaponEffect','GetItemAfterUse','GetUnitEquippedWeapon','GetUnitEquippedWeaponSlot','IsItemCoveringRange','CanUnitUseWeapon','CanUnitUseWeaponNow','CanUnitUseStaff','IsItemEffectiveAgainst','IsUnitEffectiveAgainst']+['GetItem'+stat+'Bonus' for stat in ['Hp','Pow','Skl','Spd','Def','Res','Lck']]
unitFunctions=['GetUnitMaxHp','GetUnitPower','GetUnitSkill','GetUnitSpeed','GetUnitDefense','GetUnitResistance','GetUnitLuck','CountRedUnits','CountGreenUnits','SetUnitStatus','UnitRemoveInvalidItems','UnitHasMagicRank','GetUnitLastItem']
battleFunctions=['BattleGenerateSimulation','BattleGenerateSimulationInternal','BattleGenerate','InitBattleUnit','SetBattleUnitWeapon','ComputeBattleUnitSupportBonuses','ComputeBattleUnitStats','ComputeBattleUnitEffectiveStats','ComputeBattleUnitDefense','ComputeBattleUnitAttack','ComputeBattleUnitSpeed','ComputeBattleUnitHitRate','ComputeBattleUnitAvoidRate','ComputeBattleUnitCritRate','ComputeBattleUnitDodgeRate','ComputeBattleUnitEffectiveHitRate','ComputeBattleUnitEffectiveCritRate','ComputeBattleUnitSilencerRate','ComputeBattleUnitWeaponRankBonuses','ComputeBattleUnitStatusBonuses','ComputeBattleUnitSpecialWeaponStats','BattleApplyWeaponTriangleEffect','BattleApplyReaverEffect','BattleInitTargetCanCounter','ClearBattleHits','BattleUpdateBattleStats','BattleGenerateHitScriptedDamage','BattleGenerateHitEffects','BattleUnwindScripted','BattleHitGetInfo','BattleRoll1RN','BattleRoll2RN','BattleUnwind','BattleGetBattleUnitOrder','BattleGetFollowUpOrder','BattleGenerateRoundHits','GetBattleUnitHitCount','BattleCheckBraveEffect','BattleCheckSureShot','BattleCheckPierce','BattleCheckGreatShield','BattleCheckSilencer','BattleCheckPetrify','BattleGenerateHitAttributes','BattleGenerateHitTriangleAttack','BattleGenerateHit','BattleApplyExpGains','CanBattleUnitGainLevels','CheckBattleUnitLevelUp','CheckBattleUnitStatCaps','GetBattleUnitUpdatedWeaponExp','UpdateUnitFromBattle','GetUnitExpLevel','GetUnitRoundExp','GetUnitPowerLevel','GetUnitClassKillExpBonus','GetUnitExpMultiplier','GetUnitKillExpBonus','ModifyUnitSpecialExp','GetBattleUnitExpGain','BattleCheckTriangleAttack','ComputeBattleUnitBaseDefense','BattleUnitTargetSetEquippedWeapon','BattleInitItemEffect','BattleInitItemEffectTarget','GetBattleUnitStaffExp','BattleApplyItemExpGains']
functions=[fn('src/bmitem.c',name) for name in itemFunctions]+[fn('src/bmunit.c',name) for name in unitFunctions]+[fn('src/bmbattle.c',name) for name in battleFunctions]
functions=[f.replace('gItemData + itemIndex','gItemData + (itemIndex&255)') for f in functions]
functions += [fn('src/popup.c','NewPopup_GeneralItemGot'),fn('src/bmmind.c','BATTLE_HandleItemDrop').replace('bool BATTLE_HandleItemDrop','int BATTLE_HandleItemDrop')]
functions += [fn('src/bmitem.c','IsItemStealable'),fn('src/bmunit.c','UnitRemoveItem'),fn('src/bmbattle.c','BattleApplyMiscActionExpGains')]

for i,f in enumerate(functions):
 if f.startswith('s8 BattleCheckTriangleAttack'):
  f=f.replace('int uId = gBmMapUnit[adjacentLookup[i*2 + 1] + y][adjacentLookup[i*2 + 0] + x];','int uId = fe8_unit_at(adjacentLookup[i*2 + 0] + x,adjacentLookup[i*2 + 1] + y);if(uId<0)uId=0;')
 if f.startswith(('void BattleInitItemEffect(', 'void BattleInitItemEffectTarget(')):f=f.replace('SetBattleUnitTerrainBonusesAuto','Terrain')
 if f.startswith('void BattleInitItemEffect('):f=f.replace('int item = actor->items[itemSlot];','int item = itemSlot < 0 ? 0 : actor->items[itemSlot];')
 if f.startswith('void BattleGenerateSimulationInternal'):
  f=f.replace('    if (gBattleStats.config & BATTLE_CONFIG_BALLISTA)\n        SetBattleUnitWeaponBallista(&gBattleActor);\n    else\n        SetBattleUnitWeapon(&gBattleActor, actorWpnSlot);','    SetBattleUnitWeapon(&gBattleActor, actorWpnSlot); // Ballista refused by native entry guard.')
  f=f.replace('    DisableAllLightRunes();','    // Light-rune traps not supported by this prologue entry.').replace('    EnableAllLightRunes();','').replace('SetBattleUnitTerrainBonusesAuto','Terrain')
 if f.startswith('void BattleGenerate('):
  f=f.replace('    if (target == NULL)\n        ComputeBattleObstacleStats();','    // Target NULL/obstacles refused by native entry guard.')
 functions[i]=f
(root/'web/generated/battle_routines.h').write_text('\n'.join(f[:f.index('{')].strip()+';' for f in functions)+'\n'+'\n'.join(functions))
(root/'web/generated/battle_assets.json').write_text(json.dumps({'items':len(entries),'routines':itemFunctions+unitFunctions+battleFunctions,'boundaries':['support bonuses use relocated original support/affinity tables','magic seal and unusable locks rejected','ballista and arena rejected','normal physical combat uses original hit/EXP/result routines; map presentation adapted; AI execution and death events pending','simulation uses three original routines in SIMULATE mode, with isolated BattleUnit/Hit results and restored combat state','simulation guards reject ballista/magic/stat-bonus equipment/obstacles; light-rune trap toggling not integrated' ]},indent=2))
print(f'Battle: {len(entries)} item records and {len(functions)} original routines for native scripted and normal combat')

(root/'web/generated/battle_obstacle.h').write_text(fn('src/bmbattle.c','ComputeBattleObstacleStats'))
steal=fn('src/bmmind.c','ActionSteal').replace('BattleApplyMiscAction(proc);','BattleApplyMiscActionExpGains(); // Native presentation has no blocking map-animation Proc.').replace('    EndAllMus();','    (void)proc;').replace('    BeginMapAnimForSteal();','    // Native UI presents the transferred item and EXP; full steal animation pending.')
(root/'web/generated/battle_steal.h').write_text(steal)
