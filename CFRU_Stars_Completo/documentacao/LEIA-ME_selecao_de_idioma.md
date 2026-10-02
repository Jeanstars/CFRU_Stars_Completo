# Selecao de idioma depois da tela de titulo

Tela com 3 opcoes (cima/baixo + A): PORTUGUES (BR), ENGLISH (USA), ESPANOL (ESP).
Liga so UMA destas flags (as mesmas dos seus scripts XSE):

    0x260 = BR      0x261 = USA      0x262 = ESP

Nos scripts continua igual:
    checkflag 0x260 / if 0x1 goto @Script_BR  (etc.)

## Quando a tela aparece
- Save novo/vazio: aparece.
- Save que ja tem uma das 3 flags: pula (vale a flag).
- Save antigo sem nenhuma flag: aparece uma vez.

## Como funciona
- `src/scripting.c` (SetTitleScreenScene_Cry): no fim da tela de titulo chama a tela de idioma
  em vez do menu principal, se precisar. Depois da escolha segue para o menu principal.
- `src/language_select.c` (novo) + `strings/language_select.string` (novo): a tela e a logica.
- O "Novo Jogo" zera as flags. Por isso a escolha tambem fica num byte livre do SaveBlock2
  (filler_28[0x6F]) e `src/character_swap.c` (WarpToPlayersRoom_Custom) religa a flag certa
  no fim do NewGameInitData. Exige NEW_GAME_START_LOCATION ligado (esta no seu config.h).
- `src/overworld.c`: na rota do Nuzlocke (ClearSaveData) o idioma e preservado.
- Sem alteracao no arquivo `hooks`.

## Observacoes
- Se um script seu mudar as flags depois, vale a flag (o byte so serve para restaurar no Novo Jogo).
- Para trocar o idioma depois do jogo comecado: mude as flags por script (ligue 1, desligue as outras).
  Uma opcao no menu Options pode ser adicionada depois.
- Neste build o .bss fica na ROM (nao grava), por isso o estado da tela fica na task.
- Compilou sem erro (language_select.c, scripting.c, character_swap.c, overworld.c) e os textos
  foram convertidos pela ferramenta do projeto. NAO foi testado no emulador.
  Comece um save novo para testar.

## Visual da tela (versao nova)
A tela de idioma agora tem: fundo em degrade azul, um cartao por idioma com a **bandeira** (Brasil, EUA, Espanha),
cartao selecionado em azul-claro que **pulsa**, uma setinha que desliza ate o idioma escolhido e balanca,
e entrada/saida com fade.

- `src/language_select.c`: a logica e a tela (3 camadas de fundo: textos, cartoes/bandeiras, degrade; 1 sprite: a seta).
- `src/language_select_gfx.c`: GERADO por `ferramentas/tela_idioma/gerar_tela_idioma.py` (nao edite a mao).
- Para mudar cores ou desenhos: edite a secao CORES (ou o desenho) em `gerar_tela_idioma.py`, rode
  `python gerar_tela_idioma.py` (ele regrava `projeto/src/language_select_gfx.c` e uma previa `previa_tela_idioma.png`)
  e compile de novo.
- Todo o desenho e original (feito em pixel art para este projeto), sem gráficos copiados do jogo.
- Limites do GBA respeitados: 16 cores por paleta; o fundo usa 10 paletas para o degrade, 3 para os cartoes
  (a bandeira de cada idioma fica na paleta do seu cartao) e 1 para os textos.
- Verificado rodando a ROM num emulador (mGBA): cursor nos 3 idiomas e volta ao primeiro, pulsacao do cartao,
  escolha de cada idioma levando a introducao no idioma certo (PT, EN, ES).
