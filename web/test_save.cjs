const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const dist=process.env.FE8_TEST_DIST||path.join(__dirname,'dist'),create=require(path.join(dist,'fe8.js'));
const Storage=require('./storage.js');const newModule=()=>create({wasmBinary:fs.readFileSync(path.join(dist,'fe8.wasm'))});
const checksum16=(buf)=>{let sum=0,xor=0;for(let i=0;i<buf.length;i+=2){const value=buf[i]|buf[i+1]<<8;sum+=value;xor^=value;}return (sum+xor)&65535;};
const checksum32=(buf)=>{let sum=0,xor=0;for(let i=0;i<buf.length;i+=2){const value=buf[i]|buf[i+1]<<8;sum=(sum+value)&65535;xor^=value;}return ((xor<<16)>>>0)|sum;};
(async()=>{
 const m=await newModule();const read=()=>Buffer.from(m.HEAPU8.slice(m._fe8_save_buffer(),m._fe8_save_buffer()+32768));
 const importSave=bytes=>{m.HEAPU8.set(bytes,m._fe8_save_import_buffer());return m._fe8_save_import(bytes.length);};
 const check=bytes=>{
  assert.equal(bytes.length,32768);assert.equal(bytes.subarray(0,8).toString(),'AGB-FE9\0');
  assert.equal(bytes.readUInt32LE(8),0x40624);assert.equal(bytes.readUInt16LE(12),0x200a);
  assert.equal(bytes.readUInt16LE(96),checksum16(bytes.subarray(0,80)));
  for(let slot=0;slot<3;slot++){const p=100+slot*16;if(bytes.readUInt16LE(p+4)!==0x200a)continue;
   assert.equal(bytes.readUInt32LE(p),0x40624);assert.equal(bytes[p+6],0);assert.equal(bytes.readUInt16LE(p+10),0xdc8);
   const offset=bytes.readUInt16LE(p+8);assert.equal(offset,0x3fc4+slot*0xdc8);
   assert.equal(bytes.readUInt32LE(p+12),checksum32(bytes.subarray(offset,offset+0xdc8))>>>0);
  }
 };
 m._fe8_save_reset_empty();check(read());assert.equal(m._fe8_save_global_valid(),1);
 for(let slot=0;slot<3;slot++){
  assert.equal(m._fe8_save_create(slot,slot),0);assert.equal(m._fe8_save_valid(slot),1);assert.equal(m._fe8_save_difficulty(slot),slot);
  const bytes=read(),offset=0x3fc4+slot*0xdc8;check(bytes);
  assert.equal(bytes[offset+12],slot);assert.equal(bytes[offset+14],0);assert.equal(bytes[offset+24],slot+1);
  assert.equal(bytes[offset+27],1);assert.equal(bytes[offset+43],1);
  assert.equal(bytes[offset+0x14]&64,slot===2?64:0);assert.equal((bytes[offset+0x42]>>5)&1,slot?1:0);
  assert.ok(bytes.subarray(offset+0x4c,offset+0x4c+52*36).every(c=>c===0),'Original initial unit blocks are empty');
  assert.equal(m._fe8_save_last_slot(),slot);
 }
 const all=read(),revision=m._fe8_save_revision();assert.equal(m._fe8_save_create(0,0),-2);assert.equal(m._fe8_save_revision(),revision);
 assert.equal(m._fe8_save_erase(1),0);assert.equal(m._fe8_save_copy(0,1),0);check(read());
 assert.deepEqual(read().subarray(0x4d8c,0x5b54),all.subarray(0x3fc4,0x4d8c),'Copy preserves every opaque byte');
 assert.equal(m._fe8_save_copy(0,2),-3);assert.equal(m._fe8_save_copy(1,1),-1);
 assert.equal(m._fe8_save_load(2),0);assert.equal(m._fe8_save_last_slot(),2);
 assert.equal(importSave(all),0);assert.deepEqual(read(),all,'Import/export exact SRAM roundtrip');
 for(const offset of [0,8,12,96,100,104,106,108,110,112,0x3fc4,0x4100]){
  const corrupt=Buffer.from(all);corrupt[offset]^=1;assert.ok(importSave(corrupt)<0,'Reject corruption at '+offset);assert.deepEqual(read(),all,'Rejected import leaves live bytes unchanged');
 }
 const hostile=Buffer.from(all);hostile.writeUInt16LE(0xffff,108);assert.ok(importSave(hostile)<0);assert.deepEqual(read(),all);
 assert.equal(m._fe8_save_import(32767),-1);assert.equal(m._fe8_save_import(65536),-1);
 // Hundreds of checksum-valid unknown fields must remain opaque and preserved.
 let random=0x12345678;
 for(let test=0;test<250;test++){
  const foreign=Buffer.from(all);for(let i=0;i<20;i++){random=(Math.imul(random,1664525)+1013904223)>>>0;foreign[0x4010+random%2000]=(random>>>24)&255;}
  foreign.writeUInt32LE(checksum32(foreign.subarray(0x3fc4,0x4d8c))>>>0,112);
  assert.equal(importSave(foreign),0);assert.deepEqual(read(),foreign);
 }
 // A checksum-valid relocated game block must not alias later fixed-slot writes.
 const relocated=Buffer.from(all);all.subarray(0x3fc4,0x4d8c).copy(relocated,0xd4);relocated.writeUInt16LE(0xd4,108);
 assert.ok(importSave(relocated)<0);const untouched=read();assert.ok(importSave(relocated)<0);assert.deepEqual(read(),untouched);
 assert.equal(m._fe8_save_errors(),0);
 class MemoryStorage{constructor(){this.values=new Map();this.fail=false;}getItem(k){return this.values.get(k)||null;}setItem(k,v){if(this.fail)throw Error('quota');this.values.set(k,v);}}
 const local=new MemoryStorage(),a=await newModule(),store=new Storage(a,local);assert.equal(store.initialize(),true);
 assert.equal(a._fe8_save_create(0,2),0);assert.equal(store.flush(),true);
 const b=await newModule(),restored=new Storage(b,local);assert.equal(restored.initialize(),true);assert.equal(b._fe8_save_valid(0),1);assert.equal(b._fe8_save_difficulty(0),2);
 assert.equal(b._fe8_save_erase(0),0);assert.equal(restored.flush(),true);assert.equal(restored.restorePrevious().ok,true);assert.equal(b._fe8_save_valid(0),1);
 const backup=local.getItem(Storage.BACKUP);b._fe8_save_load(0);restored.flush();assert.equal(local.getItem(Storage.BACKUP),backup,'Identical writes retain meaningful backup');
 const prior=restored.snapshot();assert.equal(restored.importBytes(new Uint8Array(32768)).ok,false);assert.deepEqual(restored.snapshot(),prior);
 local.fail=true;b._fe8_save_create(1,0);assert.equal(restored.flush(),false);assert.equal(b._fe8_save_valid(1),1,'Persistence failure keeps RAM save exportable');
 local.fail=false;assert.equal(restored.flush(true),true);
 const damaged=new MemoryStorage();damaged.setItem(Storage.KEY,'corrupt');const c=await newModule(),safe=new Storage(c,damaged);assert.equal(safe.initialize(),false);assert.equal(damaged.getItem(Storage.KEY),'corrupt','Failed startup never silently overwrites saved data');
 console.log('OK: SRAM original de 32 KiB, 3 dificuldades, checksums independentes, cópia/exclusão e 250 roundtrips opacos.');
 console.log('OK: importações inválidas sem mutação; persistência, recarga, backup, deduplicação e falha de quota.');
})().catch(error=>{console.error(error);process.exitCode=1;});
