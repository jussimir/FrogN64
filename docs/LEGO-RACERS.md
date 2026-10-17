# LEGO Racers — primeiro teste no FrogN64

## Estado confirmado em 17/10/2026

Build da interface: `dc8ddb828dc62b9d7642b2949820a02260756967`.
ROM no cartão do usuário: LEGO Racers (U) [!], ID `ea406a09100abe8a-45`.
Teste no PSP via PSPLink, CPU 333 MHz, sem medição de FPS.

O usuário confirmou navegador, abertura do jogo e pausa. Depois de ativar o
áudio por ROM, relatou:

- Estalos frequentes, tanto em Synchronous quanto em Asynchronous.
- Modelos 3D dos karts adversários ausentes.
- Torsos de personagens LEGO ausentes.

O volume de firmware estava em 30/30, sem mute. Foi ajustado e lido de volta
como 6/30. Portanto, a ausência inicial de som era compatível com Audio
Disabled; os estalos continuam sendo um problema de emulação a investigar.

A configuração registrada no teste tinha audio Async, frameskip 0, HLE e
recompilação dinâmica habilitados, além das otimizações de memória, loop e
doubles. Ainda não houve comparação controlada com essas otimizações
individualmente desabilitadas. Não foi capturada a cena dos modelos ausentes.

## Pistas no código, ainda sem correção validada

O núcleo de áudio e gráficos não foi modificado pela adaptação da interface.

Em `Source/HLEAudio/AudioBuffer.cpp`, o produtor avança o ponteiro antes de
escrever a amostra; o consumidor começa lendo no ponteiro atual. O produtor
publica seu ponteiro somente no fim do bloco e espera espaço em um loop sem
yield. Isso precisa de testes de ordem das amostras, wrap e saturação da fila,
além de contadores de underrun no PSP. Não foi demonstrado que essa seja a
causa dos estalos específicos do LEGO Racers.

Em `Source/HLEGraphics/BaseRenderer.cpp`, existe um comentário histórico sobre
problemas de clipping em LEGO Racers/Rocket Robot. É uma pista para examinar
as rotinas de recorte e descarte de triângulos, não uma confirmação da causa
dos karts e torsos ausentes. É necessário comparar a mesma cena antes/depois.

## Próximo teste

Primeiro registrar a cena defeituosa e FPS, depois comparar mudanças isoladas.
Para o áudio, verificar a fila em testes no host antes de alterar o plugin;
para gráficos, verificar culling/clipping e estado de renderização por jogo.
Não há correção de compatibilidade reivindicada nesta versão.

A sessão física foi encerrada e a trava liberada a pedido do usuário para
outro projeto. Evidências locais: `dist/psplink/frogn64-ui-v1-20261017/`.
