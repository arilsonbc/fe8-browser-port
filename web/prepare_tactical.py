from pathlib import Path
import re,hashlib,subprocess,json
root=Path(__file__).resolve().parent.parent;rom=(root/'baserom.gba').read_bytes();assert hashlib.sha1(rom).hexdigest()=='c25b145e37456171ada4b0d440bf88a19f4d509f'
symbols={}
for line in subprocess.check_output(['arm-none-eabi-nm','-S','fireemblem8.elf'],cwd=root,text=True).splitlines():
 p=line.split()
 if len(p)==4:symbols[p[3]]=(int(p[0],16),int(p[1],16))
 elif len(p)==3:symbols[p[2]]=(int(p[0],16),0)
def fn(file,name):
 s=(root/file).read_text();m=re.search(r'(?:(?:static )?inline )?(?:const s8\*|void|int|bool|s8|u8) '+name+r'\([^\n]*\)\s*\{',s);assert m,name
 p=m.end();depth=1
 while depth:
  if s[p]=='{':depth+=1
  if s[p]=='}':depth-=1
  p+=1
 return s[m.start():p].replace('static inline ','static ').replace('inline ','')
functions=[];manifest=[]
for file,names in [('src/bmphase.c',['GetPhaseAbleUnitCount','CountUnitsInState','AreUnitsAllied','IsSameAllegiance','GetCurrentPhase','GetNonActiveFaction']),('src/bm.c',['SwitchPhases']),('src/playerphase.c',['GetPlayerSelectKind','CanMoveActiveUnitTo']),('src/bmunit.c',['GetUnitMovementCost','UnitBeginAction','MoveActiveUnit','ClearActiveFactionGrayedStates','GetUnitCurrentHp','UnitBeginCantoAction','CanUnitMove','GetUnitAid','CanUnitRescue','UnitRescue','UnitDrop','UnitGive','CanUnitCrossTerrain','UnitFinalizeMovement','UnitGetDeathDropLocation']),('src/bmidoten.c',['SetWorkingMoveCosts','GenerateUnitMovementMapExt','GenerateMovementMap','sub_801A570','GenerateBestMovementScript','RevertMovementScript','GenerateExtendedMovementMap'])]:
 for name in names:
  f=fn(file,name);manifest.append({'name':name,'source':file})
  if name=='sub_801A570':
   f=f.replace('    tileMovementCost =', '    if(x<0||y<0||x>=gBmMapSize.x||y>=gBmMapSize.y)return;\n    if(gMovMapFillState.dst>=poolEnd){floodError=1;return;}\n    tileMovementCost =')
  if name=='GenerateBestMovementScript':
   f=f.replace('|= 0xFF','= 0xFF').replace('        // Build neighbor cost list','        if(output-outputStart>=MOVE_CMD_MAX_COUNT-1){floodError=2;*outputStart=MOVE_CMD_HALT;return;}\n        // Build neighbor cost list')
  if name=='UnitGive':
   f=f.replace('    // return couldGive; // devs probably forgot to add this', '    return couldGive; // Defined native return; gameplay uses the prevalidated transfer.')
  if name=='CanUnitMove':
   f=f.replace('        int cost;', '        int cost;\n        if(xLocal<0||yLocal<0||xLocal>=gBmMapSize.x||yLocal>=gBmMapSize.y)continue;')
  if name=='UnitFinalizeMovement':
   f=re.sub(r'    if \(unit->state & US_IN_BALLISTA\) \{.*?\n    \}', '    // Ballista branch refused by native selection/entry guards.', f, flags=re.S)
  functions.append(f)
parts=['#include "constants/terrains.h"']
for name in ['MovMapFillStateExt','MovMapFillState']:parts.append(re.search(r'struct '+name+r'\s*\{.*?\};',(root/'include/bmidoten.h').read_text(),re.S).group())
parts+=re.findall(r'enum\s*\{[^}]*PLAYER_SELECT_NOUNIT[^}]*\};',(root/'include/playerphase.h').read_text(),re.S)
parts+=re.findall(r'enum\s*\{[^}]*WEATHER_FINE[^}]*\};',(root/'include/types.h').read_text(),re.S)
parts+=['enum {MOVE_CMD_MOVE_LEFT=0,MOVE_CMD_MOVE_RIGHT=1,MOVE_CMD_MOVE_DOWN=2,MOVE_CMD_MOVE_UP=3,MOVE_CMD_HALT=4,MOVE_CMD_MAX_COUNT=64,BM_FLAG_PREPSCREEN=0x10,MAP_MOVEMENT_MAX=120};']
for name,size in [('TerrainTable_MovCost_FlyNormal',65),('Unk_TerrainTable_0880BC18',65),('gUnknown_08A02EB4',128),('gUnknown_08A02F34',96)]:
 a=symbols[name][0]-0x8000000;data=rom[a:a+size];word=name=='gUnknown_08A02F34';signed=name in ['Unk_TerrainTable_0880BC18','TerrainTable_MovCost_FlyNormal'];values=[int.from_bytes(data[p:p+2],'little') for p in range(0,size,2)] if word else [int.from_bytes(bytes([v]),'little',signed=signed) for v in data]
 parts.append('static const '+('u16' if word else 's8' if signed else 'u8')+' '+name+'[]={'+','.join(map(str,values))+'};')
