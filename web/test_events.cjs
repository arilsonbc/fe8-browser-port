const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const dist=process.env.FE8_TEST_DIST||path.join(__dirname,'dist'),create=require(path.join(dist,'fe8.js'));
const scripts=JSON.parse(fs.readFileSync(path.join(__dirname,'generated/event_assets.json')));
(async()=>{
 const m=await create({wasmBinary:fs.readFileSync(path.join(dist,'fe8.wasm'))});
 const initial=scripts.find(s=>s.name==='EventScr_Prologue_BeginningScene').address;
 const snapshot=()=>m.HEAPU8.slice(m._fe8_save_buffer(),m._fe8_save_buffer()+32768);let pages=0;
 for(let difficulty=0;difficulty<3;difficulty++){
  m._fe8_game_reset();m._fe8_save_reset_empty();m._fe8_save_create(0,difficulty);m._fe8_save_load(0);const before=snapshot();m._InitRN(42);m._fe8_chapter_start();
  assert.equal(m._fe8_event_info(0),1);const initialPixels=m._fe8_map_pixels();assert.ok(m.HEAPU8.slice(initialPixels,initialPixels+153600).every((v,i)=>i%4===3||v===0),'No flash before initial cutscene fade');const trace=[];let previous=-1;
  for(let tick=0;tick<20000&&m._fe8_event_info(1)<0&&m._fe8_event_info(0);tick++){
   m._fe8_event_test_tick(0);const id=m._fe8_event_info(2);
   if(id>=0){if(id!==previous){trace.push([id,m._fe8_map_info(0),m._fe8_units_count()]);previous=id;}pages++;m._fe8_event_advance();}
  }
  const common=[[0x664,16,8],[0x903,16,9],[0x904,16,8],[0x905,16,7],[0x906,16,17],[0x907,64,3],[0x908,64,2],[0x909,64,5],[0x90b,64,5],[0x90c,64,3],[0x90d,0,2],[0x90e,0,2]];
  assert.deepEqual(trace,[...common,...(difficulty===0?[[0x919,0,2]]:[[0x90f,0,2],[0x910,0,5]])]);
  assert.equal(m._fe8_event_info(1),difficulty===0?11:-1,'Easy waits for conditional tutorial; Normal/Hard finish opening');assert.equal(m._fe8_battle_info(9),1);assert.equal(m._fe8_battle_info(7),13);assert.equal(m._fe8_battle_info(8),34);assert.equal(m._fe8_map_info(0),0);assert.ok(m._fe8_event_info(5)>900);
  assert.deepEqual(snapshot(),before,'Cutscene preserves SRAM');assert.equal(m._fe8_units_errors(),0);
  assert.equal(m._fe8_event_preview(),0);for(let i=0;i<150;i++)m._fe8_event_test_tick(0);assert.equal(m._fe8_event_info(6),1);assert.equal(m._fe8_event_busy(),0);assert.equal(m._fe8_map_info(0),0);assert.equal(m._fe8_units_count(),5);
  const units=Array.from({length:255},(_,i)=>i+1).filter(i=>m._fe8_unit_info(i,0)>0),eirika=units.find(i=>m._fe8_unit_info(i,0)===1),seth=units.find(i=>m._fe8_unit_info(i,0)===2);
  for(const [u,f,v] of [[eirika,3,4],[eirika,4,5],[seth,3,4],[seth,4,4],[seth,6,13]])assert.equal(m._fe8_unit_info(u,f),v);
  assert.equal(m._fe8_unit_info(eirika,31)&255,9);assert.deepEqual(snapshot(),before);
 }
 const guard=scripts.find(s=>s.name==='EventScr_CallOnTutorialMode').address;
 m._fe8_game_reset();m._fe8_save_reset_empty();m._fe8_save_create(0,1);m._fe8_save_load(0);assert.equal(m._fe8_event_start(guard),0);
 for(let i=0;i<40&&m._fe8_event_info(0);i++)m._fe8_event_test_tick(0);
 assert.equal(m._fe8_event_info(0),0);assert.equal(m._fe8_event_info(1),-1);
 assert.equal(m._fe8_event_start(0x06010000),-1);assert.equal(m._fe8_event_busy(),0);
 for(let i=0;i<30;i++){m._fe8_game_reset();m._fe8_event_start(initial);for(let frame=0;frame<6;frame++)m._fe8_event_test_tick(0);assert.ok(m._fe8_game_proc_count()>=5,'Real parent and blocking event fade exist');m._fe8_game_reset();assert.equal(m._fe8_units_count(),0);assert.equal(m._fe8_event_busy(),0);for(let frame=0;frame<160&&m._fe8_game_state()!==1;frame++)m._fe8_game_tick(0);assert.equal(m._fe8_game_state(),1);assert.equal(m._fe8_game_proc_count(),3);}
 console.log(`OK: 3 dificuldades, 13/14 diálogos por dificuldade/${pages} páginas, mapas/unidades, chamadas/retornos/desvios e 30 reinícios; SRAM preservada.`);
})().catch(e=>{console.error(e);process.exitCode=1;});
