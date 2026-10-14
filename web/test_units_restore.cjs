const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const dist=process.env.FE8_TEST_DIST||path.join(__dirname,'dist'),create=require(path.join(dist,'fe8.js'));
(async()=>{
 const m=await create({wasmBinary:fs.readFileSync(path.join(dist,'fe8.wasm'))});m._fe8_save_reset_empty();m._fe8_save_create(0,1);
 const read=()=>Buffer.from(m.HEAPU8.slice(m._fe8_save_buffer(),m._fe8_save_buffer()+32768)),base=read();
 const checksum=b=>{let sum=0,xor=0;for(let i=0;i<b.length;i+=2){const v=b.readUInt16LE(i);sum=(sum+v)&65535;xor^=v;}return ((xor<<16)|sum)>>>0;};
 for(let flags=0;flags<256;flags++){
  const bytes=Buffer.from(base),record=Buffer.alloc(36);let bits=0n,offset=0n;
  const put=(value,width)=>{bits|=BigInt(value)<<offset;offset+=BigInt(width);};
  const exp=flags%3===0?127:flags%3===1?100:37,x=flags%2?63:4,y=flags%2?63:5;
  [[2,7],[17,5],[exp,7],[x,6],[y,6],[flags,13],[28,6],[14,5],[15,5],[16,5],[17,5],[18,5],[19,5],[2,5],[3,5],[0x2809,14],[0x1e6c,14],[0,14],[0,14],[0,14]].forEach(([v,w])=>put(v,w));assert.equal(offset,160n);
  for(let i=0;i<20;i++)record[i]=Number((bits>>BigInt(i*8))&255n);record[20]=1;for(let i=0;i<8;i++)record[21+i]=i*30;for(let i=0;i<7;i++)record[29+i]=i*20;
  record.copy(bytes,0x4010);bytes.writeUInt32LE(checksum(bytes.subarray(0x3fc4,0x4d8c)),0x70);m.HEAPU8.set(bytes,m._fe8_save_import_buffer());assert.equal(m._fe8_save_import(32768),0);assert.equal(m._fe8_units_restore(0),1);
  for(const [field,value] of [[0,1],[1,2],[13,17],[14,exp>99?255:exp],[3,x===63?-1:x],[4,y===63?-1:y],[5,28],[6,28],[7,14],[8,15],[9,16],[10,17],[11,18],[12,19],[30,0x2809],[31,0x1e6c],[21,2],[22,3],[23,0]])assert.equal(m._fe8_unit_info(1,field),value);
  let state=flags&1?5:0;if(flags&2)state|=9;if(flags&4)state|=1<<14;if(flags&8)state|=1<<15;if(flags&16)state|=1<<13;if(flags&32)state|=1<<16;if(flags&64)state|=1<<25;if(flags&128)state|=1<<21;assert.equal(m._fe8_unit_info(1,15),state);
  for(let i=0;i<8;i++)assert.equal(m._fe8_unit_info(1,40+i),i*30);for(let i=0;i<7;i++)assert.equal(m._fe8_unit_info(1,50+i),i*20);assert.deepEqual(read(),bytes);
 }
 // Render imported classes across the original sprite phase transitions, including unused class records.
 const template=Buffer.from(base);const record=Buffer.alloc(36);let packed=0n,bit=0n;
 [[1,7],[1,5],[37,7],[4,6],[5,6],[0,13],[28,6],[14,5],[15,5],[16,5],[17,5],[18,5],[19,5],[0,5],[0,5],[0,14],[0,14],[0,14],[0,14],[0,14]].forEach(([v,w])=>{packed|=BigInt(v)<<bit;bit+=BigInt(w);});for(let i=0;i<20;i++)record[i]=Number(packed>>BigInt(i*8)&255n);record[20]=1;
 for(let jid=1;jid<=127;jid++){
  record[0]=(record[0]&128)|jid;record.copy(template,0x4010);template.writeUInt32LE(checksum(template.subarray(0x3fc4,0x4d8c)),0x70);m.HEAPU8.set(template,m._fe8_save_import_buffer());assert.equal(m._fe8_save_import(32768),0);m._fe8_map_start(0);assert.equal(m._fe8_units_restore(0),1);const p=m._fe8_map_pixels();for(const frame of [0,32,36,68])m._fe8_units_render(p,0,0,frame);assert.equal(m._fe8_units_errors(),0,'Sprite bounds for class '+jid);
 }
 assert.equal(m._fe8_units_restore(3),-1);assert.equal(m._fe8_units_errors(),0);console.log('OK: 256 registros packed restaurados, estados, coordenadas inválidas, EXP, atributos, itens/ranks e SRAM intacta.');
})().catch(e=>{console.error(e);process.exitCode=1;});
