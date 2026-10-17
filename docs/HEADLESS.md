# Testes locais antes do PSP

O fluxo usa três níveis: testes rápidos da fila no host; o código MIPS e o
EBOOT no PPSSPPHeadless; depois, confirmação de diferenças de hardware no PSP.
Não precisa de janela, interação manual ou acesso ao PSPLink para os dois
primeiros níveis. ROMs e artefatos ficam somente em `dist/`, ignorado pelo Git.

## Fila de áudio no host

```sh
cmake -S tests/audio -B dist/tests/audio -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build dist/tests/audio --parallel 2
ctest --test-dir dist/tests/audio --output-on-failure
```

ASan e UBSan são habilitados por padrão. Para conferir concorrência com TSan:

```sh
cmake -S tests/audio -B dist/tests/audio-tsan \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFROGN64_SANITIZER=thread
cmake --build dist/tests/audio-tsan --parallel 2
ctest --test-dir dist/tests/audio-tsan -R audio-full --output-on-failure
```

Os seis casos verificam ordem/canais estéreo, silêncio no underrun, pacotes de
uma amostra, reamostragem de uma rampa conhecida, independência das fronteiras
de pacotes, wrap e produção concorrente de 100 mil amostras em uma fila pequena.
Cada teste tem timeout: um bloqueio da fila falha em vez de prender o processo.
O workflow **Host regression tests** executa ASan/UBSan em Linux.

Na base `9a715c0`, o caso FIFO devolvia três das quatro amostras, começando por
memória não inicializada. O bloco maior que a fila não terminava. A correção
escreve na posição atual, publica blocos parciais antes de esperar, cede CPU
quando cheia e usa operações acquire/release. A fase e a última amostra da
reamostragem continuam entre pacotes; a taxa igual não descarta amostras.
O contrato é um produtor e um consumidor.

## Projeções no host e no MIPS

```sh
cmake -S tests/graphics -B dist/tests/graphics -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build dist/tests/graphics --parallel 2
ctest --test-dir dist/tests/graphics --output-on-failure
```

É necessário GLM nos includes (`libglm-dev` no Ubuntu). Para uma cópia local,
use `-DGLM_INCLUDE_DIR=/diretorio/que/contem/glm`. Cinco casos analíticos verificam
projeções misturadas no cache, o caminho sem mudança, mil mudanças consecutivas,
cópia/recarga e normalização/reset. O controle reproduz o deslocamento original.
ASan/UBSan são habilitados por padrão. O workflow de regressão executa os casos.

```sh
docker run --rm --platform linux/amd64 \
  --mount "type=bind,source=$PWD,target=/workspace" --workdir /workspace \
  "$(cat Tools/psp-image.txt)" make -C tests/graphics

dist/headless-venv/bin/python Tools/headless.py \
  --emulator /caminho/PPSSPPHeadless --tag projection-jit \
  --package tests/graphics --backend null --script tests/headless/projection.json
# Repetir com outra tag e --cpu interpreter para o núcleo PSP interpretado.
```

O roteiro exige `FROGN64_PROJECTION_TESTS_PASS` no log do programa MIPS.
Não usa ROM e verifica a mesma implementação C++ da build do emulador.

## Código MIPS e WAV sintético

Compile o teste com a mesma imagem fixada da build PSP:

```sh
docker run --rm --platform linux/amd64 \
  --mount "type=bind,source=$PWD,target=/workspace" --workdir /workspace \
  "$(cat Tools/psp-image.txt)" make -C tests/audio
python3 Tools/audio_smoke.py --emulator /caminho/PPSSPPHeadless
```

O workflow PSP também compila esse PRX. Ele executa os casos de áudio do host,
exceto a thread de stress, e emite um tom de 440 Hz, reamostrado de 32 kHz para
44,1 kHz, pela API `sceAudioOutputBlocking`. Dois buffers alternados respeitam o
ciclo de reprodução da API. O script exige o marcador de sucesso do programa
MIPS e analisa o WAV: presença de sinal, limite de amplitude, saltos entre
amostras e canais iguais. Não usa ROM de jogo.

## FrogN64 e controles automatizados

