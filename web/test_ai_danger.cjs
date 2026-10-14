const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const dist=process.env.FE8_TEST_DIST||path.join(__dirname,'dist'),create=require(path.join(dist,'fe8.js')),rom=fs.readFileSync(path.join(__dirname,'../baserom.gba')),map=fs.readFileSync(path.join(__dirname,'../fireemblem8.map'),'utf8'),groups=JSON.parse(fs.readFileSync(path.join(__dirname,'generated/unit_assets.json')));
const addr=n=>parseInt(map.match(new RegExp('(0x[0-9a-f]+)\\s+'+n+'\\b'))[1],16)-0x8000000,ct=addr('gClassData'),it=addr('gItemData');
(async()=>{
 const binary=fs.readFileSync(path.join(dist,'fe8.wasm')),m=await create({wasmBinary:binary}),rng=await create({wasmBinary:binary});m._fe8_game_reset();m._fe8_save_reset_empty();const sram=m.HEAPU8.slice(m._fe8_save_buffer(),m._fe8_save_buffer()+32768);
 const get=(i,f)=>m._fe8_unit_info(i,f),snapshot=()=>Array.from({length:255},(_,n)=>Array.from({length:57},(_,f)=>get(n+1,f)));
 function setup(actor=[8,6],player=[4,5],weather=0){m._fe8_event_preview();m._fe8_unit_move_index(130,...actor);m._fe8_unit_move_index(2,...player);for(let i=0;i<200;i++)m._fe8_units_tick();m.HEAPU8[m._fe8_save_loaded_playst()+21]=weather;}
 const allied=(a,b)=>(((a^b)&128)===0);
 let wraps=0;
 function dangerReference(index,fixBug=false){
  const w=m._fe8_map_info(1),h=m._fe8_map_info(2),weather=m.HEAPU8[m._fe8_save_loaded_playst()+21],result=Array.from({length:h},()=>Array(w).fill(0)),occupied=new Map();
  for(let i=1;i<256;i++)if(get(i,0)>0&&!(get(i,15)&(1|4|8|16)))occupied.set(get(i,3)+','+get(i,4),i);
  let item=0,might=0;
  for(let i=1;i<192;i++){
   if(get(i,0)<1||(get(i,15)&(1|4|8|65536))||allied(index,i))continue;
   if(fixBug)item=might=0;
   for(let slot=0;slot<5;slot++){
    const v=get(i,30+slot);if(!v)break;const row=it+(v&255)*36,attrs=rom.readUInt32LE(row+8),type=rom[row+7];
    if(!(attrs&1)||type>7||get(i,40+type)<rom[row+28])continue;
    if(rom[row+21]>might){item=v;might=rom[row+21];}
   }
   if(!item)continue;const range=rom[it+(item&255)*36+25],min=range>>4,max=range&15;
   if(Math.abs(get(index,3)-get(i,3))+Math.abs(get(index,4)-get(i,4))>get(index,17)+get(i,17)+max)continue;
   const cp=ct+(get(i,1)-1)*84,ptr=rom.readUInt32LE(cp+56+(weather===4?1:weather===1?2:0)*4)-0x8000000;
   const costs=Array.from({length:h},()=>Array(w).fill(255)),queue=[[0,get(i,3),get(i,4)]];costs[get(i,4)][get(i,3)]=0;
   while(queue.length){queue.sort((a,b)=>a[0]-b[0]);const [d,x,y]=queue.shift();if(costs[y][x]!==d)continue;for(const [dx,dy] of [[1,0],[-1,0],[0,1],[0,-1]]){const nx=x+dx,ny=y+dy;if(nx<0||ny<0||nx>=w||ny>=h)continue;const uid=occupied.get(nx+','+ny);if(uid&&!allied(i,uid))continue;const value=d+rom[ptr+m._fe8_map_terrain(nx,ny)];if(value<=get(i,17)&&value<costs[ny][nx]){costs[ny][nx]=value;queue.push([value,nx,ny]);}}}
   const coverage=Array.from({length:h},()=>Array(w).fill(0));for(let sy=0;sy<h;sy++)for(let sx=0;sx<w;sx++)if(costs[sy][sx]<=120)for(let y=0;y<h;y++)for(let x=0;x<w;x++){const d=Math.abs(x-sx)+Math.abs(y-sy);if(d>=min&&d<=max)coverage[y][x]=(coverage[y][x]+1)&255;}
   for(let y=0;y<h;y++)for(let x=0;x<w;x++)if(coverage[y][x]){const sum=result[y][x]+((get(i,7)+might)>>1);if(sum>255)wraps++;result[y][x]=sum&255;}
  }return result;
 }
 let dangerCases=0,cells=0,bugCells=0;
 for(const weather of [0,1,4])for(const actor of [[8,6],[12,8],[5,5]])for(const player of [[4,5],[1,1],[10,7]]){
  setup(actor,player,weather);const expected=dangerReference(130),fixed=dangerReference(130,true),before=snapshot();m._InitRN(75);rng._InitRN(75);assert.equal(m._fe8_ai_danger_probe(130),0);
  for(let y=0;y<expected.length;y++)for(let x=0;x<expected[y].length;x++){assert.equal(m._fe8_ai_danger_at(x,y),expected[y][x],`${weather}/${actor}/${player} danger ${x},${y}`);bugCells+=expected[y][x]!==fixed[y][x];cells++;}assert.deepEqual(snapshot(),before);assert.equal(m._NextRN(),rng._NextRN());dangerCases++;
 }
 assert.ok(bugCells,'Fixtures distinguish original accumulated-item bug from a correction');assert.equal(m._fe8_ai_danger_at(-1,0),-1);assert.equal(m._fe8_ai_danger_probe(0),-1);
 setup();for(let n=0;n<15;n++)assert.ok(m._fe8_units_load(groups.find(g=>g.name==='UnitDef_Event_PrologueEnemy').address,0)>0);const wrapReference=dangerReference(2);assert.ok(wraps>0,'Dense enemy fixture exercises original unsigned-byte overflow');assert.equal(m._fe8_ai_danger_probe(2),0);for(let y=0;y<wrapReference.length;y++)for(let x=0;x<wrapReference[y].length;x++)assert.equal(m._fe8_ai_danger_at(x,y),wrapReference[y][x]);
 // Duplicate generic IDs must never redirect an indexed movement to the first unit.
 setup();const group=groups.find(g=>g.name==='UnitDef_Event_PrologueEnemy');m._fe8_units_load(group.address,0);const ids=Array.from({length:255},(_,n)=>n+1).filter(i=>get(i,0)===130);assert.ok(ids.length>=2);const first=[get(ids[0],3),get(ids[0],4)],last=ids.at(-1);assert.equal(m._fe8_unit_move_index(last,2,2),0);for(let i=0;i<200;i++)m._fe8_units_tick();assert.deepEqual([get(ids[0],3),get(ids[0],4)],first);assert.deepEqual([get(last,3),get(last,4)],[2,2]);const q=m._fe8_save_import_buffer();m.HEAPU32[q/4]=3|(2<<6);m.HEAPU32[q/4+1]=0;assert.equal(m._fe8_unit_move_queue_index(last,q,2),0);for(let i=0;i<200;i++)m._fe8_units_tick();assert.deepEqual([get(last,18),get(last,19)],[48,32]);assert.deepEqual([get(ids[0],3),get(ids[0],4)],first);assert.equal(m._fe8_unit_move_index(0,1,1),-1);assert.equal(m._fe8_unit_move_queue_index(last,0,2),-1);
 setup();m._fe8_tactical_start();m._fe8_tactical_select(2);const w=m._fe8_map_info(1),h=m._fe8_map_info(2),costs=Array.from({length:w*h},(_,i)=>m._fe8_tactical_cost(i%w,Math.floor(i/w))),p=m._fe8_map_pixels(),pixels=m.HEAPU8.slice(p,p+153600);assert.equal(m._fe8_ai_danger_probe(130),0);assert.deepEqual(Array.from({length:w*h},(_,i)=>m._fe8_tactical_cost(i%w,Math.floor(i/w))),costs);const p2=m._fe8_map_pixels();assert.deepEqual(m.HEAPU8.slice(p2,p2+153600),pixels);assert.deepEqual(m.HEAPU8.slice(m._fe8_save_buffer(),m._fe8_save_buffer()+32768),sram);
 console.log(`OK: ${dangerCases} mapas de perigo/${cells} células contra Dijkstra e ROM, ${bugCells} células distinguem bug original; movimento por índice com genéricos duplicados; RNG/unidades/alcance/pixels/SRAM preservados.`);
})().catch(e=>{console.error(e);process.exitCode=1;});

