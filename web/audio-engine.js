/* Native M4A data sequencer and mixer. No CPU, MMIO, ROM execution or emulator.
 * Commands follow m4a_1.s; PCM pitch/volume/envelope follow m4a.c/SoundMainRAM.
 * Float mixing, interpolation, PSG oscillators and output rate are host adaptations.
 */
(function(root){
'use strict';
class FE8SoundBank {
 constructor(meta,bytes){this.meta=meta;this.bytes=new Uint8Array(bytes);this.view=new DataView(this.bytes.buffer,this.bytes.byteOffset,this.bytes.byteLength);this.samples=new Map();if(this.bytes.length!==meta.size)throw Error('Banco de áudio incompleto');}
 offset(a,n=1){const p=a-this.meta.base;if(!Number.isInteger(p)||p<0||p+n>this.bytes.length)throw Error('Chave de áudio fora do banco: '+a.toString(16));return p;}
 u8(a){return this.bytes[this.offset(a)];} s8(a){const n=this.u8(a);return n<128?n:n-256;}
 u16(a){return this.view.getUint16(this.offset(a,2),true);} u32(a){return this.view.getUint32(this.offset(a,4),true);}
 clock(i){if(i<0||i>48)throw Error('Duração M4A inválida');return this.u8(this.meta.tables.gClockTable+i);}
 tone(a,key){const type=this.u8(a);if(type&0xc0){const group=this.u32(a+4),index=type&0x40?this.u8(this.u32(a+8)+key):key;const b=group+index*12;if(this.u8(b)&0xc0)throw Error('Tone split recursivo');return {address:b,key:type&0x80?this.u8(b+1):key,pan:type&0x80&&this.u8(b+3)&0x80?(this.u8(b+3)-0xc0)*2:0};}return {address:a,key,pan:0};}
 wave(a){if(this.samples.has(a))return this.samples.get(a);const type=this.u16(a),size=this.u32(a+12),loop=this.u32(a+8);if(type!==0||!size||size>2000000||loop>=size)throw Error('Formato de amostra pendente: '+type);this.offset(a+16,size);const pcm=new Float32Array(size);for(let i=0;i<size;i++)pcm[i]=this.s8(a+16+i)/128;const w={pcm,loop,looping:!!(this.u8(a+3)&0xc0),freq:this.u32(a+4)};this.samples.set(a,w);return w;}
 frequency(w,key,fine){if(key>178){key=178;fine=255;}key=Math.max(0,key);const tab=this.meta.tables;const a=this.u8(tab.gScaleTable+key),b=this.u8(tab.gScaleTable+key+1);const f1=Math.floor(this.u32(tab.gFreqTable+(a&15)*4)/2**(a>>4));const f2=Math.floor(this.u32(tab.gFreqTable+(b&15)*4)/2**(b>>4));return Math.floor(w.freq*(f1+Math.floor((f2-f1)*fine/256))/4294967296);}
}
class FE8AudioEngine {
 constructor(bank,rate=48000){this.bank=bank;this.rate=rate;this.frames=0;this.errors=[];this.trace=null;this.reset();}
 reset(){this.players=new Map();this.voices=[];this.pending=null;this.current=0;this.saved=null;this.sampleClock=0;this.frames=0;this.errors=[];this.notes=0;this.peak=0;this.maxChannels=7;}
 error(p,e){this.errors.push({song:p?p.id:-1,frame:this.frames,message:String(e.message||e)});if(this.errors.length>32)this.errors.shift();if(p){this.players.delete(p.slot);this.voices=this.voices.filter(v=>v.player!==p);}}
 start(id,bgm=false){if(bgm&&id<128)this.maxChannels=[1,2,66,67,64,86,116].includes(id)?8:7;const s=this.bank.meta.songs[id];if(!s||s.name==='dummy_song'||!s.tracks){if(id===0)return null;throw Error('Música sem dados: '+id);}const a=s.address,n=this.bank.u8(a),slot=s.player;const old=this.players.get(slot);if(old&&!bgm&&old.priority>this.bank.u8(a+2)&&old.tracks.some(t=>t.active))return null;if(old)this.voices=this.voices.filter(v=>v.player!==old);const p={id,slot,bgm,priority:this.bank.u8(a+2),tone:this.bank.u32(a+4),tempo:150,acc:0,gain:1,ramp:null,ticks:0,tracks:[]};for(let i=0;i<n;i++)p.tracks.push({pc:this.bank.u32(a+8+i*4),active:true,wait:0,status:0,stack:[],rep:0,voice:0,vol:0,pan:0,bend:0,bendr:2,keysh:0,tune:0,lfoSpeed:22,lfoPhase:0,lfoDelay:0,lfoDelayC:0,mod:0,modt:0,modM:0,key:0,velocity:0,priority:0,echoVolume:0,echoLength:0,overrides:{}});this.players.set(slot,p);return p;}
 bgmPlayers(){return [...this.players.values()].filter(p=>p.bgm);}
 ramp(p,to,frames,stop=false){if(!p)return;p.ramp={from:p.gain,to,duration:Math.max(1,frames),clock:0,stop};}
 command(c){try{const [kind,a,b,d]=c;
  if(kind===0){this.reset();return;}
  if(kind===1){if(this.current===a&&this.bgmPlayers().some(p=>p.id===a)&&!this.pending)return;this.saved=null;this.pending=null;const ps=this.bgmPlayers();if(ps.length&&b>0){for(const p of ps)this.ramp(p,0,b*16,true);this.pending={id:a,delay:b*16+1};}else{this.stopBgm();if(a)this.start(a,true);}this.current=a;}
  else if(kind===2){this.start(a,false);}
  else if(kind===3){this.stopBgm();this.pending=null;this.saved=null;const p=a?this.start(a,true):null;if(p){p.gain=0;this.ramp(p,1,b*16);}this.current=a;}
  else if(kind===4){this.pending=null;for(const p of this.bgmPlayers())this.ramp(p,0,a*16,true);this.current=0;}
  else if(kind===5){if(!this.saved)this.saved={players:this.bgmPlayers(),voices:this.voices.filter(v=>v.player.bgm),id:this.current};this.stopBgm();this.current=a;this.pending=a?{id:a,delay:33}:null;}
  else if(kind===6){this.pending=null;this.stopBgm();if(this.saved){for(const p of this.saved.players){p.gain=0;this.players.set(p.slot,p);this.ramp(p,1,a*16);}this.voices.push(...this.saved.voices);this.current=this.saved.id;this.saved=null;}}
  else if(kind===7){for(const p of this.bgmPlayers()){p.gain=a/256;this.ramp(p,b/256,d,b===0);}if(!b)this.current=0;}
  else throw Error('Pedido de áudio desconhecido: '+kind);
 }catch(e){this.error(null,e);}}
 stopBgm(){for(const p of this.bgmPlayers())this.players.delete(p.slot);this.voices=this.voices.filter(v=>!v.player.bgm);}
 byte(t){return this.bank.u8(t.pc++);}
 pointer(t){const a=this.bank.u32(t.pc);t.pc+=4;this.bank.offset(a);return a;}
 sequence(p,t){const bank=this.bank;let steps=0;
  for(const v of this.voices)if(v.track===t&&v.gate>0&&!--v.gate)v.releasing=true;
  while(t.active&&!t.wait){if(++steps>256)throw Error('Loop M4A sem espera');let op=bank.u8(t.pc);if(op>=128){t.pc++;if(op>=0xbd)t.status=op;}else op=t.status;if(op<128)throw Error('Running status M4A inválido');
   if(this.trace)this.trace({song:p.id,tick:p.ticks,pc:t.pc,op});
   if(op<=0xb0)t.wait=bank.clock(op-128);
   else if(op>=0xcf){let gate=bank.clock(op-0xcf);if(bank.u8(t.pc)<128){t.key=this.byte(t);if(bank.u8(t.pc)<128){t.velocity=this.byte(t);if(bank.u8(t.pc)<128)gate=(gate+this.byte(t))&255;}}this.note(p,t,gate);}
   else if(op===0xb1){t.active=false;for(const v of this.voices)if(v.track===t)v.releasing=true;}
   else if(op===0xb2)t.pc=this.pointer(t);
   else if(op===0xb3){if(t.stack.length>=3)throw Error('Stack M4A excedido');const dest=this.pointer(t);t.stack.push(t.pc);t.pc=dest;}
   else if(op===0xb4){if(t.stack.length)t.pc=t.stack.pop();}
   else if(op===0xb5){const count=this.byte(t),dest=this.pointer(t);if(!count||++t.rep<count)t.pc=dest;else t.rep=0;}
   else if(op===0xba)t.priority=this.byte(t);
   else if(op===0xbb)p.tempo=this.byte(t)*2;
   else if(op===0xbc)t.keysh=this.byte(t)<<24>>24;
   else if(op===0xbd){t.voice=this.byte(t);t.overrides={};bank.offset(p.tone+t.voice*12,12);}
   else if(op===0xbe)t.vol=this.byte(t);
   else if(op===0xbf)t.pan=this.byte(t)-64;
   else if(op===0xc0)t.bend=this.byte(t)-64;
   else if(op===0xc1)t.bendr=this.byte(t);
   else if(op===0xc2){t.lfoSpeed=this.byte(t);if(!t.lfoSpeed)t.lfoPhase=t.modM=0;}
   else if(op===0xc3)t.lfoDelay=this.byte(t);
   else if(op===0xc4){t.mod=this.byte(t);if(!t.mod)t.lfoPhase=t.modM=0;}
   else if(op===0xc5)t.modt=this.byte(t);
   else if(op===0xc8)t.tune=this.byte(t)-64;
   else if(op===0xce){let key=t.key;if(bank.u8(t.pc)<128)key=this.byte(t);for(const v of this.voices)if(v.track===t&&v.note===key)v.releasing=true;}
   else if(op===0xcd){const sub=this.byte(t);if(sub===1)t.overrides.wav=this.pointer(t);else if([2,4,5,6,7,8,9,10,11].includes(sub)){const value=this.byte(t);if(sub===8)t.echoVolume=value;else if(sub===9)t.echoLength=value;else t.overrides[sub]=value;}else throw Error('XCMD pendente: '+sub);}
   else throw Error('Opcode M4A pendente: '+op.toString(16));
  }
  if(t.wait)t.wait--;
  if(t.lfoSpeed&&t.mod){if(t.lfoDelayC)t.lfoDelayC--;else{t.lfoPhase=(t.lfoPhase+t.lfoSpeed)&255;const q=t.lfoPhase;const triangle=((q-64)<<24)>=0?128-q:q<<24>>24;t.modM=(triangle*t.mod)>>6;}}
 }
 note(p,t,gate){const b=this.bank,r=b.tone(p.tone+t.voice*12,t.key),a=r.address,type=t.overrides[2]??b.u8(a),cgb=type&7;if(cgb>4)throw Error('Tone CGB inválido');const wav=t.overrides.wav??b.u32(a+4);let wave=null;
  if(!cgb)wave=b.wave(wav);
  else if(cgb===3){b.offset(wav,16);const pcm=new Float32Array(32);for(let i=0;i<32;i++)pcm[i]=((b.u8(wav+(i>>1))>>(i%2?0:4)&15)-7.5)/8;wave={pcm,loop:0,looping:true};}
  const priority=Math.min(255,p.priority+t.priority);
  if(cgb){const old=this.voices.find(v=>v.cgb===cgb&&!v.dead);if(old&&old.priority>priority)return;this.voices=this.voices.filter(v=>v!==old);}
  else if(this.voices.filter(v=>!v.cgb&&!v.dead).length>=this.maxChannels){const old=this.voices.filter(v=>!v.cgb&&!v.dead).sort((x,y)=>x.priority-y.priority||x.env-y.env)[0];if(old.priority>priority)return;this.voices=this.voices.filter(v=>v!==old);}
  const v={player:p,track:t,type,cgb,wave,note:t.key,key:r.key,pan:r.pan,gate,velocity:t.velocity,position:0,env:0,stage:3,releasing:false,dead:false,priority,attack:t.overrides[4]??b.u8(a+8),decay:t.overrides[5]??b.u8(a+9),sustain:t.overrides[6]??b.u8(a+10),release:t.overrides[7]??b.u8(a+11),echoVolume:t.echoVolume,echoLength:t.echoLength,duty:[.125,.25,.5,.75][wav&3],noise:0x7fff,noiseCounter:0,shortNoise:!!(wav&1)};
  // PSG envelopes are expressed in hardware units; translated to 60 Hz host steps.
  if(cgb){v.attack=v.attack?Math.ceil(255/(v.attack*4)):255;v.decay=v.decay?Math.max(0,256-Math.ceil(256/(v.decay*4))):0;v.sustain=Math.round(v.sustain*17);v.release=v.release?Math.max(0,256-Math.ceil(256/(v.release*4))):0;}
  t.lfoDelayC=t.lfoDelay;if(t.lfoDelay)t.lfoPhase=t.modM=0;this.voices.push(v);this.notes++;if(this.onNote)this.onNote({song:p.id,track:p.tracks.indexOf(t),tick:p.ticks,key:t.key,velocity:t.velocity,gate});
 }
 frame(){this.frames++;
  if(this.pending&&!--this.pending.delay){const id=this.pending.id;this.pending=null;this.stopBgm();try{this.start(id,true);}catch(e){this.error(null,e);}}
  for(const p of [...this.players.values()]){try{
   if(p.ramp){const r=p.ramp;p.gain=r.from+(r.to-r.from)*Math.min(1,++r.clock/r.duration);if(r.clock>=r.duration){p.ramp=null;if(r.stop){this.players.delete(p.slot);this.voices=this.voices.filter(v=>v.player!==p);continue;}}}
   p.acc+=p.tempo;let steps=0;while(p.acc>=150){if(++steps>4)throw Error('Tempo inválido');p.acc-=150;for(const t of p.tracks)if(t.active)this.sequence(p,t);p.ticks++;}
   if(!p.tracks.some(t=>t.active)&&!this.voices.some(v=>v.player===p))this.players.delete(p.slot);
  }catch(e){this.error(p,e);}}
  for(const v of this.voices){if(v.releasing){v.env=v.env*v.release>>8;if(v.env<=v.echoVolume){if(v.echoVolume&&v.echoLength>0){v.env=v.echoVolume;v.echoLength--;}else v.dead=true;}}
   else if(v.stage===3){v.env+=v.attack;if(v.env>=255){v.env=255;v.stage=2;}}
   else if(v.stage===2){v.env=v.env*v.decay>>8;if(v.env<=v.sustain){v.env=v.sustain;v.stage=1;if(!v.env)v.releasing=true;}}
   const t=v.track;const pitch=(t.tune+t.bend*t.bendr)*4+(t.keysh<<8)+(t.modt===0?t.modM*16:0);const key=Math.max(0,v.key+(pitch>>8)),fine=pitch&255;
   if(!v.cgb)v.step=((v.type&8)?13379:this.bank.frequency(v.wave,key,fine))/this.rate;
   else if(v.cgb===4){const k=Math.min(59,Math.max(0,key-21)),reg=this.bank.u8(this.bank.meta.tables.gNoiseTable+k),ratio=reg&7;v.step=(524288/(ratio||.5)/2**((reg>>4)+1))/this.rate;}
   else v.step=(440*2**((key+fine/256-69)/12))/this.rate*(v.cgb===3?32:1);
   let x=t.vol*2;if(t.modt===1)x=x*(t.modM+128)>>7;const pan=Math.max(-128,Math.min(127,t.pan*2+v.pan+(t.modt===2?t.modM:0)));const amplitude=v.env/255*v.velocity/127*v.player.gain*.35;
   v.left=((127-pan)*x>>8)/127*amplitude;v.right=((pan+128)*x>>8)/127*amplitude;
  }
  this.voices=this.voices.filter(v=>!v.dead);
 }
 render(left,right){for(let i=0;i<left.length;i++){if(this.sampleClock<=0){this.frame();this.sampleClock+=this.rate/60;}this.sampleClock--;let l=0,r=0;
  for(const v of this.voices){if(v.dead)continue;let sample=0;if(v.cgb===1||v.cgb===2){sample=v.position<v.duty?1:-1;v.position=(v.position+v.step)%1;}
   else if(v.cgb===4){v.noiseCounter+=v.step;while(v.noiseCounter>=1){v.noiseCounter--;const bit=(v.noise^(v.noise>>1))&1;v.noise=(v.noise>>1)|(bit<<14);if(v.shortNoise)v.noise=(v.noise&~64)|(bit<<6);}sample=(v.noise&1)?-1:1;}
   else {const w=v.wave;if(v.position>=w.pcm.length){if(w.looping)v.position=w.loop+(v.position-w.loop)%(w.pcm.length-w.loop);else{v.dead=true;continue;}}const pos=Math.floor(v.position),next=pos+1<w.pcm.length?pos+1:w.looping?w.loop:pos;sample=w.pcm[pos]+(w.pcm[next]-w.pcm[pos])*(v.position-pos);v.position+=v.step;}
   l+=sample*v.left;r+=sample*v.right;}
  this.peak=Math.max(this.peak,Math.abs(l),Math.abs(r));left[i]=Math.max(-1,Math.min(1,l));right[i]=Math.max(-1,Math.min(1,r));}
 }
 info(){return {frames:this.frames,current:this.current,players:[...this.players.values()].map(p=>({song:p.id,ticks:p.ticks,gain:p.gain})),voices:this.voices.length,notes:this.notes,peak:this.peak,errors:this.errors.slice()};}
}
root.FE8SoundBank=FE8SoundBank;root.FE8AudioEngine=FE8AudioEngine;
if(typeof module==='object')module.exports={FE8SoundBank,FE8AudioEngine};
})(globalThis);
