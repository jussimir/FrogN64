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

## Diagnóstico inicial (antes dos testes headless)

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

## Sessão física encerrada

A sessão física foi encerrada e a trava liberada a pedido do usuário para
outro projeto. Evidências locais: `dist/psplink/frogn64-ui-v1-20261017/`.

## Avanço sintético, sem acesso ao PSP

Os testes do host reproduziram a amostra inicial não escrita, a perda da
última amostra e o bloqueio com um bloco maior que a fila. A correção faz a
fila publicar os dados antes de esperar espaço e preserva a fase de
reamostragem entre pacotes. Os testes estão em `tests/audio/` e foram
executados também como código MIPS no PPSSPPHeadless, com tom WAV sintético.

O PRX MIPS passou no JIT e interpretador PSP do PPSSPP; o tom WAV passou na
análise de amplitude, continuidade e canais. O host passou os seis casos com
ASan/UBSan e o stress com TSan. Isso confirma esses defeitos da fila corrigidos,
não a eliminação de todos os estalos do LEGO Racers.

Com a ROM local fornecida pelo usuário, o torso ausente foi reproduzido no menu
e na seleção de piloto. A automação também alcança a largada e salva uma série
de quadros dos adversários, alguns com geometria parcial. Ainda não foi isolada
uma cena de desaparecimento completo de kart. A ROM tem o mesmo ID do teste
físico; seu hash e os roteiros ficam documentados em [HEADLESS.md](HEADLESS.md).

O torso continuou ausente nas comparações sem HLE, sem otimizações de memória,
loop/doubles e sem VFPU, além dos testes isolados de clipping/alpha/blend/depth.
Esses experimentos não são correções e ficam fora da build normal. A comparação
com o interpretador N64 não alcançou a mesma cena; não permite conclusão visual.

Nenhum acesso ao aparelho foi feito nesta etapa. O caminho Sync foi exercitado
com o EBOOT até a corrida, acima de realtime no Mac; Async/Media Engine e a
melhora perceptível no áudio ainda precisam da confirmação física posterior.
