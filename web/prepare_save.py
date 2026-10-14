from pathlib import Path
import re
root=Path(__file__).resolve().parent.parent
for p in (root/'web').rglob('*'):
 if p.is_file():p.read_bytes()
def struct(file,name):
 s=(root/file).read_text();m=re.search(r'struct '+re.escape(name)+r'\s*\{.*?\}\s*(?:BITPACKED)?;',s,re.S)
 if not m:raise RuntimeError(name+' missing')
 return m.group()
def function(file,name):
 s=(root/file).read_text();m=re.search(r'(?:void\s*\*|void|bool|u16|u32|int)\s*'+re.escape(name)+r'\([^\n]*\)\s*\{',s)
 if not m:raise RuntimeError(name+' missing')
 p=m.end();d=1
 while d:
  if s[p]=='{':d+=1
  if s[p]=='}':d-=1
  p+=1
 return s[m.start():p]
types=['#ifndef FE8_SAVE_TYPES_H','#define FE8_SAVE_TYPES_H','#include "../compat.h"','#include <stdbool.h>','#define BITPACKED __attribute__((packed))','#define UNIT_SAVE_AMOUNT_BLUE 51','#define UNIT_SUPPORT_MAX_COUNT 7','#define BWL_ARRAY_NUM 70','#define WIN_ARRAY_NUM 48','#define MAX_SAVED_GAME_CLEARS 12','#define GLOBALSIZEINFO_SIZE_FOR_CHECKSUM 0x50','#define CART_SRAM_SIZE 32768','#define SRAM_SIZE_SUSPEND 0x1f78','#define SRAM_SIZE_MARENA 0x874','#define SRAM_SIZE_XMAP 0xc00','#define SRAM_OFFSET_XMAP 0x7400','#define GAMESAVE_SIZE_SUPPLY 176','#define CONVOY_ITEM_COUNT 100','#define ITEM_USES(item) ((item)>>8)']
for file,name in [('include/types.h','PlaySt_30'),('include/types.h','PlaySt_OptionBits'),('include/types.h','PlaySt'),('include/bmsave.h','GlobalSaveInfo'),('include/bmsave.h','SaveBlockInfo'),('include/bmsave.h','GameSavePackedUnit'),('include/bmsave.h','UnitUsageStats'),('include/bmsave.h','ChapterStats'),('include/bmsave.h','GMapSaveInfo')]:
 value=struct(file,name)
 if name=="UnitUsageStats":value=value.replace("} BITPACKED;","    u8 web_original_tail_padding; // AGBCC original stride: 16 bytes, modern packed struct: 15.\n} BITPACKED;")
 types.append(value)
types.append('// Dungeon records are opaque until their runtime module is ported; original on-disk stride is 12 bytes.\nstruct Dungeon {u8 bytes[12];} __attribute__((aligned(4)));')
types.append(struct('include/bmsave.h','GameSaveBlock'))
for file,pattern in [('include/bmsave.h',r'enum save_chunk_index\s*\{.*?\};'),('include/bmsave.h',r'enum\s*\{[^}]*SAVEBLOCK_KIND_GAME[^}]*\};'),('include/bmsave.h',r'enum bmsave_magics_fe8\s*\{.*?\};'),('include/types.h',r'enum PlaySt_chapterStateBits\s*\{.*?\};')]:
 types.append(re.search(pattern,(root/file).read_text(),re.S).group())
