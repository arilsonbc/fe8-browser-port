(function(root){
 function attachSaveUI(module,store,reset){
  const section=document.createElement('section');section.className='save-panel';
  section.innerHTML='<h2>Saves locais</h2><p>Arquivos SRAM de 32 KiB. A importação substitui os slots locais e mantém uma cópia anterior.</p><p class="save-status" role="status"></p><p class="save-slots"></p><div class="save-actions"></div><p class="save-feedback" role="status"></p>';
  document.querySelector('#gameMount').after(section);
  const status=section.querySelector('.save-status'),slots=section.querySelector('.save-slots'),feedback=section.querySelector('.save-feedback'),actions=section.querySelector('.save-actions');
  const button=(label,action)=>{const b=document.createElement('button');b.textContent=label;b.onclick=action;actions.append(b);return b;};
  const update=()=>{
   status.textContent=store.status;status.style.color=store.error?'#fbbf24':'';
   slots.textContent=[0,1,2].map(slot=>module._fe8_save_valid(slot)?`Slot ${slot+1}: capítulo ${module._fe8_save_chapter(slot)} · ${['Easy','Normal','Difficult'][module._fe8_save_difficulty(slot)]}`:`Slot ${slot+1}: vazio`).join(' | ')+(module._fe8_save_suspend_slot()>=0?' · Há suspensão preservada; Resume ainda não integrado.':'');
  };
  button('Exportar .sav',()=>{
   const data=store.snapshot(),url=URL.createObjectURL(new Blob([data],{type:'application/octet-stream'})),a=document.createElement('a');
   a.href=url;a.download='fireemblem8-web.sav';a.click();setTimeout(()=>URL.revokeObjectURL(url),10000);feedback.textContent='Arquivo SRAM exportado: 32 KiB.';
  });
  const input=document.createElement('input');input.type='file';input.accept='.sav,.srm,application/octet-stream';input.hidden=true;input.setAttribute('aria-label','Arquivo SRAM para importar');section.append(input);
  button('Importar .sav',()=>input.click());
  input.onchange=async()=>{
   const file=input.files[0];if(!file)return;
   try{
    if(file.size!==32768){feedback.textContent='Importação recusada: o arquivo precisa ter exatamente 32 KiB. Os saves atuais foram preservados.';return;}
    const bytes=new Uint8Array(await file.arrayBuffer()),result=store.importBytes(bytes);
    if(!result.ok){feedback.textContent='Importação recusada: assinatura, checksum ou blocos inválidos. Os saves atuais foram preservados.';return;}
    reset();feedback.textContent=result.warnings?'Importado. Blocos adicionais foram preservados; suspensão, arena e mapas extras ainda não são executáveis neste port.':'Save importado e validado.';update();
   }catch(error){feedback.textContent='Erro ao importar: '+error.message;}finally{input.value='';}
  };
  button('Restaurar cópia anterior',()=>{const result=store.restorePrevious();if(result.ok){reset();feedback.textContent='Cópia anterior restaurada.';update();}else feedback.textContent=result.message;});
  button('Tentar guardar novamente',()=>{store.flush(true);update();});
  update();let previous=module._fe8_save_revision();
  return {tick(){const current=module._fe8_save_revision();if(current!==previous){store.flush();update();previous=current;}}};
 }
 root.attachFE8SaveUI=attachSaveUI;
})(globalThis);
