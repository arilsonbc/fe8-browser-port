const {setup,assert}=require('./chapter3-test-helpers.cjs');
(async()=>{
 for(const difficulty of [1,2])for(const variant of ['absent','green','dead','blue-neimi-dead','green-neimi-dead']){
  const q=await setup(difficulty),{m,pid,move,select,drain,tick,messages}=q;
  // Explicit branch fixtures: deaths use the original scripted-hit decoder;
  // positions/healing isolate the normal boss fight, not a complete playthrough.
  const kill=id=>{const u=pid(id),p=m._fe8_save_import_buffer();assert.ok(u);m.HEAPU32[p/4]=m._fe8_unit_info(u,6)<<8;m.HEAPU32[p/4+1]=255;assert.equal(m._fe8_battle_start(2,id,0,0,p,2),1);while(m._fe8_battle_busy())tick();assert.equal(m._fe8_unit_info(u,6),0);};
  if(variant!=='absent'){assert.equal(m._fe8_tactical_end_turn(),0);drain();assert.ok(pid(9));}
  const recruited=variant==='dead'||variant==='blue-neimi-dead';
  if(recruited){move(pid(9),2,8);select(pid(8),2,9);assert.equal(m._fe8_tactical_begin_talk(),0);assert.equal(m._fe8_tactical_talk(),0);q.finish();}
  if(variant==='dead')kill(9);
  if(variant.endsWith('neimi-dead'))kill(8);
  const originalColm=pid(9),colmItems=originalColm?Array.from({length:5},(_,s)=>m._fe8_unit_info(originalColm,30+s)):null;
  const seth=pid(2),boss=pid(72);move(seth,14,2);
  for(let fights=0;m._fe8_unit_info(boss,6)>0;fights++){
   assert.ok(fights<30);if(m._fe8_unit_info(seth,6)<m._fe8_unit_info(seth,5)){assert.ok(m._fe8_unit_give(2,0x6d)>0);const slot=Array.from({length:5},(_,s)=>s).find(s=>(m._fe8_unit_info(seth,30+s)&255)===0x6d);assert.ok(m._fe8_recovery_item(seth,slot)>0);}
   m._InitRN(71+fights);assert.ok(m._fe8_battle_attack(seth,boss,-1)>0);while(m._fe8_battle_busy())tick();drain();assert.ok(m._fe8_unit_info(seth,6)>0);
  }
  select(pid(1),14,1);assert.equal(m._fe8_tactical_seize(),0);drain(true);
  assert.equal(m._fe8_event_info(1),42,variant);assert.equal(m._fe8_event_info(9),4);
  assert.equal(messages.has(0x995),variant==='green',variant);
  for(const id of [0x996,0x997,0x998,0x999])assert.ok(messages.has(id));
  const colm=pid(9);assert.ok(colm,variant);assert.equal(m._fe8_unit_info(colm,2),0);
  const shouldDie=['absent','dead','green-neimi-dead'].includes(variant);
  assert.equal(m._fe8_unit_info(colm,6)===0,shouldDie,variant);assert.equal(!!(m._fe8_unit_info(colm,15)&4),shouldDie);
  if(colmItems)assert.deepEqual(Array.from({length:5},(_,s)=>m._fe8_unit_info(colm,30+s)),colmItems,'Faction conversion preserves packed items');
  else {assert.deepEqual(Array.from({length:5},(_,s)=>m._fe8_unit_info(colm,30+s)),[0,0,0,0,0]);assert.ok(m._fe8_unit_info(colm,15)&8,'StrictLoadUniqueAlly marks missing Colm not deployed');}
  if(variant.endsWith('neimi-dead'))assert.equal(m._fe8_unit_info(pid(8),6),0);
  q.checkSave();
  console.log('Ch3 variant',difficulty,variant,'Colm',colm,'dead',shouldDie);
 }
 console.log('OK: dez finais originais do capítulo3, Colm ausente/verde/morto e Neimi morta, SPAWN/CUSA/StrictLoadUniqueAlly, itens e SRAM preservados; mortes/posições são fixtures.');
})().catch(e=>{console.error(e);process.exitCode=1;});
