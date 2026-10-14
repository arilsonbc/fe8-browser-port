const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const dist=process.env.FE8_TEST_DIST||path.join(__dirname,'dist'),create=require(path.join(dist,'fe8.js'));
(async()=>{
 const m=await create({wasmBinary:fs.readFileSync(path.join(dist,'fe8.wasm'))});
 const tick=(key=0,n=1)=>{for(let i=0;i<n;i++)m._fe8_game_tick(key);},press=key=>{tick();tick(key);tick();};
 const wait=state=>{for(let n=0;n<400&&m._fe8_game_state()!==state;n++)tick();assert.equal(m._fe8_game_state(),state);};
 const menu=()=>{m._fe8_game_reset();wait(1);press(8);wait(3);};
 const select=bit=>{for(let i=0;i<6;i++){const options=m._fe8_menu_options();const bits=[1,2,4,8,16,32,64,128].filter(b=>options&b);if(bits[m._fe8_menu_selection()]===bit){press(1);return;}press(128);}assert.fail('Option missing '+bit);};
 const bytes=()=>m.HEAPU8.slice(m._fe8_save_buffer(),m._fe8_save_buffer()+32768);
 m._fe8_save_reset_empty();
 for(let difficulty=0;difficulty<3;difficulty++){
  menu();select(16);wait(7);for(let i=0;i<difficulty;i++){press(128);tick(0,6);}press(8);wait(8);
  assert.equal(m._fe8_menu_selection(),difficulty,'First empty slot');press(1);wait(11);
  assert.equal(m._fe8_save_valid(difficulty),1);assert.equal(m._fe8_save_difficulty(difficulty),difficulty);
  assert.equal(m._fe8_save_chapter(difficulty),0);assert.equal(m._fe8_map_active(),1);assert.equal(m._fe8_map_info(0),0);
  assert.equal(m._fe8_event_preview(),0);tick(0,80);const before=bytes();press(16);tick(0,6);assert.equal(m._fe8_map_info(3),5);assert.deepEqual(bytes(),before,'Map exploration does not write runtime game saves');
  press(2);wait(1);assert.equal(m._fe8_map_active(),0);
 }
 menu();assert.equal(m._fe8_menu_options(),42);select(8);wait(8);assert.equal(m._fe8_menu_selection(),2);
 const beforeErase=bytes();press(1);assert.equal(m._fe8_menu_confirmation(),2,'Erase defaults to Cancel');press(1);assert.equal(m._fe8_menu_confirmation(),0);assert.deepEqual(bytes(),beforeErase);
 press(1);press(32);assert.equal(m._fe8_menu_confirmation(),1);press(1);wait(3);assert.equal(m._fe8_save_valid(2),0);
 select(4);wait(8);assert.equal(m._fe8_menu_selection(),0);press(1);assert.equal(m._fe8_menu_selection(),2,'Copy destination must be empty');assert.equal(m._fe8_save_valid(2),0);press(1);wait(3);assert.equal(m._fe8_save_valid(2),1);assert.equal(m._fe8_save_difficulty(2),0);
 for(let cycle=0;cycle<25;cycle++){
  menu();select(2);wait(8);press(1);wait(11);assert.equal(m._fe8_map_active(),1);assert.equal(m._fe8_game_proc_count(),1);m._fe8_event_preview();press(2);wait(1);assert.equal(m._fe8_game_proc_count(),3);
 }
 // Imported suspend records stay opaque until the user confirms the original New Game dialog.
 m._fe8_save_reset_empty();let suspended=Buffer.from(bytes());
 suspended.fill(0,0xd4,0x204c);suspended[0xd4+12]=1;suspended[0xd4+14]=4;
 const checksum=buf=>{let sum=0,xor=0;for(let i=0;i<buf.length;i+=2){const v=buf[i]|buf[i+1]<<8;sum=(sum+v)&65535;xor^=v;}return ((xor<<16)|sum)>>>0;};
 const h=0x64+3*16;suspended.writeUInt32LE(0x40624,h);suspended.writeUInt16LE(0x200a,h+4);suspended[h+6]=1;suspended[h+7]=0;suspended.writeUInt16LE(0xd4,h+8);suspended.writeUInt16LE(0x1f78,h+10);suspended.writeUInt32LE(checksum(suspended.subarray(0xd4,0x204c)),h+12);
 m.HEAPU8.set(suspended,m._fe8_save_import_buffer());assert.equal(m._fe8_save_import(32768),0);assert.equal(m._fe8_save_suspend_slot(),1);
 menu();assert.equal(m._fe8_menu_options(),48,'Unavailable Resume is not offered');select(16);wait(7);press(8);wait(8);press(1);assert.equal(m._fe8_menu_confirmation(),2);assert.deepEqual(Buffer.from(bytes()),suspended);
 press(1);assert.equal(m._fe8_menu_confirmation(),0);assert.deepEqual(Buffer.from(bytes()),suspended,'Cancel preserves imported suspension');
 press(1);press(32);press(1);wait(11);assert.equal(m._fe8_save_valid(0),1);assert.equal(m._fe8_save_suspend_slot(),-1,'Confirmed original New Game invalidates suspension');
 assert.equal(m._fe8_save_errors(),0);assert.equal(m._menu_video_errors(),0);
 console.log('OK: criação real via menu nas 3 dificuldades, Cancel/Erase, Copy, Restart e 25 ciclos capítulo/título sem vazamentos.');
})().catch(e=>{console.error(e);process.exitCode=1;});

