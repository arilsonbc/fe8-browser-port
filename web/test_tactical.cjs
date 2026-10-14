const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const dist=process.env.FE8_TEST_DIST||path.join(__dirname,'dist'),create=require(path.join(dist,'fe8.js'));
const rom=fs.readFileSync(path.join(__dirname,'../baserom.gba')),map=fs.readFileSync(path.join(__dirname,'../fireemblem8.map'),'utf8'),groups=JSON.parse(fs.readFileSync(path.join(__dirname,'generated/unit_assets.json')));
const addr=n=>parseInt(map.match(new RegExp('(0x[0-9a-f]+)\\s+'+n+'\\b'))[1],16)-0x8000000,classTable=addr('gClassData');
(async()=>{
 const m=await create({wasmBinary:fs.readFileSync(path.join(dist,'fe8.wasm'))});m._fe8_game_reset();m._fe8_save_reset_empty();const sram=m.HEAPU8.slice(m._fe8_save_buffer(),m._fe8_save_buffer()+32768);let cells=0,fixtures=0;
 const setup=chapter=>{m._fe8_event_reset();m._fe8_units_reset();assert.equal(m._fe8_map_start(chapter),0);for(const name of ['UnitDef_Event_PrologueAlly','UnitDef_Event_PrologueEnemy'])m._fe8_units_load(groups.find(g=>g.name===name).address,0);const w=m._fe8_map_info(1),h=m._fe8_map_info(2);m._fe8_unit_move(2,0,0);m._fe8_unit_move(1,Math.min(1,w-1),Math.min(1,h-1));for(let i=0;i<200;i++)m._fe8_units_tick();assert.equal(m._fe8_tactical_start(),0);return {w,h};};
 const reference=(index,w,h,weather)=>{
  const movement=Array.from({length:h},()=>Array(w).fill(255)),x=m._fe8_unit_info(index,3),y=m._fe8_unit_info(index,4),jid=m._fe8_unit_info(index,1),mov=m._fe8_unit_info(index,17),weatherIndex=weather===4?1:weather===1?2:0,costPointer=rom.readUInt32LE(classTable+(jid-1)*84+56+weatherIndex*4)-0x8000000;
  const blocked=new Set();for(let i=1;i<256;i++)if(m._fe8_unit_info(i,0)>0&&m._fe8_unit_info(i,2)===128&&!(m._fe8_unit_info(i,15)&(1|4|8|16)))blocked.add(m._fe8_unit_info(i,3)+','+m._fe8_unit_info(i,4));
  movement[y][x]=0;const queue=[[0,x,y]];
  while(queue.length){queue.sort((a,b)=>a[0]-b[0]);const [d,cx,cy]=queue.shift();if(d!==movement[cy][cx])continue;for(const [dx,dy] of [[1,0],[-1,0],[0,1],[0,-1]]){const nx=cx+dx,ny=cy+dy;if(nx<0||ny<0||nx>=w||ny>=h||blocked.has(nx+','+ny))continue;const value=d+rom[costPointer+m._fe8_map_terrain(nx,ny)];if(value<=mov&&value<movement[ny][nx]){movement[ny][nx]=value;queue.push([value,nx,ny]);}}}
  return movement;
 };
 for(let chapter=0;chapter<79;chapter++)for(const weather of [0,1,4])for(const index of [1,2]){
  const {w,h}=setup(chapter);m.HEAPU8[m._fe8_save_loaded_playst()+0x15]=weather;const expected=reference(index,w,h,weather);assert.equal(m._fe8_tactical_select(index),0);
  for(let y=0;y<h;y++)for(let x=0;x<w;x++){assert.equal(m._fe8_tactical_cost(x,y),expected[y][x],`Movement ${chapter}/${weather}/${index}/${x},${y}`);cells++;}
  assert.equal(m._fe8_tactical_cost(-1,0),-1);assert.equal(m._fe8_tactical_cancel(),0);assert.equal(m._fe8_tactical_info(0),0);fixtures++;
 }
 setup(0);m.HEAPU8[m._fe8_save_loaded_playst()+0x15]=0;m._fe8_unit_move(1,4,5);m._fe8_unit_move(2,4,4);for(let i=0;i<200;i++)m._fe8_units_tick();
 let p=m._fe8_map_pixels();const backdrop=m.HEAPU8.slice(p,p+153600);assert.equal(m._fe8_tactical_select(2),0);assert.ok(m._fe8_unit_info(2,15)&1,'Original UnitBeginAction hides the canonical unit');
 // Original range tile graphics/palette animation and 10/6 alpha blend, independently read from ROM.
 const image=addr('gUnknown_08A02EB4'),palette=addr('gUnknown_08A02F34');let rangePixels=0;
 for(let tick=0;tick<=62;tick++){
  if(tick)m._fe8_map_tick(0);if(![0,14,62].includes(tick))continue;p=m._fe8_map_pixels();
  for(let y=0;y<160;y++)for(let x=0;x<240;x++){
   let skip=Math.abs(x-m._fe8_map_info(7))<24&&Math.abs(y-m._fe8_map_info(8))<24;
   for(let i=1;i<256&&!skip;i++)if(m._fe8_unit_info(i,0)>0){const sx=m._fe8_unit_info(i,18),sy=m._fe8_unit_info(i,19);if(x>=sx-8&&x<sx+32&&y>=sy-20&&y<sy+20)skip=true;}
   if(skip)continue;const dx=x&15,dy=y&15,offset=(Math.floor(dy/8)*2+Math.floor(dx/8))*32+(dy&7)*4+Math.floor((dx&7)/2),index=rom[image+offset]>>((dx&1)*4)&15,visible=m._fe8_tactical_cost(Math.floor(x/16),Math.floor(y/16))<120;
   const color=rom.readUInt16LE(palette+(Math.floor(tick/2)%32+index-1)*2),pixel=(y*240+x)*4;
   for(let c=0;c<3;c++){let expected=backdrop[pixel+c];if(index&&visible){const v=Math.min(31,(((color>>(c*5))&31)*10+(expected>>3)*6)>>4);expected=(v<<3)|(v>>2);}assert.equal(m.HEAPU8[p+pixel+c],expected,'Original range pixel');}rangePixels++;
  }
 }
 assert.equal(m._fe8_tactical_move(4,4),-1,'Occupied ally is not a valid endpoint');assert.equal(m._fe8_tactical_move(11,9),-1,'Occupied enemy is not a valid endpoint');
 assert.equal(m._fe8_tactical_move(5,5),0);let ticks=0;while(m._fe8_tactical_info(0)===2){m._fe8_event_test_tick(0);assert.ok(++ticks<300);}assert.equal(m._fe8_tactical_info(0),3);assert.equal(m._fe8_unit_info(2,3),5);assert.equal(m._fe8_unit_info(2,4),5);
 assert.equal(m._fe8_tactical_cancel(),0);assert.equal(m._fe8_tactical_info(0),1);assert.equal(m._fe8_unit_info(2,3),4);assert.equal(m._fe8_unit_info(2,4),5);assert.equal(m._fe8_tactical_cancel(),0);assert.equal(m._fe8_unit_info(2,15)&1,0);
 assert.equal(m._fe8_tactical_select(2),0);assert.equal(m._fe8_tactical_move(5,5),0);while(m._fe8_tactical_info(0)===2)m._fe8_event_test_tick(0);assert.equal(m._fe8_tactical_wait(),0);assert.ok(m._fe8_unit_info(2,15)&2,'Wait grays the unit');assert.equal(m._fe8_tactical_select(2),-3);
 assert.equal(m._fe8_tactical_select(1),0);assert.equal(m._fe8_tactical_move(4,4),0);assert.equal(m._fe8_tactical_info(0),3);assert.equal(m._fe8_tactical_wait(),0);assert.equal(m._fe8_tactical_info(0),7,'Enemy phase started');assert.equal(m._fe8_tactical_info(2),128);assert.equal(m._fe8_tactical_end_turn(),-1,'Cannot skip the enemy phase');
 assert.deepEqual(m.HEAPU8.slice(m._fe8_save_buffer(),m._fe8_save_buffer()+32768),sram);m._fe8_game_reset();assert.equal(m._fe8_tactical_info(0),-1);
 // Exercise real input/controller ordering: X cancels a selected action before it can return to title.
 const game=await create({wasmBinary:fs.readFileSync(path.join(dist,'fe8.wasm'))});game._fe8_save_reset_empty();game._fe8_save_create(0,1);
 const tick=(key=0,n=1)=>{for(let i=0;i<n;i++)game._fe8_game_tick(key);},press=key=>{tick();tick(key);tick();},wait=state=>{for(let i=0;i<500&&game._fe8_game_state()!==state;i++)tick();assert.equal(game._fe8_game_state(),state);};
 game._fe8_game_reset();wait(1);press(8);wait(3);press(1);wait(8);press(1);wait(11);game._fe8_event_preview();tick(0,100);const before=game.HEAPU8.slice(game._fe8_save_buffer(),game._fe8_save_buffer()+32768);
 press(1);assert.equal(game._fe8_tactical_info(0),1);press(16);tick(0,6);press(1);while(game._fe8_tactical_info(0)===2)tick();assert.equal(game._fe8_tactical_info(0),3);
 press(2);assert.equal(game._fe8_game_state(),11);assert.equal(game._fe8_tactical_info(0),1);press(2);assert.equal(game._fe8_game_state(),11);assert.equal(game._fe8_tactical_info(0),0);
 press(16);tick(0,6);press(1);assert.equal(game._fe8_tactical_info(0),4);press(128);assert.equal(game._fe8_tactical_info(4),1);press(1);assert.equal(game._fe8_tactical_info(0),0,'Map menu Cancel');
 press(1);assert.equal(game._fe8_tactical_info(0),4);press(1);assert.equal(game._fe8_tactical_info(0),7);assert.deepEqual(game.HEAPU8.slice(game._fe8_save_buffer(),game._fe8_save_buffer()+32768),before);press(2);assert.equal(game._fe8_game_state(),11,'B cannot interrupt NPC movement/combat');for(let f=0;f<1500&&game._fe8_tactical_info(0)===7;f++){tick();if(game._fe8_event_info(2)>=0)game._fe8_event_advance();}assert.equal(game._fe8_tactical_info(0),0,'Enemy and ally phases return to player');press(2);wait(1);
 console.log(`OK: ${fixtures} alcances/${cells} células e ${rangePixels} pixels contra Dijkstra e recursos da ROM; seleção, movimento/cancelamento, Wait e fase inimiga; SRAM preservada.`);
})().catch(e=>{console.error(e);process.exitCode=1;});
