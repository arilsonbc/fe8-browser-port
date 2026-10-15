const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const dist=process.env.FE8_TEST_DIST||path.join(__dirname,'dist'),create=require(path.join(dist,'fe8.js')),rom=fs.readFileSync(path.join(__dirname,'../baserom.gba')),map=fs.readFileSync(path.join(__dirname,'../fireemblem8.map'),'utf8'),manifest=JSON.parse(fs.readFileSync(path.join(__dirname,'generated/ai_vm_assets.json')));
const addr=n=>parseInt(map.match(new RegExp('(0x[0-9a-f]+)\\s+'+n+'\\b'))[1],16)-0x8000000;
const script=a=>{const item=manifest.scripts.find(v=>v.address===a);assert.ok(item);return Array.from({length:item.commands},(_,i)=>{const p=a-0x8000000+i*16;return {cmd:rom[p],a:rom[p+1],b:rom[p+2],target:rom[p+3]};});};
const tables=['gAi1ScriptTable','gAi2ScriptTable'].map(n=>Array.from({length:manifest.tables[n]},(_,i)=>script(rom.readUInt32LE(addr(n)+i*4))));
(async()=>{
 const wasmBinary=fs.readFileSync(path.join(dist,'fe8.wasm')),m=await create({wasmBinary}),rng=await create({wasmBinary});m._fe8_game_reset();m._fe8_save_reset_empty();m._fe8_event_preview();const sram=m.HEAPU8.slice(m._fe8_save_buffer(),m._fe8_save_buffer()+32768),unit=Array.from({length:57},(_,f)=>m._fe8_unit_info(129,f));
 for(const row of manifest.scripts){const raw=rom.subarray(row.address-0x8000000,row.address-0x8000000+row.commands*16);assert.equal(crypto.createHash('sha256').update(raw).digest('hex'),row.sha256);}
 function model(kind,id,pc){let a=kind?6:id,b=kind?id:3,steps=0,error=0,opcode=-1;for(let i=0;i<256;i++){
  const table=tables[kind][kind?b:a];if(!table||pc>=table.length){error=2;break;}let cmd=table[pc];if(cmd.cmd===28){error=3;break;}steps++;opcode=cmd.cmd;
  if(cmd.cmd===2){if(cmd.a!==255)a=cmd.a;if(cmd.b!==255)b=cmd.b;if((kind===0&&cmd.a!==255)||(kind===1&&cmd.b!==255))pc=0;else pc++;break;}
  if(cmd.cmd===3){pc=cmd.target?table.findIndex(c=>c.cmd===28&&c.target===cmd.target)+1:0;}
  else if([6,14,20,21,23].includes(cmd.cmd)){pc++;break;}
  else if(cmd.cmd===27)pc++;
  else{error=100+cmd.cmd;break;}
  if(i===255)error=4;
 }return {result:error?-error:0,steps,pc,a,b,opcode,error};}
 let cases=0;m._InitRN(41);rng._InitRN(41);
 for(let kind=0;kind<2;kind++)for(let id=0;id<tables[kind].length;id++)for(let pc=0;pc<tables[kind][id].length;pc++){
  if(tables[kind][id].some(c=>[0,12,13,18].includes(c.cmd)))continue;const expected=model(kind,id,pc);assert.equal(m._fe8_ai_script_table_probe(129,kind,id,pc),expected.result,`VM ${kind}/${id}/${pc}`);for(const [field,value] of [[0,expected.error],[1,expected.steps],[3,kind],[4,expected.pc],[5,expected.a],[6,expected.b],[7,expected.opcode]])assert.equal(m._fe8_ai_script_info(field),value,`VM field ${kind}/${id}/${pc}/${field}`);assert.equal(m._NextRN(),rng._NextRN(),'Control-flow probe does not consume combat RNG');assert.deepEqual(Array.from({length:57},(_,f)=>m._fe8_unit_info(129,f)),unit);cases++;
 }
 let scanned=0;for(let kind=0;kind<2;kind++)for(let id=0;id<tables[kind].length;id++)for(let pc=0;pc<tables[kind][id].length;pc++){assert.ok(m._fe8_ai_script_table_probe(129,kind,id,pc)<=0);assert.equal(m._NextRN(),rng._NextRN());assert.deepEqual(Array.from({length:57},(_,f)=>m._fe8_unit_info(129,f)),unit);scanned++;}
 assert.equal(m._fe8_ai_script_probe(129,0,0),0);assert.equal(m._fe8_ai_script_info(4),1);assert.equal(m._fe8_ai_script_probe(129,0,1),0);assert.equal(m._fe8_ai_script_info(1),2);assert.equal(m._fe8_ai_script_probe(129,1,0),0);assert.equal(m._fe8_ai_script_probe(130,0,0),-105);assert.equal(m._fe8_ai_script_probe(130,1,0),0);assert.equal(m._fe8_ai_script_info(8),1);assert.equal(m._fe8_ai_script_probe(131,1,0),0);assert.equal(m._fe8_ai_script_info(4),1);assert.equal(m._fe8_ai_script_probe(131,1,1),0);assert.equal(m._fe8_ai_script_info(6),0);assert.equal(m._fe8_unit_info(131,25),18,'Probe does not prematurely commit next-turn AI');
 assert.equal(m._fe8_ai_script_table_probe(129,0,255,0),-2);assert.equal(m._fe8_ai_script_probe(129,0,255),-2);assert.equal(m._fe8_ai_script_probe(0,0,0),-1);assert.equal(m._fe8_ai_script_probe(129,2,0),-1);assert.equal(m._fe8_ai_script_probe(129,0,256),-2);assert.deepEqual(m.HEAPU8.slice(m._fe8_save_buffer(),m._fe8_save_buffer()+32768),sram);m._fe8_game_reset();assert.equal(m._fe8_ai_script_info(7),-1);
 console.log(`OK: ${manifest.scripts.length} scripts/ROM e ${cases} referências de controle/${scanned} entradas de PC, goto/labels/AI changes/no-op, limites e fronteiras de ações; NPCs, RNG e SRAM preservados.`);
})().catch(e=>{console.error(e);process.exitCode=1;});
