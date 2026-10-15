from pathlib import Path
import re
root=Path(__file__).resolve().parent.parent
def function(file,name):
 s=(root/file).read_text();m=re.search(r'(?:void|s8|u8|int) '+name+r'\([^\n]*\)\s*\{',s);assert m,name;i=m.end();depth=1
 while depth:
  if s[i]=='{':depth+=1
  if s[i]=='}':depth-=1
  i+=1
 return s[m.start():i]
parts=[]
for name in ['gTerrainList_LootableVillages','gTerrainList_LootableVillagesAndChests']:
 parts.append(re.search(r'u8 CONST_DATA '+name+r'\[\] = .*?;', (root/'src/cp_utility.c').read_text()).group().replace('CONST_DATA',''))
functions=[function('src/bmunit.c','GetUnitItemCount')]+[function('src/cp_utility.c',n) for n in ['AiGetChestUnlockItemSlot','AiFindClosestTerrainPosition','AiFindPillageLocation','AiLocationIsPillageTarget']]+[function('src/cp_script.c',n) for n in ['AiScriptCmd_10_DoLooting','AiScriptCmd_11_MoveTowardsSafety']]
(root/'web/generated/ai_loot.h').write_text('\n'.join(parts+[f[:f.index('{')].strip()+';' for f in functions]+functions))
print('Loot AI: 7 original routines; native keys/chests and stealing integrated; rogue remains guarded')

# Original special-item selection for known lockpick/door-key branches.
import subprocess
symbols={p[-1]:int(p[0],16) for line in subprocess.check_output(['arm-none-eabi-nm','-S','fireemblem8.elf'],cwd=root,text=True).splitlines() if len(p:=line.split()) in [3,4]}
rom=(root/'baserom.gba').read_bytes();a=symbols['gAiItemConfigTable']-0x8000000
parts=['static const u32 nativeAiItemConfig[]={'+','.join(str(int.from_bytes(rom[a+i*4:a+i*4+4],'little')) for i in range(79))+'};','static const u8 gUnknown_085A92DC[]={TERRAIN_DOOR,0};','static const u8 gUnknown_085A92DE[]={TERRAIN_CHEST_FULL,0};']
funcs=[function('src/cp_utility.c',n) for n in ['AiGetPositionRange','AiFindBestAdjacentPositionByFunc','AiFindClosestTerrainAdjacentPosition','AiFindClosestUnlockPosition']]
funcs += [function('src/cpextra_80407F0.c',n) for n in ['GetSpecialItemFuncIndex','AiSpecialItemDoorKey','AiSpecialItemLockpick','AiSpecialItemAntitoxin','sub_8040B38','sub_8040B8C','sub_8040BB4','sub_8040E04','InitAiMoveMapForUnit','sub_8040E98','sub_8040F88','sub_8040FBC','AiTryDoSpecialItems']]
funcs[0]=funcs[0].replace('    if (gMapRangeSigned','    if(x<0||y<0||x>=gBmMapSize.x||y>=gBmMapSize.y)return 255;\n    if (gMapRangeSigned')
parts += [f[:f.index('{')].strip()+';' for f in funcs]
parts += ['struct AiSpecialItemLutEntry {u16 itemId;void(*func)(int);};','static const struct AiSpecialItemLutEntry sAiSpecialItemFuncLut[]={{ITEM_DOORKEY,AiSpecialItemDoorKey},{ITEM_LOCKPICK,AiSpecialItemLockpick},{ITEM_ANTITOXIN,AiSpecialItemAntitoxin},{0}};']
parts += funcs
(root/'web/generated/ai_keys.h').write_text('\n'.join(parts))

a=symbols['gAiStealPriorityItemList']-0x8000000;items=[]
for i in range(256):
 item=int.from_bytes(rom[a+i*2:a+i*2+2],'little');items.append(item)
 if item==65535:break
else:raise RuntimeError('No steal list terminator')
funcs=[function('src/cp_utility.c',n) for n in ['AiGetItemStealRank','AiGetUnitStealItemSlot']]+[function('src/cp_battle.c',n) for n in ['AiAttemptStealAction_GetMovementAt','AiAttemptStealActionWithinMovement']]
funcs[2]=funcs[2].replace('    if (gMapMovementSigned','    if(x<0||y<0||x>=gBmMapSize.x||y>=gBmMapSize.y)return 255;\n    if (gMapMovementSigned')
(root/'web/generated/ai_steal.h').write_text('static u16 gAiStealPriorityItemList[]={'+','.join(map(str,items))+'};\n'+'\n'.join(f[:f.index('{')].strip()+';' for f in funcs)+'\n'+'\n'.join(funcs))
