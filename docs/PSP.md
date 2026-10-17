# Desenvolvimento FrogN64 para PSP

## Base do projeto

- Fork: https://github.com/jussimir/FrogN64
- Original (`upstream`): https://github.com/DaedalusX64/daedalus
- Base inicial: `580c0c409358db332149fc2903c610449434f8fc`, de 10/10/2026.
- Build oficial dessa base: https://github.com/DaedalusX64/daedalus/actions/runs/38029124147
- Branch de desenvolvimento: `psp-development`.
- `master` acompanha o código original; nossas mudanças ficam na branch de desenvolvimento.

Um fork é uma cópia do repositório na sua conta, com histórico e ligação com o
original. Alterações e pushes neste fork não alteram o projeto original.

O FrogN64 usa o núcleo de emulação do DaedalusX64, com correções na fila de
áudio verificadas em testes sintéticos e correções no renderizador PSP para
projeções de LEGO Racers e recorte vertical. As [comparações](LEGO-RACERS.md)
foram feitas no PPSSPP, ainda sem confirmação desta build no aparelho.
O [ambiente headless](HEADLESS.md)
permite regressões locais antes do teste físico; não promete ganho de FPS.

O empacotamento define explicitamente `MEMSIZE=1` para solicitar RAM ampliada
nos modelos compatíveis. O padrão `MEMSIZE=2` da toolchain não ativa essa RAM no
PPSSPP 1.20.4, e a base original encerrou com `std::bad_alloc` na inicialização.
O foco deste pacote é o PSP; o comportamento no modo PSP do PS Vita não foi
validado.

## Compilar

Com Docker Desktop iniciado, na raiz do repositório:

```sh
./scripts/psp.sh
```

A imagem PSPSDK está fixada por digest em `Tools/psp-image.txt`. No Mac Apple
Silicon, o Docker executa a imagem Linux amd64. O compilador produz código MIPS
para o PSP. O script usa quatro jobs; ajuste com `PSP_BUILD_JOBS=2` se necessário.

Com PSPSDK nativo e suas bibliotecas já instalados:

```sh
./scripts/psp.sh --native
```

O pacote sai em `dist/FrogN64-PSP-<commit>.zip`. Compilações com alterações
locais recebem o sufixo `-modified`. O arquivo `BUILD-INFO.txt` registra commit,
estado da árvore, data UTC e toolchain. O empacotamento copia os recursos do
projeto para uma pasta limpa, sem incluir ROMs ou saves locais.

O workflow **PSP** compila cada push/PR para `psp-development`. O ZIP fica nos
artefatos da execução em Actions. O workflow multiplataforma herdado está
disponível apenas para execução manual.

## Instalar e testar

1. Faça backup da instalação anterior e dos seus saves.
2. Extraia o ZIP na raiz do Memory Stick, gerando `PSP/GAME/FrogN64/`.
3. Coloque suas ROMs em `PSP/GAME/FrogN64/Roms/`.
4. Inicie o FrogN64 pelo menu de jogos do PSP com suporte a homebrew.

Para conferir a inicialização no PPSSPP, instale a pasta `FrogN64` dentro de
`PSP/GAME/` do Memory Stick virtual e abra seu `EBOOT.PBP`. Isso mantém os
caminhos de configurações e saves em `ms0:`. No PPSSPP SDL deste Mac, a pasta é
`~/.config/ppsspp/PSP/GAME/`. Desempenho, Media Engine e compatibilidade precisam
de teste no PSP físico.

### Validação inicial em 17/10/2026

- Build Release local: PSPSDK em Docker, GCC 15.2.0, dynarec MIPS, VFPU e Media Engine habilitados.
- Empacotamento: ZIP íntegro, EBOOT em formato PBP e três plugins MIPS presentes.
- PPSSPP SDL 1.20.4: com `MEMSIZE=2`, encerrou com `std::bad_alloc`; alterando
  apenas a flag para `1` na cópia de teste, passou da inicialização e começou a
  renderizar, conforme os logs de framebuffer. Sem ROMs carregadas.
- O PPSSPP também registrou funções de firmware não implementadas e falhas na
  abertura das fontes `flash0:/font/*.pgf`. A inspeção visual automática não
  estava disponível; não foi confirmada a apresentação correta da interface.
- Nessa etapa, PSP físico, jogos, áudio e saves ainda não tinham sido testados.

### Teste físico via PSPLink em 17/10/2026

O PRX da build `266621c733171022bff9ee264b1ab5e2b7128787` foi carregado no
aparelho do usuário por `host0:`, com firmware base **6.61**, PSPLink **3.2.1**,
CPU **333 MHz** e barramento **166 MHz**. A partição de usuário informou
**57.671.680 bytes (55 MiB)**, antes de carregar o emulador. Esta é uma sessão
com RAM ampliada; não comprova compatibilidade com PSP-1000.

- Menu do Daedalus: imagem e fontes confirmadas em screenshot do PSP.
- Super Mario 64, ROM de 8 MiB já existente no cartão: abertura e tela do rosto
  do Mario confirmadas em screenshot, com geometria 3D e texturas.
- `exprint`: **No exception occurred** após iniciar o jogo.
- Plugin `mediaengine.prx` carregou; áudio permaneceu desabilitado pelo padrão
  do upstream. Isso não comprova reprodução de áudio.
- Jussimir confirmou no aparelho: **controles e imagem funcionando**, após o
  pedido para pressionar Start e entrar no jogo. É uma validação manual do
  usuário, adicional aos screenshots e logs.
- Cadência/FPS, áudio e persistência de saves ainda precisam de validação.
  Nenhum ganho de desempenho foi medido.

Evidências locais em `dist/psplink/266621c-menu-20261017/` e
`dist/psplink/266621c-sm64-20261017/`, com PRX/ELF fixos, hashes, logs e PNGs.
O PRX tem SHA-256
`94f64a934340317b21c70da6b61f3f6688c4d4128db1228645fb9b042eaf18c0`.
Configurações, caches e saves do teste usam a pasta isolada no Mac; a ROM é lida
do cartão, sem sobrescrever a instalação antiga.

Ao concluir o teste, um único reset encerrou o emulador, o PSPLink reconectou,
o diretório do shell voltou a `host0:/` e a trava compartilhada foi liberada.

O controlador de acesso ao hardware está documentado em [PSPLINK.md](PSPLINK.md).

Antes de otimizar, registre para cada teste: modelo do PSP, firmware, revisão da
build, região/revisão do jogo, trecho testado, configurações, FPS, falhas gráficas,
áudio e funcionamento de saves. Compare a mesma cena e configuração com a build
de referência. Não há ROMs incluídas neste repositório.

## Trabalhar no fork

```sh
git switch psp-development
# editar os arquivos
git add <arquivos>
git commit -m "Jussimir: Descrever a mudança"
git push origin psp-development
```

Neste Mac, use a conta `jussimir` e a identidade local do projeto. Não troque a
conta global do GitHub para sincronizar o fork.

Para incorporar novas mudanças do original com a árvore de trabalho limpa:

```sh
git fetch upstream master
git switch psp-development
git merge upstream/master
./scripts/psp.sh
# testar antes de enviar a atualização
git push origin psp-development
```

A cópia local inicial baixa somente a revisão atual para evitar transferir todo
o histórico antigo. O fork no GitHub mantém o histórico completo. Se precisar
consultá-lo ou se o merge não encontrar um ancestral comum:

```sh
git fetch --unshallow upstream master
```

Os créditos e a licença GPL do projeto original foram preservados em `README.md`
e `copying.txt`; o pacote também inclui uma cópia da licença.
