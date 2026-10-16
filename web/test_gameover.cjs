const {setup,assert,fs,path}=require('./chapter3-test-helpers.cjs');
const rom=fs.readFileSync(path.join(__dirname,'../baserom.gba')),map=fs.readFileSync(path.join(__dirname,'../fireemblem8.map'),'utf8'),addr=n=>parseInt(map.match(new RegExp('(0x[0-9a-f]+)\\s+'+n+'\\b'))[1],16)-0x8000000;
const event=JSON.parse(fs.readFileSync(path.join(__dirname,'generated/event_assets.json'))).find(e=>e.name==='EventScr_GameOver').address;
function unpack(p){assert.equal(rom[p],16);const out=Buffer.alloc(rom.readUIntLE(p+1,3));let r=p+4,w=0;while(w<out.length){const bits=rom[r++];for(let bit=7;bit>=0&&w<out.length;bit--){if(bits&(1<<bit)){const value=rom.readUInt16BE(r);r+=2;for(let n=0;n<(value>>12)+3&&w<out.length;n++){out[w]=out[w-(value&4095)-1];w++;}}else out[w++]=rom[r++];}}return out;}
const text=unpack(addr('Img_GameOverText')),fog=unpack(addr('Img_ChapterIntroFog')),tsa=addr('Tsa_GameOverFx'),palText=addr('Pal_GameOverText2'),palFog=addr('Pal_GameOverText1');
const ink=(gfx,tile,x,y)=>gfx[tile*32+y*4+(x>>1)]>>((x&1)*4)&15;
function color(p,component,ticks=32){return Math.max(0,Math.min(31,(rom.readUInt16LE(p)>>(component*5)&31)+ticks-32));}
let pixelsChecked=0;
function compare(m){const phase=m._fe8_gameover_info(0),counter=m._fe8_gameover_info(2),ticks=phase===2?10+21-counter:phase===4?32-(counter+1):32,p=m._fe8_gameover_pixels(),scroll=[4,5,6,7].map(f=>m._fe8_gameover_info(f));
 const fogPixel=(x,y,layer)=>{const px=(x+scroll[layer?0:2])&255,py=(y+scroll[layer?1:3])&255,tx=px>>3,ty=py>>3,tile=(ty%8)*32+(ty%16>=8?16:0)+(layer?15-tx%16:tx%16);return ink(fog,tile,layer?7-(px&7):px&7,py&7);};
 for(let y=0;y<160;y++)for(let x=0;x<240;x++){let front=0;const tx=(x>>3)-7,ty=(y>>3)-9,w=rom[tsa]+1,h=rom[tsa+1]+1;if(tx>=0&&tx<w&&ty>=0&&ty<h){const entry=rom.readUInt16LE(tsa+2+((h-1-ty)*w+tx)*2);assert.equal(entry>>12,0);front=ink(text,entry&1023,entry&1024?7-(x&7):x&7,entry&2048?7-(y&7):y&7);}const a=fogPixel(x,y,1),b=fogPixel(x,y,0),alpha=Math.min(16,Math.floor(Math.min(y,160-y)/3));for(let c=0;c<3;c++){const value=front?color(palText+front*2,c,ticks):a&&b?Math.min(31,(color(palFog+a*2,c,ticks)*alpha+color(palFog+b*2,c,ticks)*alpha)>>4):a||b?color(palFog+(a||b)*2,c,ticks):color(palText,c,ticks);assert.equal(m.HEAPU8[p+(y*240+x)*4+c],value<<3|value>>2,`Game Over ${phase}/${counter} pixel ${x},${y},${c}`);}assert.equal(m.HEAPU8[p+(y*240+x)*4+3],255);pixelsChecked++;}assert.equal(m._fe8_gameover_info(8),0);
}
(async()=>{
 for(const key of [0,1,2,8,16]){
  const q=await setup(),{m}=q,baseline=m._fe8_game_proc_count(),sram=m.HEAPU8.slice(m._fe8_save_buffer(),m._fe8_save_buffer()+32768);assert.equal(m._fe8_gameover_confirm(),-1);assert.equal(m._fe8_event_call(event,0),0);let frames=0,entry=-1;
  const tick=k=>{assert.ok(++frames<2400);m._fe8_event_test_tick(k);assert.equal(m._fe8_event_info(1),-1);};
  while(m._fe8_gameover_info(0)!==2)tick(0);assert.equal(m._fe8_gameover_confirm(),-1);tick(1);assert.equal(m._fe8_gameover_info(0),2,'A does not skip fade-in');compare(m);
  while(m._fe8_gameover_info(0)!==3)tick(0);entry=frames;assert.equal(m._fe8_gameover_info(2),-1);assert.equal(m._fe8_gameover_info(3),1499);compare(m);const palettes=m._fe8_palette_buffer();for(let i=0;i<16;i++){assert.equal(m.HEAPU16[palettes/2+i],rom.readUInt16LE(palText+i*2));assert.equal(m.HEAPU16[palettes/2+64+i],rom.readUInt16LE(palFog+i*2));}
  if(key===16){tick(16);assert.equal(m._fe8_gameover_info(0),3,'Direction does not leave Game Over');assert.equal(m._fe8_gameover_confirm(),0);tick(0);}else if(key)tick(key);
  if(key){while(m._fe8_gameover_info(0)!==4)tick(0);assert.ok(frames-entry<4);}else {while(m._fe8_gameover_info(0)===3)tick(0);assert.equal(frames-entry,1501,'Original idle timeout');}
  compare(m);for(let n=0;n<15;n++)tick(0);compare(m);while(m._fe8_gameover_info(0)!==5)tick(0);assert.equal(m._fe8_game_proc_count(),baseline,'All Game Over processes/children end');assert.equal(m._fe8_gameover_info(8),0);assert.deepEqual(m.HEAPU8.slice(m._fe8_save_buffer(),m._fe8_save_buffer()+32768),sram);
  assert.equal(m._fe8_game_tick(0),0);for(let n=0;n<500&&m._fe8_game_state()!==1;n++)m._fe8_game_tick(0);assert.equal(m._fe8_game_state(),1);assert.equal(m._fe8_gameover_info(0),0);assert.deepEqual(m.HEAPU8.slice(m._fe8_save_buffer(),m._fe8_save_buffer()+32768),sram);
 }
 console.log(`OK: Game Over original, ${pixelsChecked} pixels ROM/TSA/fog/blend/paletas em fade-in/idle/fade-out, A/B/Start/confirmação/direção/timeout1501, processos finalizados, título e SRAM preservada; disparo direto do evento é fixture.`);
})().catch(e=>{console.error(e);process.exitCode=1;});
