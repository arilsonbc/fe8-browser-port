const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const dist=process.env.FE8_TEST_DIST||path.join(__dirname,'dist'),create=require(path.join(dist,'fe8.js'));
const rom=fs.readFileSync(path.join(__dirname,'../baserom.gba')),map=fs.readFileSync(path.join(__dirname,'../fireemblem8.map'),'utf8');
const metadata=JSON.parse(fs.readFileSync(path.join(__dirname,'generated/dialogue_assets.json')));
const address=name=>parseInt(map.match(new RegExp('(0x[0-9a-f]+)\\s+'+name+'\\b'))[1],16)-0x8000000;
const table=address('portrait_data'),positions=[3,6,9,21,24,27,-8,38];
function unpack(pointer){let p=pointer-0x8000000;const size=rom.readUIntLE(p+1,3);if(rom[p]===0)return rom.subarray(p+4,p+size);assert.equal(rom[p],16);p+=4;const out=Buffer.alloc(size);let written=0;while(written<size){const flags=rom[p++];for(let bit=7;bit>=0&&written<size;bit--){if(flags&(1<<bit)){const value=rom.readUInt16BE(p);p+=2;const n=(value>>12)+3,d=(value&4095)+1;assert.ok(d<=written);for(let i=0;i<n&&written<size;i++){out[written]=out[written-d];written++;}}else out[written++]=rom[p++];}}return out;}
(async()=>{
 const m=await create({wasmBinary:fs.readFileSync(path.join(dist,'fe8.wasm'))});m._fe8_game_reset();m._fe8_map_start(0);const pointer=m._fe8_map_pixels(),blank=new Uint8Array(153600);for(let i=3;i<blank.length;i+=4)blank[i]=255;
 const pixel=(buffer,x,y,index,pal)=>{if(!index||x<0||x>=240||y<0||y>=160)return;const color=rom.readUInt16LE(pal+index*2),p=(y*240+x)*4;for(let c=0;c<3;c++){const v=color>>(c*5)&31;buffer[p+c]=(v<<3)|(v>>2);}};
 const defs=JSON.parse(fs.readFileSync(path.join(__dirname,'generated/unit_assets.json'))),ally=defs.find(d=>d.name==='UnitDef_Event_PrologueAlly');m._fe8_units_load(ally.address,0);m._fe8_tactical_start();m._fe8_tactical_select(1);m._fe8_tactical_move(m._fe8_unit_info(1,3),m._fe8_unit_info(1,4));m._fe8_tactical_wait();const activePortrait=rom.readUInt16LE(address('gCharacterData')+(m._fe8_unit_info(1,0)-1)*52+6);
 let checked=0;
 for(const page of metadata.pages){
  m.HEAPU8.set(blank,pointer);assert.equal(m._fe8_dialogue_test_render(pointer,page.id,page.page),0);const actual=m.HEAPU8.slice(pointer,pointer+153600),expected=Uint8Array.from(blank);
  for(let slot=0;slot<8;slot++){
   const fid=page.faces[slot]===255?activePortrait:page.faces[slot];if(!fid)continue;const row=table+(fid-1)*28,image=unpack(rom.readUInt32LE(row)),pal=rom.readUInt32LE(row+8)-0x8000000,mouth=rom.readUInt32LE(row+12)-0x8000000,flip=positions[slot]<=14;
   // Read the original OAM tables directly; the reference does not use the native piece list.
   const oam=address(flip?'gSprite_Face96x96_Flipped':'gSprite_Face96x96');
   const sizes=[[[8,8],[16,16],[32,32],[64,64]],[[16,8],[32,8],[32,16],[64,32]],[[8,16],[8,32],[16,32],[32,64]]];
   for(let piece=0;piece<rom.readUInt16LE(oam);piece++){
    const a=rom.readUInt16LE(oam+2+piece*6),b=rom.readUInt16LE(oam+4+piece*6),tile=rom.readUInt16LE(oam+6+piece*6)&1023,[w,h]=sizes[a>>14][b>>14];
    let sx=b&511,sy=a&255;if(sx>=256)sx-=512;if(sy>=128)sy-=256;sx+=positions[slot]*8;sy+=80;
    for(let y=0;y<h;y++)for(let x=0;x<w;x++){const px=b&4096?w-1-x:x,py=b&8192?h-1-y:y,offset=(tile+Math.floor(py/8)*32+Math.floor(px/8))*32+(py&7)*4+Math.floor((px&7)/2);assert.ok(offset<image.length);pixel(expected,sx+x,sy+y,image[offset]>>((px&1)*4)&15,pal);}
   }
   const offset=(page.smiles[slot]?0:24)+16,mx=positions[slot]*8+(flip?4-rom[row+20]:rom[row+20]-4)*8-16,my=80+rom[row+21]*8;
   for(let y=0;y<16;y++)for(let x=0;x<32;x++){const px=flip?31-x:x,p=mouth+(offset+Math.floor(y/8)*4+Math.floor(px/8))*32+(y&7)*4+Math.floor((px&7)/2);pixel(expected,mx+x,my+y,rom[p]>>((px&1)*4)&15,pal);}
  }
  assert.deepEqual(actual.subarray(96*240*4),expected.subarray(96*240*4),'ROM portrait pixels '+page.id+'/'+page.page);checked+=240*64;
  assert.equal(m._fe8_dialogue_info(3),0,'No resource overflow');
 }
 assert.equal(m._fe8_dialogue_test_render(pointer,99999,0),-1);
 // Compare the original talk font and its color LUT directly with a complete caption.
 m.HEAPU8.set(blank,pointer);m._fe8_dialogue_test_render(pointer,0x664,0);
 const font=address('TextGlyphs_Talk'),textPal=address('Pal_Text')+32,lut=address('gUnknown_0858849C'),caption='Renais Castle';
 const glyphs=Array.from(caption,c=>{const p=rom.readUInt32LE(font+c.charCodeAt(0)*4)-0x8000000;return {width:rom[p+5],p};});
 const width=Math.ceil(glyphs.reduce((n,g)=>n+g.width,0)/8)+2,boxX=Math.min(29-width,Math.max(1,18-Math.floor(width/2)));let gx=(boxX+1)*8;
 for(const glyph of glyphs){for(let y=0;y<16;y++)for(let x=0;x<16;x++){const index=rom.readUInt32LE(glyph.p+8+y*4)>>(x*2)&3;if(!index)continue;const ink=rom.readUInt16LE(lut+index*2)&15,color=rom.readUInt16LE(textPal+ink*2),p=pointer+((56+y)*240+gx+x)*4;for(let c=0;c<3;c++){const v=color>>(c*5)&31;assert.equal(m.HEAPU8[p+c],(v<<3)|(v>>2),'Talk font pixel');}}gx+=glyph.width;}
 // Actual event input: first press completes printing, release then second press advances.
 m._fe8_save_reset_empty();m._fe8_save_create(0,1);m._fe8_save_load(0);const sram=m.HEAPU8.slice(m._fe8_save_buffer(),m._fe8_save_buffer()+32768);m._fe8_chapter_start();
 for(let i=0;i<5000&&m._fe8_event_info(2)!==0x903;i++){m._fe8_event_test_tick(0);if(m._fe8_event_info(2)===0x664)m._fe8_event_advance();}
 assert.equal(m._fe8_event_info(2),0x903);assert.equal(m._fe8_dialogue_info(0),3);assert.equal(m._fe8_dialogue_info(12),0x51);assert.equal(m._fe8_dialogue_info(13),0x6a);
 assert.ok(m._fe8_dialogue_info(1)<m._fe8_dialogue_info(2));m._fe8_event_test_tick(1);assert.equal(m._fe8_event_info(3),0);assert.equal(m._fe8_dialogue_info(1),m._fe8_dialogue_info(2));m._fe8_event_test_tick(0);m._fe8_event_test_tick(1);assert.equal(m._fe8_event_info(3),1);
 m._fe8_event_reset();assert.equal(m._fe8_dialogue_info(0),-1);assert.equal(m._fe8_dialogue_info(1),0);assert.deepEqual(m.HEAPU8.slice(m._fe8_save_buffer(),m._fe8_save_buffer()+32768),sram);
 console.log(`OK: ${metadata.pages.length} páginas, ${metadata.portraits.length} retratos, ${checked} pixels contra OAM/paletas/recursos da ROM; avanço, reset e SRAM preservada.`);
})().catch(e=>{console.error(e);process.exitCode=1;});

