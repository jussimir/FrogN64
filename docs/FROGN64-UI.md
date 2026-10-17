# Interface FrogN64

A primeira interface PSP usa a organização e a paleta do FrogGBA, referência
`9d75721fe8e3df23db4148f49e92df2bafe6dd1a`, com uma implementação C++ conectada
às funções existentes do DaedalusX64. O núcleo GBA e suas fontes bitmap não
são incorporados; o PSP fornece as fontes de firmware usadas pelo Daedalus.

## Controles

| Tela | Comando | Ação |
| --- | --- | --- |
| Menu FrogN64, navegador e recentes | Direcional / analógico | Selecionar item |
| Menu FrogN64, navegador e recentes | X / Start | Abrir ou confirmar |
| Menu principal | L | Abrir navegador |
| Navegador | Esquerda / direita | Alternar arquivos e pastas |
| Navegador | L / R | Avançar uma página para cima / baixo |
| Navegador | Quadrado | Abrir a raiz `ms0:/` |
| Navegador | Select | Abrir `Roms/` do FrogN64 |
| Navegador | Círculo | Subir uma pasta; na raiz, voltar ao menu |
| Navegador / recentes | Triângulo | Configurar a ROM selecionada |
| Jogo | Select | Abrir pausa |
| Pausa | Círculo | Retomar jogo |
| Pausa | R | Abrir carregamento de estado |
| Configurações | Esquerda / direita | Alterar opção |
| Submenus | Círculo | Voltar |

São listados `.z64`, `.n64`, `.v64`, `.rom`, `.bin`, `.usa`, `.jap` e `.pal`,
sem diferença entre maiúsculas e minúsculas. ZIP só aparece se o suporte a ROMs
comprimidas estiver habilitado na compilação (desabilitado nesta build).

`frogn64-directory.txt` guarda a última pasta. `frogn64-recent.txt` guarda até
dez caminhos, adicionados somente depois de abrir o jogo, sem duplicatas.
Esses arquivos ficam na pasta do aplicativo. Removê-los limpa as preferências
de navegação, sem apagar ROMs, configurações de emulação ou saves.

O menu de pausa oferece retorno ao jogo, salvar/carregar estado, configurações
por jogo, opções avançadas, cheats, configurações gerais, screenshot e retorno
ao menu principal. As telas detalhadas de configurações e slots continuam
usando os componentes do DaedalusX64, com as cores do FrogN64.

## Créditos e recursos visuais

FrogGBA: tzubertowski (Prosty) e colaboradores, baseado em gpSP/TempGBA de
Exophase, takka e colaboradores. GPL v2 ou posterior, compatível com esta base.
A licença e a referência exata acompanham o pacote em `licenses/`.

O ícone FrogN64 é um desenho original feito com retângulos. Os PNGs do EBOOT
podem ser regenerados no macOS com `swift Tools/branding/render.swift`.

## Validação da primeira interface — 17/10/2026

- Compilação PSP Release, PSPSDK fixado, sem novos avisos do compilador.
- EBOOT identificado como FrogN64 e PRX conferido byte a byte com o empacotado.
- ZIP íntegro, três plugins e avisos de licença presentes, sem ROMs/savestates.
- Funções de extensão/persistência verificadas isoladamente no host: extensões
  em maiúsculas, caminhos com espaços, arquivo ausente, substituição, CRLF e
  limite de dez entradas.
- Menu renderizado no PSP físico via PSPLink e capturado em
  [frogn64-menu.png](images/frogn64-menu.png).
- Uma abertura pela interface alcançou a introdução LEGO no aparelho.
- O usuário confirmou navegador, abertura do jogo e pausa: “a princípio tudo
  funcionando”. Áudio e savestates ainda não têm confirmação de uso.

As evidências locais estão em `dist/psplink/frogn64-ui-v1-20261017/`.
O teste usa uma cópia fixa do PRX e registra seu SHA-256 em `session.json`.
