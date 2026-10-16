from pathlib import Path
import re,hashlib,subprocess,json
root=Path(__file__).resolve().parent.parent;rom=(root/'baserom.gba').read_bytes();assert hashlib.sha1(rom).hexdigest()=='c25b145e37456171ada4b0d440bf88a19f4d509f'
symbols={}
for line in subprocess.check_output(['arm-none-eabi-nm','-S','fireemblem8.elf'],cwd=root,text=True).splitlines():
 p=line.split()
 if len(p)==4:symbols[p[3]]=(int(p[0],16),int(p[1],16))
byAddress={a:n for n,(a,_) in symbols.items()};parts=[];manifest=[];seen={};tableCounts={}
def script(address):
 if address in seen:return seen[address]
 name=byAddress[address];a,n=symbols[name];assert n and n%16==0 and n//16<256
 data=rom[a-0x8000000:a-0x8000000+n];rows=[];labels={data[i+3] for i in range(0,n,16) if data[i]==28}
 for i in range(0,n,16):
  row=data[i:i+16]
  if row[0]==3 and row[3]:assert row[3] in labels,name
  rows.append('{'+','.join(map(str,row[:4]))+','+str(int.from_bytes(row[4:8],'little'))+',(const void*)(uintptr_t)'+str(int.from_bytes(row[8:12],'little'))+',(const void*)(uintptr_t)'+str(int.from_bytes(row[12:16],'little'))+'}')
 ident='aiVmScript_'+str(len(seen));seen[address]=ident;parts.append('static struct AiScr '+ident+'[]={'+','.join(rows)+'};');manifest.append({'name':name,'address':address,'commands':n//16,'sha256':hashlib.sha256(data).hexdigest()});return ident
for table in ['gAi1ScriptTable','gAi2ScriptTable']:
 a,n=symbols[table];assert n%4==0;entries=[];lengths=[]
 for i in range(n//4):
  address=int.from_bytes(rom[a-0x8000000+i*4:a-0x8000000+i*4+4],'little');entries.append(script(address));lengths.append(symbols[byAddress[address]][1]//16)
 tableCounts[table]=len(entries)
 ident='aiVmA' if table=='gAi1ScriptTable' else 'aiVmB';parts.append('static struct AiScr *'+ident+'[]={'+','.join(entries)+'};');parts.append('static const int '+ident+'Lengths[]={'+','.join(map(str,lengths))+'};')
for name in ['gAiScript_FallbackAi1','gAiScript_FallbackAi2']:parts.append('#define '+name+' '+script(symbols[name][0]))
(root/'web/generated/ai_vm_assets.h').write_text('\n'.join(parts));(root/'web/generated/ai_vm_assets.json').write_text(json.dumps({'scripts':manifest,'tables':tableCounts,'keys':'callback/data addresses are keys only; unsupported handlers never dereference or execute them'},indent=2))
source=(root/'src/cp_script.c').read_text()
names=['AiTryExecScriptA','AiTryExecScriptB','AiExecFallbackScriptA','AiExecFallbackScriptB','AiScript_Exec','AiScriptCmd_00_ConditionalGoto','AiScriptCmd_02_ChangeAi','AiScriptCmd_03_Goto','AiScriptCmd_06_DoNothing','AiScriptCmd_0E_DoNothing','AiScriptCmd_17_DoEscape','AiScriptCmd_14_DoNothing','AiScriptCmd_15_DoNothing','AiScriptCmd_1B_NoOp','AiScriptCmd_0D_MoveTowardsCharacterUntilInRange','AiScriptCmd_12_MoveTowardsEnemy','AiIsUnitEnemy','AiIsUnitEnemyAndNotInScrList']
functions=[]
for name in names:
 m=re.search(r'(?:s8|void) '+name+r'\([^\n]*\)\s*\{',source);assert m,name;p=m.end();depth=1
 while depth:
  if source[p]=='{':depth+=1
  if source[p]=='}':depth-=1
  p+=1
 f=source[m.start():p]
 if name=='AiScriptCmd_00_ConditionalGoto':f=f.replace('AiCompare(gpAiScriptCurrent->unk_08','AiVmCompareKey(gpAiScriptCurrent->unk_08')
 if name=='AiScript_Exec':f=re.sub(r'= (AiScriptCmd_\w+),',lambda m:m.group() if m.group(1) in names else '= AiNativeUnsupported,',f)
 if name=='AiScriptCmd_12_MoveTowardsEnemy':
  f=f.replace('    struct Vec2 pos;','    struct Vec2 pos;\n    if(gpAiScriptCurrent->unk_08&&!ResolveAiShortList(gpAiScriptCurrent->unk_08)){vmError=6;return;}')
 if name=='AiIsUnitEnemyAndNotInScrList':f=f.replace('gpAiScriptCurrent->unk_08,','ResolveAiShortList(gpAiScriptCurrent->unk_08),')
 functions.append(f)
for file,name in [('src/cp_utility.c','AiIsInShortList'),('src/cp_utility.c','AiCompare'),('src/cp_utility.c','AiFindTargetInReachByCharId'),('src/cp_utility.c','AiFindTargetInReachByFunc'),('src/cp_utility.c','AiTryMoveTowards'),('src/cp_utility.c','AiIsWithinRectDistance'),('src/cp_decide.c','AiClearDecision'),('src/cp_decide.c','AiSetDecision'),('src/cp_decide.c','AiUpdateDecision')]:
 s=(root/file).read_text();m=re.search(r'(?:s8|void) '+name+r'\([^\n]*\)\s*\{',s);assert m,name;p=m.end();depth=1
 while depth:
  if s[p]=='{':depth+=1
  if s[p]=='}':depth-=1
  p+=1
 f=s[m.start():p]
 if name=='AiFindTargetInReachByCharId':
  f=f.replace('        if (gBmMapRange[unit->yPos][unit->xPos]', '        if(unit->xPos<0||unit->yPos<0||unit->xPos>=gBmMapSize.x||unit->yPos>=gBmMapSize.y)continue;\n        if (gBmMapRange[unit->yPos][unit->xPos]')
  f=f.replace('    if (!(GetUnitFromCharId(uid)->state & US_UNAVAILABLE))', '    struct Unit *target=GetUnitFromCharId(uid);if(!target){gAiState.cmd_result[0]=1;return 0;}\n    if (!(target->state & US_UNAVAILABLE))')
 if name=='AiTryMoveTowards':f=f.replace('    s16 ix;', '    if(maxDanger!=255&&!runtimeHealing){vmError=6;return;}\n    s16 ix;')
 functions.append(f)
parts.append('#define AI_VM_STATE_KEY '+str(symbols['gAiState'][0]))
(root/'web/generated/ai_vm_assets.h').write_text('\n'.join(parts))
header=(root/'include/cp_script.h').read_text();parts=re.findall(r'enum \w+\s*\{.*?\};',header,re.S)+[re.search(r'struct AiScr\s*\{.*?\};',header,re.S).group(),'typedef void(*AiScrCmd)(u8*);','enum {AI_SCRIPT_AI1,AI_SCRIPT_AI2};','_Static_assert(sizeof(struct AiScr)==16,"Native AI instruction ABI");']
common=(root/'include/cp_common.h').read_text();parts.append(re.search(r'struct AiDecision\s*\{.*?\};',common,re.S).group());parts.append(re.search(r'enum\s*\{[^}]*AI_COMPARE_GT[^}]*\};',common,re.S).group());parts.append('enum {AI_ACTION_NONE=0,MAP_MOVEMENT_MAX=120};')
(root/'web/generated/ai_vm_types.h').write_text('\n'.join(parts))
(root/'web/generated/ai_vm_routines.h').write_text('\n'.join(f[:f.index('{')].strip()+';' for f in functions)+'\n'+'\n'.join(functions))
print(f'AI VM: {len(manifest)} verified scripts and {len(functions)} original routines; known branch/movement planning; offensive/staff/callback paths guarded')

# Relocate only bounded original short lists used by known action/movement scripts.
keys=set()
for address in seen:
 a,n=symbols[byAddress[address]];data=rom[a-0x8000000:a-0x8000000+n]
 for i in range(0,n,16):
  key=int.from_bytes(data[i+8:i+12],'little')
  if data[i] in [5,18] and key:keys.add(key)
parts=[];entries=[]
for key in sorted(keys):
 assert key in byAddress,hex(key)
 name=byAddress[key];a,n=symbols[name];assert 0<n<=128 and n%2==0,name
 data=rom[a-0x8000000:a-0x8000000+n];values=[int.from_bytes(data[i:i+2],'little') for i in range(0,n,2)];assert 0 in values,name
 values=values[:values.index(0)+1];ident='aiShortList_'+str(len(entries));parts.append('static const u16 '+ident+'[]={'+','.join(map(str,values))+'};');entries.append('{'+str(key)+','+ident+'}')
parts.append('static const struct {u32 key;const u16 *data;} aiShortLists[]={'+','.join(entries)+'};')
(root/'web/generated/ai_target_lists.h').write_text('\n'.join(parts))