source=(root/'web/generated/menu_assets.h').read_text()
for name in ['gUiFrameImage','gUiFramePaletteA']:
 parts.append(re.search(r'static const (?:u8|u16) '+name+r'\[\]=\{[^}]*\};',source).group())
base=symbols['sUiFrameModelTilemapLookup'][0]-0x8000000;ptr=int.from_bytes(rom[base:base+4],'little')-0x8000000
parts.append('static const u16 tacticalFrameModel[]={'+','.join(str(int.from_bytes(rom[p:p+2],'little')) for p in range(ptr,ptr+32,2))+'};')
parts.append('static const u16 *sUiFrameModelTilemapLookup[]={tacticalFrameModel};')
canto=fn('src/playerphase.c','TryMakeCantoUnit');canto=canto.replace('    BmMapFill(gBmMapRange, 0);','    // Native movement range is rebuilt by caller.').replace('    EndAllMus();','').replace('    StartMu(gActiveUnit);','').replace('    SetAutoMuDefaultFacing();','').replace('        Proc_Goto(proc, 4);','        (void)proc;').replace('        Proc_Goto(proc, 1);','        (void)proc;');functions.append(canto);manifest.append({'name':'TryMakeCantoUnit','source':'src/playerphase.c','adaptation':'MU/camera Proc presentation is replaced by native movement state'})
functions.append(fn('src/bmtrade.c','TradeMenu_ApplyItemSwap'));manifest.append({'name':'TradeMenu_ApplyItemSwap','source':'src/bmtrade.c'})
functions.append(fn('src/bmtarget.c','TryAddRescuedUnitToTakeTargetList'));manifest.append({'name':'TryAddRescuedUnitToTakeTargetList','source':'src/bmtarget.c'})
functions.append(fn('src/bmtarget.c','TryAddUnitToGiveTargetList'));manifest.append({'name':'TryAddUnitToGiveTargetList','source':'src/bmtarget.c'})
functions.append(fn('src/bmtarget.c','TryAddUnitToRescueTargetList'));manifest.append({'name':'TryAddUnitToRescueTargetList','source':'src/bmtarget.c'})
functions.append(fn('src/bmtarget.c','TryAddToDropTargetList'));manifest.append({'name':'TryAddToDropTargetList','source':'src/bmtarget.c'})
functions.append(fn('src/bmtarget.c','TryAddUnitToTradeTargetList'));manifest.append({'name':'TryAddUnitToTradeTargetList','source':'src/bmtarget.c'})
functions.append(fn('src/uiutils.c','DrawUiFrame'));manifest.append({'name':'DrawUiFrame','source':'src/uiutils.c'})
textSource=(root/'texts/texts.txt').read_text()
for name,ident in [('tacticalSteal',0x67f),('tacticalChest',0x683),('tacticalDoor',0x684),('tacticalAttack',0x67b),('tacticalSeize',0x67a),('tacticalStaff',0x67c),('tacticalItem',0x68d),('tacticalTrade',0x68e),('tacticalRescue',0x689),('tacticalDrop',0x68a),('tacticalTake',0x68b),('tacticalGive',0x68c),('tacticalTalk',0x680),('tacticalVisit',0x682),('tacticalWait',0x695),('tacticalCancel',0x24),('tacticalEnd',0x6a0)]:
 message=re.search(r'^## MSG_'+format(ident,'03X')+r'\n(.*?)(?=^## |\Z)',textSource,re.M|re.S).group(1)
 text=re.sub(r'\[[^]]*\]','',message).strip();parts.append('static const char '+name+'[]='+json.dumps(text)+';')
# Item names retain the original IDs decoded by prepare_events.py.
itemNames=json.loads(re.search(r'globalThis.FE8ItemNames=(.*);', (root/'web/event-texts.js').read_text()).group(1))
parts.append('static const char *tacticalItemNames[256]={'+','.join('['+key+']='+json.dumps(value['name']) for key,value in itemNames.items())+'};')
for name,ident in [('tacticalInventoryFull',0x866),('tacticalInventorySupply',0x867)]:
 message=re.search(r'^## MSG_'+format(ident,'03X')+r'\n(.*?)(?=^## |\Z)',textSource,re.M|re.S).group(1)
 text=re.sub(r'\[[^]]*\]','',message).strip();parts.append('static const char '+name+'[]='+json.dumps(text)+';')
(root/'web/generated/tactical_assets.h').write_text('\n'.join(parts))
(root/'web/generated/tactical_routines.h').write_text('\n'.join(f[:f.index('{')].strip()+';' for f in functions)+'\n'+'\n'.join(functions))
(root/'web/generated/tactical_assets.json').write_text(json.dumps({'routines':manifest,'flood':'Original C test step, ARM core neighbor order; bounds and pool guards added','path':'Original randomized tie choice; uninitialized edge OR replaced with assignment'},indent=2))
print(f'Tactical: {len(functions)} original selection, movement/path and phase routines')
steal=[fn('src/bmtarget.c','AddAsTarget_IfCanStealFrom'),fn('src/bmmenu.c','StealCommandUsability'),fn('src/bmmenu.c','StealItemMenuCommand_Usability')]
(root/'web/generated/tactical_steal.h').write_text('\n'.join(steal))
