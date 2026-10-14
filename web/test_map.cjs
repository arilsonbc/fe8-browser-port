const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const dist=process.env.FE8_TEST_DIST||path.join(__dirname,'dist');
const create=require(path.join(dist,'fe8.js')),rom=fs.readFileSync(path.join(__dirname,'../baserom.gba'));
const symbols=fs.readFileSync(path.join(__dirname,'../fireemblem8.map'),'utf8');
const address=name=>parseInt(symbols.match(new RegExp('(0x[0-9a-f]+)\\s+'+name+'\\b'))[1],16)-0x8000000;
const chapterTable=address('gChapterDataTable'),assetTable=address('gChapterDataAssetTable');
function decompress(offset){
 assert.equal(rom[offset],16);const length=rom.readUIntLE(offset+1,3),out=Buffer.alloc(length);let source=offset+4,target=0;
 while(target<length){const flags=rom[source++];for(let b=7;b>=0&&target<length;b--){if(flags&(1<<b)){const code=rom.readUInt16BE(source);source+=2;const count=(code>>12)+3,distance=(code&4095)+1;assert.ok(distance<=target);for(let i=0;i<count&&target<length;i++){out[target]=out[target-distance];target++;}}else out[target++]=rom[source++];}}
 return out;
}
const asset=(id,palette=false)=>{const p=rom.readUInt32LE(assetTable+id*4);return !p?null:palette?rom.subarray(p-0x8000000,p-0x8000000+320):decompress(p-0x8000000);};
(async()=>{
 const m=await create({wasmBinary:fs.readFileSync(path.join(dist,'fe8.wasm'))});
 let checked=0,cells=0,changes=0;
 for(let chapter=0;chapter<79;chapter++){
  const meta=rom.subarray(chapterTable+chapter*148,chapterTable+(chapter+1)*148),map=asset(meta[8]),config=asset(meta[7]),palette=asset(meta[6],true),vram=Buffer.alloc(65536);
  asset(meta[4]).copy(vram,0x8000);const gfx2=asset(meta[5]);if(gfx2)gfx2.copy(vram,0xc000);
  assert.equal(m._fe8_map_start(chapter),0);assert.equal(m._fe8_map_info(1),map[0]);assert.equal(m._fe8_map_info(2),map[1]);
  for(let y=0;y<map[1];y++)for(let x=0;x<map[0];x++){
   const tile=map.readUInt16LE(2+(y*map[0]+x)*2);assert.equal(m._fe8_map_terrain(x,y),config[8192+(tile>>2)]);cells++;
   for(let part=0;part<4;part++)assert.equal(m._fe8_map_tile(x,y,part),(0x6000+config.readUInt16LE((tile+part)*2))&65535);
  }
  const ptr=m._fe8_map_pixels(),pixels=m.HEAPU8.slice(ptr,ptr+153600);
  for(let y=0;y<160;y++)for(let x=0;x<240;x++){
   let color=0;
   if(x<map[0]*16&&y<map[1]*16){
    const metatile=map.readUInt16LE(2+((y>>4)*map[0]+(x>>4))*2),part=((y&8)>>2)+((x&8)>>3),tile=(0x6000+config.readUInt16LE((metatile+part)*2))&65535;
    let px=x&7,py=y&7;if(tile&1024)px=7-px;if(tile&2048)py=7-py;
    const index=(vram[0x8000+(tile&1023)*32+py*4+(px>>1)]>>((px&1)*4))&15;
    const bank=(tile>>12)-6;assert.ok(bank>=0&&bank<10);color=palette.readUInt16LE((bank*16+index)*2);
   }
   const i=(y*240+x)*4;
   for(let c=0;c<3;c++){const value=(color>>(c*5))&31;assert.equal(pixels[i+c],(value<<3)|(value>>2),`chapter ${chapter}, pixel ${x},${y}`);}assert.equal(pixels[i+3],255);checked++;
  }
  if(chapter===0)assert.ok(new Set(Array.from(pixels).filter((_,i)=>i%4!==3)).size>8,'Prologue must contain visible tiles, not a uniform backdrop');
  const changesPtr=rom.readUInt32LE(assetTable+meta[11]*4);
  if(changesPtr)for(let record=changesPtr-0x8000000;rom.readInt8(record)>=0;record+=12){
   const id=rom[record],ox=rom[record+1],oy=rom[record+2],w=rom[record+3],h=rom[record+4],ptr=rom.readUInt32LE(record+8)-0x8000000;
   m._fe8_map_start(chapter);assert.equal(m._fe8_map_change(id,0),0);changes++;
   for(let y=0;y<map[1];y++)for(let x=0;x<map[0];x++){
    let tile=map.readUInt16LE(2+(y*map[0]+x)*2);
    if(x>=ox&&y>=oy&&x<ox+w&&y<oy+h){const replacement=rom.readUInt16LE(ptr+((y-oy)*w+x-ox)*2);if(replacement)tile=replacement;}
    assert.equal(m._fe8_map_terrain(x,y),config[8192+(tile>>2)]);
    for(let part=0;part<4;part++)assert.equal(m._fe8_map_tile(x,y,part),(0x6000+config.readUInt16LE((tile+part)*2))&65535);
   }
   assert.equal(m._fe8_map_change(id,1),0);
   for(let y=oy;y<oy+h;y++)for(let x=ox;x<ox+w;x++){const tile=map.readUInt16LE(2+(y*map[0]+x)*2);assert.equal(m._fe8_map_terrain(x,y),config[8192+(tile>>2)]);for(let part=0;part<4;part++)assert.equal(m._fe8_map_tile(x,y,part),(0x6000+config.readUInt16LE((tile+part)*2))&65535);}
  }
  m._fe8_map_start(chapter);assert.equal(m._fe8_map_change(999,0),-1);assert.equal(m._fe8_map_change_at(-1,0),-1);
  const move=(keys,n=400)=>{for(let i=0;i<n;i++){m._fe8_map_tick(keys);assert.ok(m._fe8_map_info(3)>=0&&m._fe8_map_info(3)<map[0]);assert.ok(m._fe8_map_info(4)>=0&&m._fe8_map_info(4)<map[1]);assert.ok(m._fe8_map_info(5)>=0&&m._fe8_map_info(5)<=Math.max(0,map[0]*16-240));assert.ok(m._fe8_map_info(6)>=0&&m._fe8_map_info(6)<=Math.max(0,map[1]*16-160));if(i%80===0)m._fe8_map_pixels();}};
  move(16);move(128);assert.equal(m._fe8_map_info(3),map[0]-1);assert.equal(m._fe8_map_info(4),map[1]-1);
  assert.equal(m._fe8_map_info(5),Math.max(0,map[0]*16-240));assert.equal(m._fe8_map_info(6),Math.max(0,map[1]*16-160));
  move(240,16);assert.equal(m._fe8_map_info(3),map[0]-1);assert.equal(m._fe8_map_info(4),map[1]-1,'Contradictory directions do not move');
  move(32);move(64);assert.equal(m._fe8_map_info(3),0);assert.equal(m._fe8_map_info(4),0);assert.equal(m._fe8_map_info(5),0);assert.equal(m._fe8_map_info(6),0);
  assert.equal(m._fe8_map_terrain(-1,0),-1);assert.equal(m._fe8_map_tile(map[0],0,0),-1);assert.equal(m._fe8_map_info(10),0);
 }
 assert.equal(m._fe8_map_start(79),-1);assert.equal(m._fe8_map_active(),0);assert.equal(m._fe8_map_start(-1),-1);
 console.log(`OK: 79 mapas, ${cells} células/terrenos, ${checked} pixels e ${changes} alterações/reversões contra os recursos originais e limites de cursor/câmera.`);
})().catch(e=>{console.error(e);process.exitCode=1;});
