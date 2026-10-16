# Fire Emblem: The Sacred Stones — port nativo para navegador

Port em desenvolvimento em C/WebAssembly, sem emulador. Esta pasta contém o código da decompilação FE8U e as adaptações em `web/`. A base citada pelo README original é `laqieer/fireemblem8u`; o pacote local não contém o histórico Git nem um commit de origem verificável. O README original e avisos de ferramentas foram preservados. Não se atribui uma licença nova ao código ou aos recursos originais.

## Estado

Título, menu, controles, mapas, combate, fases, diálogos, retratos e áudio nativo estão integrados no percurso até o capítulo 5. Natasha/Joshua, quatro vilas, reforços, arsenal/loja, compra/venda e final até o pedido do capítulo 5x usam dados e regras originais com apresentação adaptada. Cinquenta e oito suítes passaram no WASM normal e com AddressSanitizer. A campanha contínua com seed42 completou prólogo..5 no Normal (com baixas e Joshua não recrutado); no Difficult completou prólogo..4 e entrou em5. Os cenários específicos do capítulo5 passaram nas duas dificuldades; o piloto contínuo Difficult5 ainda perde unidades e não conclui a missão.

Preparações livres, mapa-múndi completo e entrada no capítulo5x continuam pendentes. A implantação usa posições originais fixas para aliados existentes; animações/efeitos completos e tradução BR ainda não estão integrados. O início dos capítulos integrados pode ser gravado explicitamente no slot SRAM original; suspensão do turno atual continua pendente. Consulte `web/STATUS.md` para adaptações e evidências de validação. Ainda não é o jogo completo.

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