types.append('''struct SuspendSaveBlock {struct PlaySt playSt;u8 rest[SRAM_SIZE_SUSPEND-sizeof(struct PlaySt)];};
struct SaveBlocks {struct GlobalSaveInfo globalSaveInfo;struct SaveBlockInfo saveBlockInfo[7];
 struct SuspendSaveBlock suspendSaveBlocks[2];struct GameSaveBlock gameSaveBlocks[3];
 u8 multiArenaBlock[0x874];u8 rest[0x8000-0x7190];};
_Static_assert(sizeof(struct PlaySt)==0x4c,"PlaySt ABI");
_Static_assert(sizeof(struct GlobalSaveInfo)==0x64,"GlobalSaveInfo ABI");
_Static_assert(sizeof(struct SaveBlockInfo)==16,"SaveBlockInfo ABI");
_Static_assert(sizeof(struct GameSavePackedUnit)==36,"Packed unit ABI");
_Static_assert(sizeof(struct GameSaveBlock)==0xdc8,"GameSaveBlock ABI");
_Static_assert(offsetof(struct SaveBlocks,gameSaveBlocks)==0x3fc4,"Game slots offset");
_Static_assert(offsetof(struct SaveBlocks,multiArenaBlock)==0x691c,"Arena offset");
_Static_assert(sizeof(struct SaveBlocks)==32768,"SRAM ABI");
#endif''')
(root/'web/generated/save_types.h').write_text('\n'.join(types))
groups={
 'src/bmsave-lib.c':['WipeSram','Checksum16','ReadGlobalSaveInfo','WriteGlobalSaveInfo','WriteGlobalSaveInfoNoChecksum','InitGlobalSaveInfodata','SramOffsetToAddr','SramAddrToOffset','ReadSaveBlockInfo','WriteSaveBlockInfo','GetSaveWriteAddr','GetSaveReadAddr','WriteSupplyItems','ReadSupplyItems','WritePermanentFlags'],
 'src/bmsave.c':['SetBonusContentClaimFlags','WriteBonusContentClaimFlags','WriteLastGameSaveId','ReadLastGameSaveId','InvalidateGameSave','CopyGameSave','WriteNewGameSave','IsSaveValid','ReadGameSavePlaySt','InvalidateSuspendSave','SramChecksum32','VerifySaveBlockChecksum','PopulateSaveBlockChecksum'],
 'src/bmsave-bwl.c':['ClearPidChStatsSaveData','IsPlaythroughIdUnique','GetNewPlaythroughId','GetGlobalCompletionCntByInfo','GetGlobalCompletionCount'],
 'src/bmio.c':['InitPlayConfig'], 'src/bmsave-gmap.c':['ClearWorldMapStuff']}
functions=[]
for file,names in groups.items():
 for name in names:
  f=function(file,name)
  # Native address boundary: preserve original SRAM offsets, never truncate a WASM pointer.
  f=f.replace('chunk->offset = (uintptr_t)GetSaveWriteAddr(index);','chunk->offset = SramAddrToOffset(GetSaveWriteAddr(index));')
  # Deterministic padding for native exports (original compiler left these bytes unspecified).
  f=re.sub(r'struct SaveBlockInfo (chunk|tmp);',r'struct SaveBlockInfo \1 = {0};',f)
  f=f.replace('struct GlobalSaveInfo info;','struct GlobalSaveInfo info = {0};').replace('unsigned char buf[176];','unsigned char buf[176] = {0};')
  if name=='ReadSaveBlockInfo':
   f=f.replace('    ReadSramFast(&gSram->saveBlockInfo[index]', '    if(index<0||index>=SAVE_ID_MAX)return false;\n    ReadSramFast(&gSram->saveBlockInfo[index]')
   f=f.replace('    return VerifySaveBlockChecksum(chunk);','    if(!ValidBlockRange(chunk,index))return false;\n    return VerifySaveBlockChecksum(chunk);')
  if name=='WriteSaveBlockInfo':
   f=f.replace('    chunk->magic16', '    if(index<0||index>=SAVE_ID_MAX)return;\n    chunk->magic16',1)
  if name=='GetSaveReadAddr':
   f=f.replace('    ReadSaveBlockInfo(&chunk, index);','    if(!ReadSaveBlockInfo(&chunk,index))return NULL;')
  if name=='GetNewPlaythroughId':f=f[:-1]+'    return 0; // Exhausted IDs: explicit failure instead of undefined return.\n}'
  functions.append(f)
prototypes=[f[:f.index('{')].strip()+';' for f in functions]
constants=(root/'src/bmsave-lib.c').read_text();lookup=re.findall(r'static const u8 sConvySavePackMask\d\[\]\s*=\s*\{.*?\};',constants,re.S)
(root/'web/generated/save_routines.h').write_text('\n'.join(lookup+prototypes+functions))
print('Save: '+str(len(functions))+' original routines and binary ABI extracted')

