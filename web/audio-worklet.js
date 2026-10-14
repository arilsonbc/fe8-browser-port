import './audio-engine.js';
class FE8AudioProcessor extends AudioWorkletProcessor {
 constructor(){super();this.engine=null;this.report=0;this.port.onmessage=({data})=>{try{if(data.type==='bank'){this.engine=new FE8AudioEngine(new FE8SoundBank(data.meta,data.bytes),sampleRate);this.port.postMessage({type:'ready'});}else if(data.type==='commands'&&this.engine)for(const c of data.commands)this.engine.command(c);}catch(error){this.port.postMessage({type:'error',message:error.message});}};}
 process(inputs,outputs){const out=outputs[0];if(this.engine&&out.length===2){this.engine.render(out[0],out[1]);if(this.engine.frames>=this.report+60){this.report=this.engine.frames;this.port.postMessage({type:'info',info:this.engine.info()});}}return true;}
}
registerProcessor('fe8-native-audio',FE8AudioProcessor);
