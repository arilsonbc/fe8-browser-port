from pathlib import Path
import re,json
root=Path(__file__).resolve().parent.parent
names=[('src/bmidoten.c','MapAddInRange'),('src/bmidoten.c','MapAddInBoundedRange'),('src/cp_utility.c','AiCouldReachByBirdsEyeDistance'),('src/cp_utility.c','FillMovementAndRangeMapForItem'),('src/cp_0803E2F4.c','AiInitDangerMap'),('src/cp_0803E2F4.c','AiFillDangerMap')]
functions=[]
for file,name in names:
 source=(root/file).read_text();m=re.search(r'(?:inline )?(?:void|s8) '+name+r'\([^\n]*\)\s*\{',source);assert m,name;p=m.end();depth=1
 while depth:
  if source[p]=='{':depth+=1
  if source[p]=='}':depth-=1
  p+=1
 f=source[m.start():p].replace('inline ','')
 if name=='AiFillDangerMap':f=f.replace('        // BUG: Item', '        if(unit->xPos<0||unit->yPos<0||unit->xPos>=gBmMapSize.x||unit->yPos>=gBmMapSize.y)continue;\n        // BUG: Item')
 functions.append(f)
(root/'web/generated/ai_danger_routines.h').write_text('\n'.join(f[:f.index('{')].strip()+';' for f in functions)+'\n'+'\n'.join(functions))
(root/'web/generated/ai_danger_assets.json').write_text(json.dumps({'routines':[n for _,n in names],'adaptations':['invalid unit coordinates guarded before map access','original accumulated item/might bug retained','byte range and danger wrap retained','isolated planning API, not phase execution']},indent=2))
print('AI danger: six original routines; accumulated item/might and byte wrapping retained')
