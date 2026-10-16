# Fire Emblem: The Sacred Stones — port nativo para navegador

Port em desenvolvimento em C/WebAssembly, sem emulador. Esta pasta contém o código da decompilação FE8U e as adaptações em `web/`. A base citada pelo README original é `laqieer/fireemblem8u`; o pacote local não contém o histórico Git nem um commit de origem verificável. O README original e avisos de ferramentas foram preservados. Não se atribui uma licença nova ao código ou aos recursos originais.

## Estado

Título, menu, controles, mapas, combate físico, fases, diálogos, retratos e áudio nativo estão integrados no percurso já validado. O percurso avança até o capítulo 4, incluindo Artur, magia de Artur/Lute, vilas, reforços e final até o pedido de capítulo 5 (índice interno6). Item/Equip/Discard, Trade, Supply, Steal, Rescue/Drop, Take/Give e Canto usam regras e rotinas originais com apresentação adaptada. Cinquenta e três suítes passaram no WASM normal e com AddressSanitizer; Game Over original, fala de morte e retorno ao título preservando o save foram validados; mapa-múndi, preparações completas e próxima transição continuam pendentes. A implantação no capítulo4 usa posições originais fixas para os aliados existentes; efeitos completos de magia continuam pendentes. Consulte `web/STATUS.md` para adaptações e limitações. Ainda não é o jogo completo; o início dos capítulos integrados pode ser gravado explicitamente no slot original. Suspensão do turno atual ainda não foi integrada. Tradução BR futura ainda não integrada.

## Compilar localmente

São necessários Ubuntu/WSL, Python 3, Node.js, Emscripten e as dependências da compilação original descritas no README original. Disponibilize sua própria ROM americana em `baserom.gba` (SHA-1 `c25b145e37456171ada4b0d440bf88a19f4d509f`) e compile a base original para obter ELF/mapa usados pelos geradores.

```bash
bash web/build.sh
bash web/check_memory.sh
python3 -m http.server 8000 --bind 127.0.0.1 --directory web/dist
```

Abra `http://127.0.0.1:8000/`. O áudio precisa de um gesto no navegador. Não limpe o armazenamento local: os saves existentes ficam no navegador.

## O que fica local

ROMs, ELF/mapas, saves, `web/generated/`, `web/dist/` e `work/` não devem ser enviados. Os geradores regeneram dados e recursos a partir da ROM local validada. O repositório conserva fontes, geradores, testes e documentação; binários compilados contêm recursos originais e não são publicação automática. Os workflows herdados da decompilação não validam este port e devem ser revistos antes de habilitar CI/Pages.