Use Python 3.11 ou posterior e uma build de desenvolvimento do PPSSPPHeadless que tenha
`--debugger`, `cpu.runUntilTime`, `--memstick`, `--timeout-emulated`,
`--dump-audio` e `--screenshot-save`. O PPSSPP SDL instalado não substitui esse
executável. A [documentação de desenvolvimento do PPSSPP](https://www.ppsspp.org/docs/development/tips-and-tricks/)
e o [protocolo WebSocket](https://www.ppsspp.org/docs/reference/websocket-api/)
são as referências; a versão efetivamente usada fica registrada por hash.

```sh
./scripts/psp.sh
python3 -m venv dist/headless-venv
dist/headless-venv/bin/python -m pip install -r Tools/headless-requirements.txt

export PPSSPP_HEADLESS=/caminho/PPSSPPHeadless
dist/headless-venv/bin/python Tools/headless.py \
  --tag menu --script tests/headless/menu.json

dist/headless-venv/bin/python Tools/headless.py \
  --tag lego-boot --rom '/caminho/LEGO Racers (U) [!].v64' --seconds 20
```

`--emulator` também seleciona o executável. Neste Mac, se nenhum caminho for
informado, o script encontra a build no checkout irmão `pessoal-nfsu2-psp`.
Ela é somente lida e copiada para `dist/tools/<hash>/`; o checkout irmão não é
alterado. `--assets` indica os recursos do PPSSPP quando não forem encontrados
junto do executável/fonte. No macOS, `--moltenvk` indica `libMoltenVK.dylib`;
o padrão usa a biblioteca do PPSSPP SDL já instalado.

Cada execução cria `dist/headless/<tag>/` e recusa reaproveitar uma tag. Congela
o EBOOT, o emulador e a ROM indicada, registra SHA-256 e usa Memory Stick, NAND,
preferências e saves próprios. Copia as fontes de `assets/flash0` para a NAND
da sessão. Não altera configurações globais nem reinicia instâncias de outros
projetos. Nenhum comando PSPLink é chamado.

Com `--rom`, a ROM é validada pelo cabeçalho, copiada para o navegador da cópia
de teste e selecionada por dois comandos X. O perfil inicial reproduz as
principais opções usadas no teste físico do LEGO Racers: HLE, dynarec e suas
otimizações habilitadas, frameskip zero. Audio Sync é padrão; `--audio disabled`
permite a comparação sem áudio. `--no-hle`, `--no-n64-optimizations` e
`--n64-cpu interpreter` permitem isolar o núcleo N64; `--cpu interpreter` troca
o núcleo PSP do PPSSPP. São opções distintas. Um roteiro pode terminar sem
chegar à cena esperada: confira as capturas, especialmente ao trocar o núcleo.

O roteiro JSON usa `wait` (segundos emulados), `press` (botão PSP, com duração e
tempo após soltar), `analog` (x/y de −1 a 1; y positivo aponta para cima),
`capture`, `compare` e `expect_log` (exige um texto no log do programa). Os comandos são aplicados com a CPU
parada e executados até um tempo emulado definido, usando o PID confirmado do
processo lançado e sua porta exclusiva. `tests/headless/menu.json` abre Sobre,
volta e compara os pixels com o menu inicial. O recorte remove o relógio.
O teste falha se a diferença superar `max_mse`. `--cpu interpreter` (também
no audio_smoke.py) permite confrontar a execução com o JIT do PPSSPP.
O clock PSP fica explicitamente em 333 MHz. A execução acima de realtime
resulta de rodar o host sem limitação de velocidade; mantém esse orçamento
de instruções por vblank.

Capturas, WAV, log, respostas do debugger e `result.json` ficam juntos. O
resultado distingue plano concluído, erro e código de saída: não basta o
PPSSPP retornar zero. O limite emulado é o encerramento normal após o roteiro;
o limite de parede é a proteção para travamentos. O processo encerra normalmente
para finalizar o cabeçalho WAV. Um watchdog no host impõe o orçamento de parede
mesmo se o guest deixar de alcançar o loop que verifica o timeout. Em falha, somente o processo desta execução é
encerrado. Vulkan é o padrão; `--backend software` permite comparação de
rasterização. O backend `null` só aceita roteiros sem capturas.

## Medições de 17/10/2026

Emulador local: PPSSPP `f29d72cea8`, executável SHA-256
`bc658a89e1c89f029dcdfdd2e88443f74c4a02483c9afabb00047929541e87a1`,
com adaptações locais no entry point headless. O script congela o binário para
não depender de recompilações de outro projeto.

- Seis testes nativos ASan/UBSan: passaram, cerca de 0,76 s no total.
- Stress de concorrência com TSan: passou, cerca de 0,87 s.
- Testes MIPS com alias uncached e tom sintético: passaram, cerca de 0,19 s
  de parede no JIT e 2,48 s no interpretador PSP. O WAV teve
  pico 11996 e maior diferença entre amostras adjacentes 752, com canais iguais.
- Roteiro de menus com capturas e comparação: passou. A imagem após voltar foi
  idêntica fora da barra do relógio (MSE 0). 6,2 s emulados em 2,32 s de parede,
  incluindo boot, automação e comparação: aproximadamente 2,68× realtime.
- Sem a etapa de comparação, o mesmo roteiro mediu cerca de 5,63× realtime.

- LEGO, menu com torso ausente: 87,57 s emulados em 7,07 s de parede,
  aproximadamente 12,39× realtime na base `9a715c08`.
- LEGO, roteiro final de largada: 123,21 s emulados em 13,10 s de parede,
  aproximadamente 9,41× realtime com a correção de áudio.

Essas velocidades medem tempo PSP emulado por tempo de parede no Mac, incluindo
boot e automação. Não são FPS do núcleo N64 nem previsão de desempenho no PSP. A captura, IPC e a comparação têm custo de parede.
Evidências locais: `dist/tests/`, `dist/headless/menu-ws-3/` e
`dist/headless/audio-mips-uncached/` e `dist/headless/final-lego-grid/`.

## Limites para investigar o LEGO Racers

A ROM local fornecida pelo usuário tem SHA-256
`211983265868158281d16e5ba54d12f478da5ced47d34af35fb555b9a7ad51ba`, ID
Daedalus `ea406a09100abe8a-45`. Não é incluída no repositório nem no pacote.
`tests/headless/lego-menu.json` alcança o menu usado para comparar o torso.
`tests/headless/lego-racers.json` captura Joan, Turbo Charger, Scooter e Roboracer.
`tests/headless/lego-grid.json` passa pela seleção de pista/piloto e captura a
largada. As correções e imagens antes/depois estão em [LEGO-RACERS.md](LEGO-RACERS.md).
O desaparecimento completo de karts relatado no aparelho ainda exige uma cena
específica, além desta largada.

```sh
dist/headless-venv/bin/python Tools/headless.py --tag lego-menu \
  --rom '/caminho/LEGO Racers (U) [!].z64' --script tests/headless/lego-menu.json
dist/headless-venv/bin/python Tools/headless.py --tag lego-grid \
  --rom '/caminho/LEGO Racers (U) [!].z64' --script tests/headless/lego-grid.json
```

A fila corrigida foi executada também com o jogo até a corrida. Isso elimina
os defeitos demonstrados da fila; o WAV do jogo ainda não prova a eliminação
dos estalos percebidos no aparelho. O microcódigo foi identificado como
`F3DEX.NoN fifo 2.08` (hash `2484afea`), sem opcode desconhecido até a largada.

O torso foi recuperado preservando as posições projetadas dos vértices entre
mudanças de matriz. A inversão indevida de Y no recorte PSP também foi corrigida,
recuperando as pernas e a plataforma nas prévias. A build normal, com diagnóstico
OFF, é exercitada pelos mesmos roteiros. Com ambas as correções, o roteiro normal de pilotos mediu 106,39 s emulados
em 8,69 s de parede (12,25×) e a largada 123,21 s em 11,74 s (10,50×).
Isso mede a automação, não FPS N64. Evidências locais:
`dist/headless/graphics-final-racers/` e `dist/headless/graphics-final-grid/`.

A comparação com o interpretador N64 não alcançou a cena e não permite conclusão
visual. O interpretador PSP validou os testes MIPS de áudio e projeção.

O plugin Async do Daedalus executa código próprio diretamente no Media Engine.
Esse caminho não é validado pelo teste Sync nem pelo tom. O boot headless também
registra funções de firmware não implementadas, incluindo ajuste de VRAM;
o comportamento gráfico pode divergir do aparelho. Não confundir tempo de PSP
emulado com tempo/FPS do núcleo N64 nem com custo real em 333 MHz.

Um savestate completo do PPSSPP inclui o código do FrogN64. Carregá-lo por cima
de outra build restaura o código antigo e invalida o teste A/B. Para comparar
builds, use o mesmo roteiro desde o boot, ou um estado N64 carregado pelo próprio
FrogN64 com a ROM correspondente. O runner atual sempre começa pelo boot.


## Diagnóstico gráfico separado

`FROGN64_RENDER_DIAGNOSTICS` fica OFF no pacote normal. Compile em outro diretório:

```sh
# Dentro da mesma imagem PSPSDK usada por scripts/psp.sh:
psp-cmake -S . -B dist/build-render-diag -DCMAKE_BUILD_TYPE=Release \
  -DFROGN64_RENDER_DIAGNOSTICS=ON
cmake --build dist/build-render-diag --parallel 4
cmake --install dist/build-render-diag --prefix "$PWD/dist/render-diag/PSP/GAME"
```

Use `--package dist/render-diag/PSP/GAME/FrogN64 --render-debug MASCARA` no
runner. O EBOOT confirma a máscara no log, só para o cartucho LEGO `LG`.
As flags são: 1 faces CPU; 2 rejeição trivial; 4 clipping CPU; 8 alfa dos
vértices; 16 teste alfa GE; 32 blend; 64 profundidade; 128 descarte de display
list; 256 branco sem textura; 512 clipping GE; 1024 faces GE; 2048 fog GE;
4096 desenho direto sem combinador; 8192 desliga a correção de projeções;
16384 restaura a inversão incorreta de Y do recorte PSP. Zero usa as correções.
As flags podem ser somadas (máximo 32767).
São intervenções para localizar o defeito, não opções de compatibilidade.
O log também identifica microcódigo e o primeiro uso de cada comando gráfico.

A build PPSSPP usada deriva do commit oficial
`f29d72cea84e4502f883a4de222728eb5547e312`, mais o patch local preservado em
`Tools/ppsspp-headless-local.patch`. O patch registra as alterações herdadas
para permitir reconstrução; os argumentos usados pelo runner não dependem
das variáveis extras de gravação desse patch, que são limpas no processo filho.
