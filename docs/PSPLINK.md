# Testar este fork no PSP real

O controlador `Tools/psp_run.py` usa o servidor `usbhostfs_pc` já aberto neste
Mac e a trava compartilhada com o projeto `pessoal-nfsu2-psp`. Por padrão,
reutiliza o SDK e o controlador de trava desse checkout irmão. Outro local
pode ser indicado por `--shared-root`. Não usa o loader nem os pokes de memória
específicos do NFSU2 no FrogN64.

```sh
python3 Tools/psp_run.py inspect
python3 Tools/psp_run.py start \
  --host-root ../pessoal-nfsu2-psp/reports/host0-root \
  --tag meu-teste
python3 Tools/psp_run.py shot
python3 Tools/psp_run.py probe
python3 Tools/psp_run.py stop
```

`--host-root` precisa ser a raiz efetivamente servida pelo processo USBHostFS
existente. O script cria somente um link `pspn64-<tag>` nessa raiz. Cada teste
ganha uma pasta própria em `dist/psplink/<tag>/`, contendo uma cópia fixa do
pacote, PRX e ELF. Ele confere se o PRX é o mesmo incluído no EBOOT e registra
a revisão gravada em `BUILD-INFO.txt`, além do hash do PRX. Não recompile por
cima de uma sessão em andamento; a cópia da sessão permanece independente.

Para carregar diretamente uma ROM que já está no cartão:

```sh
python3 Tools/psp_run.py start \
  --host-root ../pessoal-nfsu2-psp/reports/host0-root \
  --tag mario64 \
  --rom 'ms0:/PSP/GAME/DaedalusX64 1.1.9a/Roms/Super Mario 64 (U) [!].v64'
```

O caminho é apenas o exemplo confirmado neste aparelho; use o caminho real
da sua ROM. `inspect --list-path <pasta>` lista um diretório quando o aparelho
está livre; `probe --list-path <pasta>` faz isso na sessão que este projeto
possui. A build padrão mantém o áudio desabilitado. Os screenshots usam o
comando `scrshot` do PSPLink e são convertidos para PNG pelo `sips` do macOS.

`start` deixa o emulador rodando e mantém a trava mesmo depois que o comando
termina. Ao concluir, use **stop**: ele envia um único reset do PSPLink e
aguarda a reconexão, sem repetir resets. Antes de remover o link da sessão,
restaura o diretório do shell para `host0:/`; o pspsh executa `cd .` ao conectar
e não pode usar uma pasta já removida. `recover-directory` permite restaurar a
raiz quando o diretório ainda existe. Se o link tiver sido removido cedo,
recrie o mesmo link antes de executar essa recuperação.

Falha depois da tentativa de carregar o PRX mantém a trava até uma parada
explícita. Não use comandos de outra sessão enquanto ela possuir o aparelho.
Os logs de início são capturados por oito segundos; `probe` e `shot` são
diagnósticos pontuais e não constituem benchmark de FPS.

Para testar o navegador do FrogN64 com uma pasta de ROMs existente, acrescente
`--rom-directory 'ms0:/caminho/das/Roms'` ao `start`, sem `--rom`. Essa opção
escreve somente a preferência da cópia de teste; não copia nem altera ROMs.

## Volume sem usar os botões físicos

O build também compila `Tools/psp-volume/volume.prx`, um utilitário separado do
EBOOT. Com uma sessão deste projeto ativa, **deixe o FrogN64 na pausa primeiro**:

```sh
python3 Tools/psp_run.py volume --level 6
```

Os níveis vão de 0 a 30. O utilitário usa `sceImposeGetParam/SetParam` do
firmware, lê o valor de volta, desativa mute e grava um relatório na pasta da
sessão. O controlador só informa sucesso após conferir o relatório, e para e
descarrega o utilitário. Não altera a memória do jogo.

No teste de 17/10/2026, o firmware confirmou volume anterior 30, novo 6 e mute
0. O usuário ouviu o jogo após ativar Audio nas configurações por ROM, mas
relatou estalos em LEGO Racers tanto em Sync quanto em Async.

Durante a emulação de LEGO Racers, o shell ficou sem responder e retomou na
pausa; a primeira tentativa de volume só confirmou depois disso. Um timeout
não prova cancelamento. Entre na pausa e confira o resultado antes de repetir
um comando de carregamento. O PRX confirmado tem SHA-256
`364c3b535b772ee708c7ce4c8c70c1d4f958042357de0485b2be593041319532`.
