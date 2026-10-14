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
print('Loot AI: 7 original routines; special keys/rogue/chests remain guarded')
