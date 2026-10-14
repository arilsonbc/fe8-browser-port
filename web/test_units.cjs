const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const dist=process.env.FE8_TEST_DIST||path.join(__dirname,'dist'),create=require(path.join(dist,'fe8.js')),rom=fs.readFileSync(path.join(__dirname,'../baserom.gba'));
const metadata=JSON.parse(fs.readFileSync(path.join(__dirname,'generated/unit_assets.json'))),symbolText=fs.readFileSync(path.join(__dirname,'../fireemblem8.map'),'utf8');
const address=n=>parseInt(symbolText.match(new RegExp('(0x[0-9a-f]+)\\s+'+n+'\\b'))[1],16)-0x8000000;
const chrTable=address('gCharacterData'),classTable=address('gClassData'),itemTable=address('gItemData'),spriteTable=address('unit_icon_wait_table'),palTable=address('gPal_MapSprite');
function lz(p){assert.equal(rom[p],16);const length=rom.readUIntLE(p+1,3),out=Buffer.alloc(length);let read=p+4,write=0;while(write<length){const flags=rom[read++];for(let bit=7;bit>=0&&write<length;bit--){if(flags&(1<<bit)){const value=rom.readUInt16BE(read);read+=2;const count=(value>>12)+3,distance=(value&4095)+1;for(let i=0;i<count&&write<length;i++){out[write]=out[write-distance];write++;}}else out[write++]=rom[read++];}}return out;}
const factory=()=>create({wasmBinary:fs.readFileSync(path.join(dist,'fe8.wasm'))});
(async()=>{
 const m=await factory(),rng=await factory();let checked=0;
 const scan=()=>Array.from({length:255},(_,i)=>i+1).filter(i=>m._fe8_unit_info(i,0)>0);
 for(const group of metadata){
  m._fe8_units_reset();m._InitRN(123);rng._InitRN(123);m._fe8_map_start(0);assert.equal(m._fe8_units_load(group.address,0),group.count);assert.equal(m._fe8_units_count(),group.count);
  const ids=scan();
  for(let i=0;i<group.count;i++){
   const def=rom.subarray(group.address-0x8000000+i*20,group.address-0x8000000+(i+1)*20),pid=def[0],jid=def[1],ch=rom.subarray(chrTable+(pid-1)*52,chrTable+pid*52),cl=rom.subarray(classTable+(jid-1)*84,classTable+jid*84),faction=(def[3]>>1)&3;
   const native=ids.find(index=>m._fe8_unit_info(index,0)===pid&&m._fe8_unit_info(index,1)===jid&&!ids.slice(0,i).includes(index))||ids[i];
   // Generic PIDs can repeat: preserve each definition by faction slot order.
   const matching=ids.filter(index=>m._fe8_unit_info(index,0)===pid&&m._fe8_unit_info(index,1)===jid);const prior=Array.from({length:i},(_,j)=>rom[group.address-0x8000000+j*20]===pid&&rom[group.address-0x8000000+j*20+1]===jid).filter(Boolean).length;
   const index=matching[prior];assert.ok(index);
   assert.equal(m._fe8_unit_info(index,2),faction===2?128:faction===1?64:0);assert.equal(m._fe8_unit_info(index,13),def[3]>>3);
   let stats=[ch.readInt8(12)+cl.readInt8(11),ch.readInt8(13)+cl.readInt8(12),ch.readInt8(14)+cl.readInt8(13),ch.readInt8(15)+cl.readInt8(14),ch.readInt8(16)+cl.readInt8(15),ch.readInt8(17)+cl.readInt8(16),ch.readInt8(18)];
   if(def[3]&1){const promoted=!!((ch.readUInt32LE(40)|cl.readUInt32LE(40))&256),levels=promoted?9:0;
    for(const levelCount of [levels,(def[3]>>3)-1].filter(Boolean))for(let stat=0;stat<7;stat++){
     const total=cl[27+stat]*levelCount,jitter=Math.floor(rng._NextRN()*Math.floor(total/4)/65536)-Math.floor(total/8);let growth=total+jitter,gain=0;while(growth>100){gain++;growth-=100;}if(growth>Math.floor(rng._NextRN()*100/65536))gain++;stats[stat]+=gain;
    }
   }
   const caps=[faction===2?120:60,cl[20],cl[21],cl[22],cl[23],cl[24],30];stats=stats.map((v,i)=>Math.min(v,caps[i]));
   for(let stat=0;stat<7;stat++){assert.equal(m._fe8_unit_info(index,[5,7,8,9,10,11,12][stat]),stats[stat],group.name+' stat '+stat);checked++;}
   let x=def.readUInt16LE(4)&63,y=(def.readUInt16LE(4)>>6)&63;
   if(def[7]){const ptr=def.readUInt32LE(8)-0x8000000+(def[7]-1)*8,point=rom.readUInt32LE(ptr);x=point&63;y=(point>>6)&63;}
   assert.equal(m._fe8_unit_info(index,3),x);assert.equal(m._fe8_unit_info(index,4),y);
   for(let item=0;item<4;item++){const id=def[12+item];const value=id?(rom.readUInt32LE(itemTable+id*36+8)&8?0:rom[itemTable+id*36+20])*256+id:0;assert.equal(m._fe8_unit_info(index,30+item),value);}
  }
  assert.equal(m._fe8_units_errors(),0);
 }
 // Native REDA interpolation must finish at the source's destination and remain bounded.
 const ally=metadata.find(g=>g.name==='UnitDef_Event_PrologueAlly');m._fe8_units_reset();assert.equal(m._fe8_units_load(ally.address,1),2);assert.equal(m._fe8_units_busy(),1);
 assert.equal(m._fe8_unit_info(1,18),13*16);for(let i=0;i<1000&&m._fe8_units_busy();i++)m._fe8_units_tick();assert.equal(m._fe8_units_busy(),0);assert.equal(m._fe8_unit_info(1,18),8*16);assert.equal(m._fe8_unit_info(1,19),5*16);
 assert.equal(m._fe8_unit_move(2,4,4),0);for(let i=0;i<100;i++)m._fe8_units_tick();assert.equal(m._fe8_unit_info(1,3),4);assert.equal(m._fe8_unit_info(1,18),64);assert.equal(m._fe8_unit_move(999,4,4),-1);
 // Compare standing sprites against original compressed sheets at all four animation transitions.
 m._fe8_units_reset();m._fe8_map_start(0);let ptr=m._fe8_map_pixels();const backdrop=m.HEAPU8.slice(ptr,ptr+153600);m._fe8_units_load(ally.address,0);
 for(const clock of [0,31,32,35,36,67,68,71,72]){
  m.HEAPU8.set(backdrop,ptr);m._fe8_units_render(ptr,0,0,clock);const actual=m.HEAPU8.slice(ptr,ptr+153600),expected=Uint8Array.from(backdrop);const phase=clock%72,frame=phase>=68?1:phase>=36?2:phase>=32?1:0;
  // Same-foot ordering: older/lower unit slot is in front, as in secondary OAM.
  for(const index of [2,1]){
   const jid=m._fe8_unit_info(index,1),sms=rom[classTable+(jid-1)*84+6],record=spriteTable+sms*8,size=rom.readUInt16LE(record+2),width=size===2?32:16,height=size===0?16:32,data=lz(rom.readUInt32LE(record+4)-0x8000000);
   const sx=m._fe8_unit_info(index,3)*16-(size===2?8:0),sy=m._fe8_unit_info(index,4)*16-(height-16);
   for(let y=0;y<height;y++)for(let x=0;x<width;x++){
    const tile=Math.floor(y/8)*(width/8)+Math.floor(x/8),offset=frame*width*height/2+tile*32+(y&7)*4+Math.floor((x&7)/2),pixel=(data[offset]>>((x&1)*4))&15;if(!pixel)continue;
    const dx=sx+x,dy=sy+y;if(dx<0||dy<0||dx>=240||dy>=160)continue;const color=rom.readUInt16LE(palTable+pixel*2),p=(dy*240+dx)*4;
    for(let c=0;c<3;c++){const value=(color>>(c*5))&31;expected[p+c]=(value<<3)|(value>>2);}
   }
  }
  assert.deepEqual(actual,expected,'Original sprites at clock '+clock);
 }
 assert.equal(m._fe8_units_load(0xDEADBEEF,0),-1);assert.equal(m._fe8_units_errors(),0);
 console.log(`OK: ${metadata.length} grupos, ${checked} atributos independentes, autolevel/RNG, genéricos duplicados, REDA e 9 fases de sprites.`);
})().catch(e=>{console.error(e);process.exitCode=1;});
