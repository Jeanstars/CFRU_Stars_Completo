# Auditoria das alterações (resumo)

Comparei a ROM compilada do **seu projeto original** com a ROM do pacote, e rodei a ROM num emulador (mGBA).

- ROM original (antes de 0x1000000): das 1042 palavras que diferem, 1017 são endereços do código CFRU que se deslocaram
  (normal ao acrescentar código) e 25 são os patches planejados (14 chamadas do Professor, o desvio, o gancho de impressão).
  Nenhuma alteração inesperada. Nenhum outro patch do CFRU/seu projeto usa esses endereços.
- 0xFC: 76.800 chamadas comparadas com a função original (300 cenários x 256 números): 0 divergências nos outros números;
  0xFC correto em todos os cenários.
- Novo Jogo nos 3 idiomas, no emulador: a introdução sai no idioma certo; as flags 0x260/0x261/0x262 e o byte do idioma
  continuam corretos depois do Novo Jogo.
- Salvar e reiniciar: a tela de idioma é pulada, vai direto para CONTINUE, e o idioma se mantém.
- Build: os mesmos 112 avisos do seu projeto original; nenhum aviso novo.
- Não testado: rota do Nuzlocke (preservar o idioma no reinício), áudio, e em console real.
