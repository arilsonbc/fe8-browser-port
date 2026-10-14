const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const dist=process.env.FE8_TEST_DIST||path.join(__dirname,'dist'),create=require(path.join(dist,'fe8.js')),rom=fs.readFileSync(path.join(__dirname,'../baserom.gba')),map=fs.readFileSync(path.join(__dirname,'../fireemblem8.map'),'utf8');
const addr=n=>parseInt(map.match(new RegExp('(0x[0-9a-f]+)\\s+'+n+'\\b'))[1],16)-0x8000000,ct=addr('gClassData'),pt=addr('gCharacterData'),it=addr('gItemData');
(async()=>{
 const binary=fs.readFileSync(path.join(dist,'fe8.wasm')),m=await create({wasmBinary:binary}),rng=await create({wasmBinary:binary});m._fe8_game_reset();m._fe8_save_reset_empty();const sram=m.HEAPU8.slice(m._fe8_save_buffer(),m._fe8_save_buffer()+32768);let cases=0;
 const get=(i,f)=>m._fe8_unit_info(i,f),units=()=>Array.from({length:255},(_,n)=>Array.from({length:57},(_,f)=>get(n+1,f))),battle=()=>Array.from({length:47},(_,f)=>m._fe8_battle_info(f));
 const weaponSlot=index=>[0,1,2,3,4].find(slot=>rom.readUInt32LE(it+(get(index,30+slot)&255)*36+8)&1);
 function stats(index,x,y){const cp=ct+(get(index,1)-1)*84,pp=pt+(get(index,0)-1)*52,item=get(index,30+weaponSlot(index)),row=it+(item&255)*36,t=m._fe8_map_terrain(x,y),terrain=off=>rom.readInt8(rom.readUInt32LE(cp+off)-0x8000000+t),con=rom.readInt8(cp+17)+rom.readInt8(pp+19)+get(index,21),speed=Math.max(0,get(index,9)-Math.max(0,rom[row+23]-con));return {hp:get(index,6),pow:get(index,7),skl:get(index,8),lck:get(index,12),def:get(index,10)+terrain(72),avoid:terrain(68)+speed*2+get(index,12),speed,mt:rom[row+21],hit:rom[row+22],type:rom[row+7],item,t};}
 for(const actor of [1,2,129,130,131])for(const target of (actor<128?[129,130,131]:[1,2]))for(const [dx,dy] of [[1,0],[-1,0],[0,1],[0,-1]])for(const seed of [0,41,257]){
  m._fe8_event_preview();const x=get(target,3)+dx,y=get(target,4)+dy;if(x<0||y<0||x>=m._fe8_map_info(1)||y>=m._fe8_map_info(2))continue;const a=stats(actor,x,y),b=stats(target,get(target,3),get(target,4)),before=units(),oldBattle=battle();m._InitRN(seed);rng._InitRN(seed);
  const triangle=(a.type===0&&b.type===2)||(a.type===1&&b.type===0)||(a.type===2&&b.type===1)?1:(a.type===2&&b.type===0)||(a.type===0&&b.type===1)||(a.type===1&&b.type===2)?-1:0;
  const damage=[Math.max(0,a.pow+a.mt+triangle-b.def),Math.max(0,b.pow+b.mt-triangle-a.def)],hit=[Math.max(0,Math.min(100,a.skl*2+a.hit+Math.floor(a.lck/2)+triangle*15-b.avoid)),Math.max(0,Math.min(100,b.skl*2+b.hit+Math.floor(b.lck/2)-triangle*15-a.avoid))];
  const hp=[a.hp,b.hp],order=[0,1];if(Math.abs(a.speed-b.speed)>=4)order.push(a.speed>b.speed?0:1);const changes=[];for(const side of order){if(!hp[0]||!hp[1])break;const d=Math.min(hp[1-side],damage[side]);hp[1-side]-=d;changes.push([side,d]);}
  assert.equal(m._fe8_battle_simulate(actor,target,x,y,weaponSlot(actor)),changes.length,`${actor}->${target}@${x},${y}`);
  for(const [field,expected] of [[0,hp[0]],[10,hp[1]],[1,a.pow+a.mt+triangle],[11,b.pow+b.mt-triangle],[2,a.def],[12,b.def],[3,hit[0]],[13,hit[1]],[5,a.speed],[15,b.speed],[6,x],[7,y],[8,a.t]])assert.equal(m._fe8_battle_simulation_info(field),expected,`simulation field ${field} ${actor}->${target}@${x},${y}`);
  for(let i=0;i<changes.length;i++){assert.equal(m._fe8_battle_simulation_info(30+i),changes[i][1]);assert.equal(m._fe8_battle_simulation_info(40+i)&3,0,'Simulation has neither misses nor critical hits');assert.equal(!!(m._fe8_battle_simulation_info(50+i)&8),!!changes[i][0]);}
  assert.equal(m._NextRN(),rng._NextRN(),'Simulation consumes no RNG');assert.deepEqual(units(),before,'No unit/stat/item/AI mutation');assert.deepEqual(battle(),oldBattle,'Live battle diagnostics restored');cases++;
 }
 m._fe8_event_preview();assert.ok(m._fe8_battle_simulate(2,130,-2,0,0)<0);assert.equal(m._fe8_battle_simulation_info(0),-1);assert.ok(m._fe8_battle_simulate(2,130,9,6,4)<0);assert.ok(m._fe8_battle_simulate(2,2,-1,-1,0)<0);
 // A pending scripted hit queue must survive a simulation.
 const q=m._fe8_save_import_buffer();m.HEAPU32[q/4]=(4<<8);m.HEAPU32[q/4+1]=255;assert.equal(m._fe8_battle_set_script(q,2),1);assert.ok(m._fe8_battle_simulate(2,130,8,5,1)>0);assert.equal(m._fe8_battle_start(1,130,0,0,0,0),1);assert.equal(m._fe8_battle_info(20),4);assert.ok(m._fe8_battle_simulate(2,130,8,5,1)<0,'Cannot overwrite active combat');for(let t=0;t<60;t++)m._fe8_event_test_tick(0);assert.equal(m._fe8_battle_info(9),1);
 assert.deepEqual(m.HEAPU8.slice(m._fe8_save_buffer(),m._fe8_save_buffer()+32768),sram);m._fe8_battle_reset();assert.equal(m._fe8_battle_simulation_info(0),-1);
 console.log(`OK: ${cases} simulações físicas/terreno contra dados ROM, dano/HP/retaliação/duplo/hits; RNG, unidades, dados de batalha, fila roteirizada e SRAM preservados; guardas/reset.`);
})().catch(e=>{console.error(e);process.exitCode=1;});

