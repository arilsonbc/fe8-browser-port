const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const dist=process.env.FE8_TEST_DIST||path.join(__dirname,'dist');
const create = require(path.join(dist,'fe8.js'));
const Clock = require('./clock.js');
for(const hz of [30,60,120,144]){
 let frames=0;const clock=new Clock(()=>frames++);
 for(let i=0;i<=hz*10;i++)clock.advance(i*1000/hz);
 assert.equal(frames,600,'Same simulation speed at '+hz+' Hz');
 clock.reset();clock.advance(100000);assert.equal(frames,600,'Resume does not catch up hidden time');
}
console.log('OK: relógio de 60 atualizações em telas de 30, 60, 120 e 144 Hz; pausa sem avanço.');
(async () => {
  const m = await create({wasmBinary: fs.readFileSync(path.join(dist, 'fe8.wasm'))});
  assert.equal(m._fe8_rng_selftest(), 0);
  const tiles=fs.readFileSync(path.join(__dirname,'../graphics/titlescreen/title_fire_emblem_logo.4bpp'));
  const palette=fs.readFileSync(path.join(__dirname,'../graphics/titlescreen/title_fire_emblem_logo.gbapal'));
  const pointer=m._fe8_graphics_pixels();
  for(let y=0;y<64;y++)for(let x=0;x<256;x++){
    const index=(tiles[(Math.floor(y/8)*32+Math.floor(x/8))*32+(y%8)*4+Math.floor((x%8)/2)] >>> ((x%2)*4))&15;
    const color=palette.readUInt16LE(index*2);
    const channels=[color&31,(color>>>5)&31,(color>>>10)&31].map(c=>Math.round(c*255/31));
    const actual=m.HEAPU8.slice(pointer+(y*256+x)*4,pointer+(y*256+x)*4+4);
    // Bit replication and rounded scaling can differ by one at 5-bit precision.
    channels.forEach((c,i)=>assert.ok(Math.abs(actual[i]-c)<=1));
    assert.equal(actual[3],index?255:0);
  }
  console.log('OK: 16384 pixels de gráficos originais conferidos.');
  const titlePointer=m._fe8_title_pixels();
  let visible=0;
  for(let y=0;y<160;y++)for(let x=0;x<240;x++){
    const offset=titlePointer+(y*240+x)*4;
    assert.equal(m.HEAPU8[offset+3],255);
    const lit=m.HEAPU8[offset]||m.HEAPU8[offset+1]||m.HEAPU8[offset+2];
    if(lit){ visible++; assert.ok(y>=48 && y<80 && x>=4); }
  }
  assert.ok(visible>100,'Logo must contain visible pixels');
  console.log('OK: composição do logotipo, limites e transparência conferidos.');
  const screenPtr=m._fe8_screen_pixels(0);
  const hidden=m.HEAPU8.slice(screenPtr,screenPtr+240*160*4);
  m._fe8_screen_pixels(1);
  let changed=0;
  for(let y=0;y<160;y++)for(let x=0;x<240;x++){
    const i=(y*240+x)*4;
    assert.equal(m.HEAPU8[screenPtr+i+3],255);
    if([0,1,2].some(c=>hidden[i+c]!==m.HEAPU8[screenPtr+i+c])){
      changed++;assert.ok(x>=72 && x<168 && y>=124 && y<140);
    }
  }
  assert.ok(changed>10,'Press Start must produce visible changes');
  console.log('OK: composição de camadas e alternância de Press Start conferidas.');
  m._fe8_input_reset();
  m._fe8_input_frame(1);
  assert.equal(m._fe8_input_held(), 1);
  assert.equal(m._fe8_input_new(), 1);
  for(let i=1;i<=12;i++) {
    m._fe8_input_frame(1);
    assert.equal(m._fe8_input_new(), 0);
    assert.equal(m._fe8_input_repeat(), i===12 ? 1 : 0);
  }
  m._fe8_input_frame(0); assert.equal(m._fe8_input_held(), 0);
  m._fe8_input_frame(65); assert.equal(m._fe8_input_new(), 65);
  console.log('OK: controles originais, combinação e repetição conferidos.');
  assert.equal(m._fe8_proc_selftest(), 0, 'Processos: espera, callbacks, bloqueio e reutilização');
  m._fe8_proc_demo_start();
  for (let frame = 1; frame <= 600; frame++) assert.equal(m._fe8_proc_demo_frame(), frame);
  // Independent integer reference, including the GBA's 16-bit truncation.
  for (const seed of [0, 1, 7, 23, 12345]) {
    const table = [0xA36E,0x924E,0xB784,0x4F67,0x8092,0x592D,0x8E70,0xA794];
    let j = seed % 7;
    let s = [table[j++ & 7],table[j++ & 7],table[j & 7]];
    const advance = () => {
      const r = (((s[1] << 11) + (s[0] >>> 5)) ^ (((s[2] * 2) + (s[1] >>> 15)) & 65535)) & 65535;
      s = [r,s[0],s[1]];
      return r;
    };
    for (let k = 0; k < seed % 23; k++) advance();
    m._InitRN(seed);
    for (let k = 0; k < 1000; k++) assert.equal(m._NextRN(), advance());
  }
  console.log('OK: 5000 valores WASM conferidos; estado e faixa validados.');
  console.log('OK: processos originais e 600 atualizações conferidas.');
  m._fe8_title_reset();
  for(let i=0;i<814;i++)assert.equal(m._fe8_title_tick(0),-1);
  assert.equal(m._fe8_title_tick(0),1,'Original attract-mode timeout at 815 updates');
  assert.equal(m._fe8_title_sound_request(),-1);
  for(const key of [1,8]){
    m._fe8_title_reset();assert.equal(m._fe8_title_tick(key),0);
    assert.equal(m._fe8_title_sound_request(),0x6d);
  }
  m._fe8_title_reset();assert.equal(m._fe8_title_tick(2),-1,'B must not open menu');
  m._fe8_title_reset();assert.equal(m._fe8_title_tick(0),-1);
  assert.equal(m._fe8_title_idle_frames(),1);
  console.log('OK: título original: A/Start, B ignorado e timeout de 815 atualizações.');
  const pulse=fs.readFileSync(path.join(__dirname,'../graphics/titlescreen/title_unk_palette_1.gbapal'));
  m._fe8_title_reset();
  let last,changes=0;
  for(let frame=0;frame<192;frame++){
    m._fe8_title_tick(0);
    const phase=(Math.floor(frame/3)+1)&31;
    const index=(phase&16)?phase&15:(31-phase)&15;
    assert.equal(m._fe8_title_pulse_color(),pulse.readUInt16LE(index*2));
    const p=m._fe8_title_render();const pixels=m.HEAPU8.slice(p,p+240*160*4);
    if(last)for(let y=0;y<160;y++)for(let x=0;x<240;x++){
      const i=(y*240+x)*4;
      if([0,1,2].some(c=>pixels[i+c]!==last[i+c])){
        changes++;assert.ok(x>=72&&x<168&&y>=124&&y<140,'Animation confined to Press Start');
      }
    }
    last=pixels;
  }
  assert.ok(changes>0,'Palette animation must visibly change pixels');
  m._fe8_title_tick(1);m._fe8_title_tick(0);m._fe8_title_tick(64);
  assert.equal(m._fe8_input_new(),64,'Input continues at action boundary');
  const demo=await create({wasmBinary:fs.readFileSync(path.join(dist,'fe8.wasm'))});
  demo._fe8_proc_demo_start();
  for(let i=1;i<=100;i++){m._fe8_title_reset();assert.equal(demo._fe8_proc_demo_frame(),i);}
  console.log('OK: 192 atualizações da paleta original, pixels, controles após ação e isolamento de processos.');

  // Endpoints across all original FadeCore kinds and rates. Slow fades perform
  // one extra component update due to the original first-tick special case.
  for(let kind=0;kind<8;kind++)for(const speed of [4,8,16,32,64]){
    m._fe8_fade_test_start(kind,speed);
    let frames=0;
    while(m._fe8_fade_test_tick())assert.ok(++frames<140,'Fade must terminate');
    const p=m._fe8_palette_buffer();
    for(let i=1;i<512;i++){
      const original=(i*67)&32767;
      let expected;
      if([1,3].includes(kind))expected=0;
      else if([5,7].includes(kind))expected=32767;
      else {
        const adjustment=speed<16?1:0;
        const sign=[0,2].includes(kind)?1:-1;
        expected=[0,5,10].map(shift=>Math.max(0,Math.min(31,((original>>shift)&31)+sign*adjustment)))
          .reduce((color,c,index)=>color|(c<<(index*5)),0);
      }
      assert.equal(m.HEAPU16[p/2+i],expected,`Fade ${kind}/${speed}, color ${i}`);
    }
  }
  console.log('OK: 40 combinações de fade original, 20440 cores finais e encerramento.');
  const readFrame=()=>{const p=m._fe8_game_pixels();return m.HEAPU8.slice(p,p+240*160*4);};
  const black=frame=>{for(let i=0;i<frame.length;i+=4){assert.equal(frame[i]|frame[i+1]|frame[i+2],0);assert.equal(frame[i+3],255);}};
  for(const key of [1,8]){
    m._fe8_game_reset();black(readFrame());
    let state=0,frames=0;
    while(state===0){state=m._fe8_game_tick(0);assert.ok(++frames<25);}
    assert.equal(state,1);assert.equal(frames,17,'Original locking entry fade');
    assert.ok(readFrame().some((c,i)=>i%4!==3&&c>0));
    assert.equal(m._fe8_title_idle_frames(),1);
    // B is ignored in the idle state; release before testing A/Start.
    assert.equal(m._fe8_game_tick(2),1);m._fe8_game_tick(0);
    assert.equal(m._fe8_game_tick(key),1,'Exit fade starts on next process update');
    assert.equal(m._fe8_title_sound_request(),0x6d);
    assert.equal(m._fe8_game_tick(key),2);
    assert.equal(m._fe8_game_volume_request(),-1,'Controller still blocked by title fade');
    frames=0;
    while(m._fe8_game_state()!==3){m._fe8_game_tick(0);assert.ok(++frames<65);}
    assert.equal(m._fe8_game_state(),3,'Original controller selects save menu');
    assert.equal(m._fe8_game_volume_request(),192,'Original music volume request');
    assert.equal(m._fe8_game_proc_count(),3,'Controller, menu and draw process; title and fades cleaned up');
    assert.ok(readFrame().some((c,i)=>i%4!==3&&c>0),'Original menu now visible');
    for(let i=0;i<60;i++)assert.equal(m._fe8_game_tick(0),3,'Menu remains active');
  }
  m._fe8_game_reset();
  while(m._fe8_game_state()===0)m._fe8_game_tick(0);
  for(let i=1;i<814;i++)assert.equal(m._fe8_game_tick(0),1);
  assert.equal(m._fe8_title_idle_frames(),814);
  m._fe8_game_tick(0);assert.equal(m._fe8_title_idle_frames(),815);
  for(let i=0;i<30&&m._fe8_game_state()<3;i++)m._fe8_game_tick(0);
  assert.equal(m._fe8_game_state(),4,'Original controller selects class reel after timeout');
  assert.equal(m._fe8_title_sound_request(),-1);
  for(let reset=0;reset<100;reset++){
    m._fe8_game_reset();assert.equal(m._fe8_game_proc_count(),4);black(readFrame());
    m._fe8_game_tick(8);assert.equal(m._fe8_title_idle_frames(),0,'Input cannot bypass blocking fade');
  }
  console.log('OK: título → fades → controlador: A/Start, B, timeout, cores, limpeza e 100 reinícios.');
})().catch(e => { console.error(e); process.exitCode = 1; });
