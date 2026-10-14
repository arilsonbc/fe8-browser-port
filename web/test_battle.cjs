const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const dist=process.env.FE8_TEST_DIST||path.join(__dirname,'dist'),create=require(path.join(dist,'fe8.js'));
const rom=fs.readFileSync(path.join(__dirname,'../baserom.gba')),map=fs.readFileSync(path.join(__dirname,'../fireemblem8.map'),'utf8'),groups=JSON.parse(fs.readFileSync(path.join(__dirname,'generated/unit_assets.json')));
const address=n=>parseInt(map.match(new RegExp('(0x[0-9a-f]+)\\s+'+n+'\\b'))[1],16)-0x8000000;
const itemTable=address('gItemData'),classTable=address('gClassData');
(async()=>{
 const binary=fs.readFileSync(path.join(dist,'fe8.wasm')),m=await create({wasmBinary:binary}),rng=await create({wasmBinary:binary});m._fe8_game_reset();m._fe8_save_reset_empty();const sram=m.HEAPU8.slice(m._fe8_save_buffer(),m._fe8_save_buffer()+32768);
 const prepare=()=>{m._fe8_battle_reset();m._fe8_units_reset();m._fe8_map_start(64);for(const name of ['UnitDef_Event_PrologueEscapees','UnitDef_Event_PrologueValterGroup'])assert.ok(m._fe8_units_load(groups.find(g=>g.name===name).address,0)>0);const ids=Array.from({length:255},(_,i)=>i+1).filter(i=>m._fe8_unit_info(i,0)>0),actor=ids.find(i=>m._fe8_unit_info(i,0)===2),target=ids.find(i=>m._fe8_unit_info(i,2)===128);m._fe8_unit_move(2,4,4);m._fe8_unit_move(m._fe8_unit_info(target,0),5,4);for(let i=0;i<60;i++)m._fe8_units_tick();return {actor,target};};
 const queue=words=>{const p=m._fe8_save_import_buffer();for(let i=0;i<words.length;i++)m.HEAPU32[p/4+i]=words[i]>>>0;return p;};
 const stats=index=>{const item=m._fe8_unit_info(index,30)&255,row=itemTable+item*36,jid=m._fe8_unit_info(index,1),t=m._fe8_map_terrain(m._fe8_unit_info(index,3),m._fe8_unit_info(index,4)),defPtr=rom.readUInt32LE(classTable+(jid-1)*84+72)-0x8000000;return {hp:m._fe8_unit_info(index,6),pow:m._fe8_unit_info(index,7),def:m._fe8_unit_info(index,10)+rom.readInt8(defPtr+t),mt:rom[row+21],type:rom[row+7]};};
 let fixtures=0;
 for(const attributes of [0,1,2,3])for(const damage of [0,1,17,127])for(const side of [0,1]){
  const {actor,target}=prepare(),a=stats(actor),b=stats(target),triangle=a.type===0&&b.type===1?-1:a.type===1&&b.type===0?1:0;
  const expectedAttack=[a.pow+a.mt+triangle,b.pow+b.mt-triangle],expectedDefense=[a.def,b.def],defender=side?0:1,expectedDamage=attributes&2?0:Math.min([a.hp,b.hp][defender],damage||Math.max(0,(expectedAttack[side]-expectedDefense[defender])*(attributes&1?3:1)));
  const words=[(attributes<<16)|(damage<<8)|side,0xffffffff],p=queue(words);m._InitRN(42);rng._InitRN(42);
  assert.equal(m._fe8_battle_start(2,m._fe8_unit_info(target,0),0,0,p,words.length),1);assert.equal(m._fe8_battle_info(20),expectedDamage);assert.equal(m._fe8_battle_info(10),expectedAttack[0]);assert.equal(m._fe8_battle_info(11),expectedAttack[1]);assert.equal(m._fe8_battle_info(12),expectedDefense[0]);assert.equal(m._fe8_battle_info(13),expectedDefense[1]);
  assert.equal(m._fe8_unit_info(actor,6),a.hp,'HP commits after presentation');assert.equal(m._fe8_unit_info(target,6),b.hp);
  for(let t=0;t<60;t++)m._fe8_event_test_tick(0);assert.equal(m._fe8_battle_busy(),0);assert.equal(m._fe8_unit_info(actor,6),a.hp-(side?expectedDamage:0));assert.equal(m._fe8_unit_info(target,6),b.hp-(side?0:expectedDamage));assert.equal(m._NextRN(),rng._NextRN(),'Scripted hit must not roll normal hit/crit RNG');assert.equal(m._fe8_battle_info(9),1);
  fixtures++;
 }
 let {target}=prepare(),pid=m._fe8_unit_info(target,0);const original=[0x20000,1,0xffffffff],p=queue(original);assert.equal(m._fe8_battle_start(2,pid,0,0,p,3),2);assert.equal(m._fe8_battle_info(7),13);assert.equal(m._fe8_battle_info(8),34);assert.equal(m._fe8_battle_info(20),0);assert.equal(m._fe8_battle_info(21),17);
 for(let i=0;i<120;i++)m._fe8_event_test_tick(0);assert.equal(m._fe8_unit_info(1,6),13);
 ({target}=prepare());pid=m._fe8_unit_info(target,0);assert.equal(m._fe8_battle_set_script(queue(original),3),2);assert.equal(m._fe8_battle_start(2,pid,0,0,0,0),2);m._fe8_battle_reset();assert.equal(m._fe8_battle_busy(),0);
 for(const invalid of [[0],[2,0xffffffff],Array(8).fill(0).concat(0xffffffff)]){assert.ok(m._fe8_battle_set_script(queue(invalid),invalid.length)<0);assert.equal(m._fe8_battle_busy(),0);}
 assert.ok(m._fe8_battle_start(999,pid,0,0,queue(original),3)<0);assert.ok(m._fe8_battle_start(2,pid,1,0,queue(original),3)<0);assert.ok(m._fe8_battle_start(2,pid,0,1,queue(original),3)<0);assert.deepEqual(m.HEAPU8.slice(m._fe8_save_buffer(),m._fe8_save_buffer()+32768),sram);
 console.log(`OK: ${fixtures} cálculos de combate roteirizado contra dados da ROM, crit/miss/dano/retaliação, Seth 30→13, limites, RNG e SRAM preservados.`);
})().catch(e=>{console.error(e);process.exitCode=1;});
