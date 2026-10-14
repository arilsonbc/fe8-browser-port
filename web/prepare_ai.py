from pathlib import Path
import re,json
root=Path(__file__).resolve().parent.parent
def fn(file,name):
 s=(root/file).read_text();m=re.search(r'(?:inline )?int '+name+r'\([^\n]*\)\s*\{',s);assert m,name;p=m.end();depth=1
 while depth:
  if s[p]=='{':depth+=1
  if s[p]=='}':depth-=1
  p+=1
 return s[m.start():p].replace('inline ','')
names=[('src/bmunit.c','GetUnitLeaderCharId')]+[('src/cp_order.c',n) for n in ['GetUnitBattleAiPriority','GetUnitAiPriority','BuildAiUnitList','SortAiUnitList']]
# Sort returns void.
functions=[fn(file,n) for file,n in names[:-1]]
s=(root/'src/cp_order.c').read_text();m=re.search(r'void SortAiUnitList\(int count\)\s*\{',s);p=m.end();depth=1
while depth:
 if s[p]=='{':depth+=1
 if s[p]=='}':depth-=1
 p+=1
functions.append(s[m.start():p])
(root/'web/generated/ai_order_routines.h').write_text('\n'.join(f[:f.index('{')].strip()+';' for f in functions)+'\n'+'\n'.join(functions))
(root/'web/generated/ai_order_assets.json').write_text(json.dumps({'routines':names,'boundaries':['unit order only; decisions/actions/healing/inventory flags not yet integrated']},indent=2))
print('AI: five original unit-order/priority routines; decisions pending')
