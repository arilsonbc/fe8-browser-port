(function(root){
 const SIZE=32768,KEY='fe8-web-native-sram-v1',BACKUP=KEY+'-previous';
 function encode(bytes){let text='';for(let i=0;i<bytes.length;i+=4096)text+=String.fromCharCode(...bytes.subarray(i,i+4096));return btoa(text);}
 function decode(text){
  if(typeof text!=='string'||text.length!==43692||!/^[A-Za-z0-9+/]+=$/.test(text))throw Error('Dados locais inválidos');
  const binary=atob(text);if(binary.length!==SIZE)throw Error('Tamanho de SRAM inválido');
  return Uint8Array.from(binary,c=>c.charCodeAt(0));
 }
 class FE8Storage{
  constructor(module,storage){this.module=module;this.storage=storage;this.revision=-1;this.record=null;this.status='';this.error=false;}
  snapshot(){const p=this.module._fe8_save_buffer();return this.module.HEAPU8.slice(p,p+SIZE);}
  apply(bytes){
   if(!(bytes instanceof Uint8Array)||bytes.length!==SIZE)return -1;
   this.module.HEAPU8.set(bytes,this.module._fe8_save_import_buffer());return this.module._fe8_save_import(bytes.length);
  }
  initialize(){
   this.snapshot();
   try{
    const raw=this.storage.getItem(KEY);
    if(raw){const record=JSON.parse(raw);if(record.version!==1||record.variant!=='FE8U-SRAM')throw Error('Formato local desconhecido');
     const result=this.apply(decode(record.data));if(result!==0)throw Error('Save local inválido: '+result);
     this.record=record;this.status='Saves locais restaurados.';this.revision=this.module._fe8_save_revision();return true;
    }
    return this.flush(true);
   }catch(error){this.error=true;this.status='Não foi possível restaurar os saves locais: '+error.message;this.revision=this.module._fe8_save_revision();return false;}
  }
  flush(force=false){
   const revision=this.module._fe8_save_revision();if(!force&&revision===this.revision)return !this.error;
   const data=encode(this.snapshot());
   if(this.record&&data===this.record.data){this.revision=revision;return !this.error;}
   const record={version:1,variant:'FE8U-SRAM',data};
   try{
    // Keep the previous valid state. Rewrites of identical bytes don't replace this backup.
    if(this.record)this.storage.setItem(BACKUP,JSON.stringify(this.record));
    this.storage.setItem(KEY,JSON.stringify(record));
    this.record=record;this.revision=revision;this.error=false;this.status='Saves guardados neste navegador.';return true;
   }catch(error){this.revision=revision;this.error=true;this.status='Os saves estão na memória, mas não foram guardados: '+error.message;return false;}
  }
  importBytes(bytes){
   const result=this.apply(bytes);if(result!==0)return {ok:false,result};
   return {ok:true,persisted:this.flush(true),warnings:this.module._fe8_save_import_warnings()};
  }
  restorePrevious(){
   try{const raw=this.storage.getItem(BACKUP);if(!raw)return {ok:false,message:'Não há cópia anterior.'};
    const record=JSON.parse(raw);if(record.version!==1||record.variant!=='FE8U-SRAM')throw Error('Cópia anterior inválida');
    const result=this.apply(decode(record.data));if(result!==0)return {ok:false,message:'Cópia anterior inválida: '+result};
    return {ok:true,persisted:this.flush(true)};
   }catch(error){return {ok:false,message:error.message};}
  }
 }
 FE8Storage.KEY=KEY;FE8Storage.BACKUP=BACKUP;FE8Storage.SIZE=SIZE;
 if(typeof module==='object')module.exports=FE8Storage;else root.FE8Storage=FE8Storage;
})(globalThis);
