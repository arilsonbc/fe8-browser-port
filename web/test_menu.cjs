const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const dist=process.env.FE8_TEST_DIST||path.join(__dirname,'dist');
const create=require(path.join(dist,'fe8.js'));
(async()=>{
 const m=await create({wasmBinary:fs.readFileSync(path.join(dist,'fe8.wasm'))});
 const tick=(keys=0,n=1)=>{for(let i=0;i<n;i++)m._fe8_game_tick(keys);};
 const press=key=>{tick(0);tick(key);tick(0);};
 const wait=state=>{for(let i=0;i<160&&m._fe8_game_state()!==state;i++)tick();assert.equal(m._fe8_game_state(),state);};
 const pixels=()=>{const p=m._fe8_game_pixels();return m.HEAPU8.slice(p,p+153600);};
 const menu=()=>{m._fe8_game_reset();wait(1);press(8);wait(3);assert.equal(m._fe8_menu_options(),48);};
 for(let fixture=0;fixture<8;fixture++){
  m._fe8_menu_test_profile(fixture,2);m._fe8_game_reset();wait(1);press(8);wait(3);
  const count=fixture.toString(2).replace(/0/g,'').length;
  const expected=32|(count?2|8:0)|(count>0&&count<3?4:0)|(count<3?16:0);
  assert.equal(m._fe8_menu_options(),expected,'Original options for every save bitmap');
 }
 m._fe8_menu_test_profile(0,2);menu();
 assert.equal(m._fe8_menu_selection(),0);press(64);assert.equal(m._fe8_menu_selection(),1,'New up wraps');
 press(128);assert.equal(m._fe8_menu_selection(),0);
 tick(128,40);assert.equal(m._fe8_menu_selection(),1,'Held down stops at last entry');tick(0);
 press(128);assert.equal(m._fe8_menu_selection(),0,'Fresh down wraps at boundary');
 assert.equal(m._fe8_game_tick(8),3,'Start does not confirm the original main menu');tick(0);
 const main=pixels();assert.ok(main.some((c,i)=>i%4!==3&&c>0));
 const afterWait=()=>{tick(0,96);return pixels();};
 assert.notDeepEqual(afterWait(),main,'Fog, palette and particles animate without keyboard');
 assert.equal(m._menu_video_errors(),0);
 press(128);press(1);wait(6);assert.equal(m._fe8_menu_selection(),0);
 press(1);wait(9);assert.equal(m._fe8_menu_request(),3,'Sound Room remains explicit pending feature');
 press(2);wait(6);press(2);wait(3);
 for(let choice=0;choice<3;choice++){
  menu();press(1);wait(7);assert.equal(m._fe8_menu_selection(),0,'Entering does not reuse confirmation');
  for(let i=0;i<choice;i++){press(128);tick(0,5);}
  assert.equal(m._fe8_menu_selection(),choice);
  const before=pixels();assert.equal(m._menu_video_errors(),0);
  press(8);wait(8);assert.equal(m._fe8_menu_difficulty(),choice);
  assert.equal(m._fe8_menu_selection(),0);press(64);assert.equal(m._fe8_menu_selection(),2);
  press(128);assert.equal(m._fe8_menu_selection(),0);press(128);assert.equal(m._fe8_menu_selection(),1);
  assert.notDeepEqual(pixels(),before,'Slot screen differs from difficulty');
  press(1);wait(9);assert.equal(m._fe8_menu_request(),1,'New game save write is a request, not fake persistence');
  tick(0,100);assert.equal(m._fe8_menu_options(),48,'Pending write does not create a fictitious save');
  press(2);wait(3);assert.equal(m._fe8_game_proc_count(),3);
 }
 menu();press(1);wait(7);press(2);wait(3);assert.equal(m._fe8_menu_difficulty(),3,'Difficulty cancel returns to main');
 for(let cycle=0;cycle<30;cycle++){
  press(2);wait(1);assert.equal(m._fe8_game_proc_count(),3,'Return title has only controller/title/sprites');
  press(8);wait(3);assert.equal(m._fe8_game_proc_count(),3,'Reopen menu has no leaked processes');
 }
 // Backend overlap must match ctc.c LIFO: newest sprite on the same layer wins.
 m._menu_video_test_scene(1);let p=m._menu_video_pixels();
 const rgb=(x,y)=>Array.from(m.HEAPU8.slice(p+(y*240+x)*4,p+(y*240+x)*4+3));
 assert.deepEqual(rgb(4,4),[255,0,0]);assert.deepEqual(rgb(6,6),[0,255,0]);assert.deepEqual(rgb(14,14),[0,0,0]);
 assert.equal(m._menu_video_errors(),0);
 m._menu_video_test_scene(2);p=m._menu_video_pixels();assert.deepEqual(rgb(4,4),[0,0,255]);assert.deepEqual(rgb(11,4),[255,0,0]);
 m._menu_video_test_scene(3);p=m._menu_video_pixels();assert.deepEqual(rgb(4,4),[255,0,0]);assert.deepEqual(rgb(11,4),[0,0,255]);
 m._menu_video_test_scene(4);p=m._menu_video_pixels();assert.deepEqual(rgb(6,6),[255,0,0],'OBJ priority beats insertion order');
 m._menu_video_test_scene(5);p=m._menu_video_pixels();assert.deepEqual(rgb(0,0),[255,0,0]);assert.deepEqual(rgb(6,0),[0,0,0],'Wrapped offscreen sprite is clipped');
 m._menu_video_test_scene(6);p=m._menu_video_pixels();assert.deepEqual(rgb(4,4),[0,0,0]);assert.deepEqual(rgb(8,4),[255,0,0],'Palette index zero is transparent');
 menu();for(let i=0;i<6000;i++){tick();if(i%20===0)pixels();}
 assert.equal(m._fe8_game_state(),3);assert.equal(m._fe8_game_proc_count(),3);assert.equal(m._menu_video_errors(),0);
 console.log('OK: 8 perfis de saves, wrap/repetição, Extras, 3 dificuldades, slots, cancelamentos, 30 ciclos e 6000 atualizações.');
 console.log('OK: fundo/neblina/sprites/textos visíveis, animação e precedência/transparência do compositor.');
})().catch(e=>{console.error(e);process.exitCode=1;});
