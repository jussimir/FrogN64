# Desenvolvimento para PSP

## Base do projeto

- Fork: https://github.com/jussimir/pessoal-pspn64emulator
- Original (`upstream`): https://github.com/DaedalusX64/daedalus
- Base inicial: `580c0c409358db332149fc2903c610449434f8fc`, de 10/10/2026.
- Build oficial dessa base: https://github.com/DaedalusX64/daedalus/actions/runs/38029124147
- Branch de desenvolvimento: `psp-development`.
- `master` acompanha o código original; nossas mudanças ficam na branch de desenvolvimento.

Um fork é uma cópia do repositório na sua conta, com histórico e ligação com o
original. Alterações e pushes neste fork não alteram o projeto original.

Esta primeira base mantém o núcleo de emulação original. Ela prepara compilação,
empacotamento e testes para futuras melhorias; não promete ganho de FPS.

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

O pacote sai em `dist/DaedalusX64-PSP-<commit>.zip`. Compilações com alterações
locais recebem o sufixo `-modified`. O arquivo `BUILD-INFO.txt` registra commit,
estado da árvore, data UTC e toolchain. O empacotamento copia os recursos do
projeto para uma pasta limpa, sem incluir ROMs ou saves locais.

O workflow **PSP** compila cada push/PR para `psp-development`. O ZIP fica nos
artefatos da execução em Actions. O workflow multiplataforma herdado está
disponível apenas para execução manual.

## Instalar e testar

1. Faça backup da instalação anterior e dos seus saves.
2. Extraia o ZIP na raiz do Memory Stick, gerando `PSP/GAME/DaedalusX64/`.
3. Coloque suas ROMs em `PSP/GAME/DaedalusX64/Roms/`.
4. Inicie o DaedalusX64 pelo menu de jogos do PSP com suporte a homebrew.

Para conferir a inicialização no PPSSPP, abra
`dist/package/PSP/GAME/DaedalusX64/EBOOT.PBP`. Isso verifica somente o que foi
observado no emulador; desempenho, Media Engine e compatibilidade precisam de
teste no PSP físico.

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
