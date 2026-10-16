from pathlib import Path
import re,json
root=Path(__file__).resolve().parent.parent
parts=['struct EventInfo {const u32 *listScript;u32 script,flag,commandId,givenMoney,givenItem;s8 xPos,yPos;u8 pidA,pidB;};','struct EvCheck01 {u32 unk0,script,unk8;};','struct EvCheck02 {u32 unk0,script;u8 turn,maxTurn;u16 faction;};','#define EVT_CMD_HI(cmd) ((cmd)>>16)','#define AI_A_INVALID 21','#define AI_B_INVALID 19','#define AI_B_0C 12','#define AI_UNIT_FLAG_3 8','#define FACTION_PURPLE 192','struct EvCheck0B {u32 unk0,script,unk8;};','#define EVT_CMD_B1(x) ((x)&255)','#define EVT_CMD_B2(x) ((x)>>8&255)','#define EVT_CMD_B3(x) ((x)>>16&255)','#define EVT_CMD_B4(x) ((x)>>24&255)']
names=[('src/eventinfo.c','EvCheck01_AFEV'),('src/eventinfo.c','EvCheck02_TURN'),('src/eventinfo.c','EvCheck0B_AREA'),('src/eventscr3.c','ChangeUnitAi'),('src/eventscr3.c','ChangeAiForCharacter'),('src/uidebug.c','AreAnyEnemyUnitDead')]
for file,name in names:
 s=(root/file).read_text();m=re.search(r'(?:int|void) '+name+r'\([^\n]*\)\s*\{',s);assert m,name;p=m.end();depth=1
 while depth:
  if s[p]=='{':depth+=1
  if s[p]=='}':depth-=1
  p+=1
 parts.append(s[m.start():p])
(root/'web/generated/chapter_trigger_routines.h').write_text('\n'.join(parts))
(root/'web/generated/chapter_trigger_assets.json').write_text(json.dumps({'routines':names,'limits':['Known chapters 0..4 turn/misc lists','FIFO bounded to four lists; unknown callbacks remain guarded','call preserves flags/tactical state/SRAM']},indent=2))
print('Chapter triggers: original turn/AFEV checks and ChangeUnitAi')
