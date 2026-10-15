from pathlib import Path
import re,hashlib,json
root=Path(__file__).resolve().parent.parent
rom=(root/'baserom.gba').read_bytes()
assert hashlib.sha1(rom).hexdigest()=='c25b145e37456171ada4b0d440bf88a19f4d509f'
maptext=(root/'fireemblem8.map').read_text()
def addr(name):return int(re.search(r'(0x[\da-f]+)\s+'+name+r'\b',maptext).group(1),16)-0x8000000
def lz(p):
 assert rom[p]==16
 size=int.from_bytes(rom[p+1:p+4],'little');p+=4;out=bytearray()
 while len(out)<size:
  flags=rom[p];p+=1
  for bit in range(7,-1,-1):
   if len(out)>=size:break
   if flags&(1<<bit):
    a,b=rom[p:p+2];p+=2;n=(a>>4)+3;d=((a&15)<<8|b)+1
    assert d<=len(out)
    for _ in range(n):
     if len(out)<size:out.append(out[-d])
   else:out.append(rom[p]);p+=1
 return bytes(out)
assets={};parts=['// Verified ROM resources. Native map data, no ROM execution.'];manifest=[]
def asset(pointer,palette=False):
 if not pointer:return ('NULL',0,b'')
 p=pointer-0x8000000;key=(p,palette)
 if key not in assets:
  data=rom[p:p+320] if palette else lz(p)
  name='map_asset_'+str(len(assets));assets[key]=(name,len(data),data)
  typ='u16' if palette or not len(data)%2 else 'u8'
  values=list(data) if typ=='u8' else [int.from_bytes(data[i:i+2],'little') for i in range(0,len(data),2)]
  parts.append('static const '+typ+' '+name+'[]={'+','.join(map(str,values))+'};')
 return assets[key]
chap=addr('gChapterDataTable');table=addr('gChapterDataAssetTable');entries=[]
for i in range(79):
 meta=rom[chap+i*148:chap+(i+1)*148]
 ids=meta[4:12];data=[]
 for k in [4,3,0,1,2]:
  pointer=int.from_bytes(rom[table+ids[k]*4:table+ids[k]*4+4],'little')
  data.append(asset(pointer,k==2))
 mapdata=data[0][2];w,h=mapdata[:2]
 assert 0<w<=64 and 0<h<=64 and len(mapdata)>=2+w*h*2
 config=data[1][2]
 assert len(config)==0x2400
 assert data[2][1]<=0x8000 and data[3][1]<=0x4000, (i,data[2][1],data[3][1])
 for p in range(2,2+w*h*2,2):
  tile=int.from_bytes(mapdata[p:p+2],'little');assert tile%4==0 and tile+3<0x1000
 entries.append('{'+','.join([data[0][0],data[1][0],'(const u8 *)'+data[2][0],str(data[2][1]),'(const u8 *)'+data[3][0],str(data[3][1]),data[4][0],str(meta[16]),str(meta[17])])+'}')
 manifest.append({'chapter':i,'width':w,'height':h,'initial':[meta[16],meta[17]],'assets':[{'bytes':x[1],'sha256':hashlib.sha256(x[2]).hexdigest()} for x in data]})
parts.append('static const struct MapAsset chapters[]={'+','.join(entries)+'};')
ui=lz(addr('gGfx_MiscUiGraphics'));pal=rom[addr('gPal_MiscUiGraphics'):addr('gPal_MiscUiGraphics')+64]
parts+=['static const u8 mapUi[]={'+','.join(map(str,ui))+'};','static const u16 mapUiPalette[]={'+','.join(str(int.from_bytes(pal[i:i+2],'little')) for i in range(0,64,2))+'};']
changeTables=[]
for i in range(79):
 meta=rom[chap+i*148:chap+(i+1)*148];pointer=int.from_bytes(rom[table+meta[11]*4:table+meta[11]*4+4],'little');records=[]
 if pointer:
  p=pointer-0x8000000
  for n in range(128):
   record=rom[p+n*12:p+(n+1)*12]
   if record[0]>=128:break
   ident,x,y,w,h=record[:5];assert w and h
   dataPtr=int.from_bytes(record[8:12],'little')-0x8000000
   data=rom[dataPtr:dataPtr+w*h*2];values=[int.from_bytes(data[k:k+2],'little') for k in range(0,len(data),2)]
   assert x+w<=manifest[i]['width'] and y+h<=manifest[i]['height'],(i,ident)
   assert all(v%4==0 and v+3<0x1000 for v in values)
   name='map_change_'+str(i)+'_'+str(n);parts.append('static const u16 '+name+'[]={'+','.join(map(str,values))+'};')
   records.append('{'+','.join(map(str,[ident,x,y,w,h]))+','+name+'}')
  else:raise RuntimeError('Unterminated map change table')
 name='map_changes_'+str(i);parts.append('static const struct MapChange '+name+'[]={'+','.join(records+['{-1,0,0,0,0,NULL}'])+'};');changeTables.append(name)
parts.append('static const struct MapChange *mapChanges[]={'+','.join(changeTables)+'};')
(root/'web/generated/map_assets.h').write_text('\n'.join(parts))
(root/'web/generated/map_assets.json').write_text(json.dumps(manifest,indent=2))
def function(file,name):
 s=(root/file).read_text();m=re.search(r'(?:void|int|const struct MapChange\*) '+name+r'\([^\n]*\)\s*\{',s);assert m,name
 end=m.end();depth=1
 while depth:
  if s[end]=='{':depth+=1
  if s[end]=='}':depth-=1
  end+=1
 return s[m.start():end]
s=(root/'src/bm.c').read_text();routines=[re.search(r's8 CONST_DATA sDirKeysToOffsetLut\[\]\[2\] = \{.*?\};',s,re.S).group()]
for name in ['sSprite_MapCursorA','sSprite_MapCursorB','sSprite_MapCursorC','sSprite_MapCursorStretched']:
 routines.append(re.search(r'u16 CONST_DATA '+name+r'\[\] = \{.*?\};',s,re.S).group())
routines.append(re.search(r'u16\* CONST_DATA sMapCursorSpriteLut\[\] = \{.*?\};',s,re.S).group())
for f,names in [('src/bm.c',['HandleMapCursorInput','HandleMoveMapCursor','HandleMoveCameraWithMapCursor','SetCursorMapPosition','PutMapCursor']),('src/bmmap.c',['RefreshTerrainBmMap','GetTrueTerrainAt','DisplayBmTile'])]:
 routines +=[function(f,n) for n in names]
routines += [function('src/bmtrick.c',n) for n in ['GetMapChange','GetMapChangeIdAt','ApplyMapChangesById']]
routines.append(function('src/bmmap.c','RevertMapChange'))
(root/'web/generated/map_routines.h').write_text('\n'.join(routines))
print('Maps: 79 verified chapter records, '+str(len(assets))+' deduplicated assets, 12 original routines')


base=addr('gChapterDataTable')
with (root/'web/generated/map_assets.h').open('a') as out:out.write('\nstatic const u8 chapterWallHp[]={'+','.join(str(rom[base+i*148+44]) for i in range(79))+'};')
