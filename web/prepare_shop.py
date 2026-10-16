from pathlib import Path
import re,json,hashlib,subprocess
root=Path(__file__).resolve().parent.parent;rom=(root/'baserom.gba').read_bytes();assert hashlib.sha1(rom).hexdigest()=='c25b145e37456171ada4b0d440bf88a19f4d509f'
symbols={}
for line in subprocess.check_output(['arm-none-eabi-nm','-S','fireemblem8.elf'],cwd=root,text=True).splitlines():
 p=line.split()
 if len(p)==4:symbols[p[3]]=(int(p[0],16),int(p[1],16))
def fn(file,name):
 source=(root/file).read_text();m=re.search(r'(?:void|int|s8|u8|u16|bool) '+name+r'\([^\n]*\)\s*\{',source);assert m,name;p=m.end();depth=1
 while depth:
  if source[p]=='{':depth+=1
  if source[p]=='}':depth-=1
  p+=1
 return source[m.start():p]
parts=[];entries=[];manifest=[]
for name in ['ShopList_Event_Ch5Armory','ShopList_Event_Ch5Vendor']:
 a,n=symbols[name];data=rom[a-0x8000000:a-0x8000000+n];values=[int.from_bytes(data[i:i+2],'little') for i in range(0,n,2)];assert values[-1]==0 and len(values)<=20
 parts.append('static const u16 '+name+'[]={'+','.join(map(str,values))+'};');entries.append('{'+str(a)+','+name+','+str(len(values)-1)+'}');manifest.append({'name':name,'address':a,'items':values[:-1],'sha256':hashlib.sha256(data).hexdigest()})
parts.append('static const struct {u32 address;const u16 *items;int count;} shopLists[]={'+','.join(entries)+'};');(root/'web/generated/shop_assets.h').write_text('\n'.join(parts));(root/'web/generated/shop_assets.json').write_text(json.dumps(manifest,indent=2))
functions=[fn('src/bmunit.c','UnitHasItem')]+[fn('src/bmshop.c',name) for name in ['GetItemPurchasePrice','GetItemSellPrice','IsItemSellable','UpdateShopItemCounts','HandleShopBuyAction','Shop_HandleSellConfirmPrompt']]+[fn('src/bmmenu.c',name) for name in ['ArmoryCommandUsability','VendorCommandUsability']]
for i,f in enumerate(functions):
 for line in ['    DrawShopSoldItems(proc);','        ShopInitTexts_OnSell(proc);','    DisplayGoldBoxText(TILEMAP_LOCATED(gBG0TilemapBuffer, 27, 6));','        DisplayGoldBoxText(TILEMAP_LOCATED(gBG0TilemapBuffer, 27, 6));']:f=f.replace(line,'')
 # Native confirmation/list state replaces shop dialogue/Proc labels, not the transaction.
 f=f.replace('Proc_Goto(proc,','NativeShopGoto(proc,');functions[i]=f
(root/'web/generated/shop_routines.h').write_text('\n'.join(f[:f.index('{')].strip()+';' for f in functions)+'\n'+'\n'.join(functions))
events='struct EvCheck0A {u32 unk0,script;u8 x,y;u16 tileCommand;};\n'+fn('src/eventinfo.c','EvCheck0A_SHOP');(root/'web/generated/shop_events.h').write_text(events)
texts=(root/'texts/texts.txt').read_text();parts=[]
for name,ident in [('shopArmoryText',0x685),('shopVendorText',0x686),('shopBuyText',0x845),('shopSellText',0x846)]:
 m=re.search(r'^## MSG_'+format(ident,'03X')+r'\n(.*?)(?=^## |\Z)',texts,re.M|re.S);assert m
 parts.append('static const char '+name+'[]='+json.dumps(re.sub(r'\[[^]]*\]','',m[1]).strip())+';')
(root/'web/generated/shop_text.h').write_text('\n'.join(parts));print('Shop: original chapter5 inventories, price/Silver Card/sellability/transactions/usability/event check; native presentation')
