# Character Swap System (Blue Flute) para CFRU-expansion — v3.5

O jogador troca entre o personagem masculino e o feminino usando a **Blue Flute**. Cada personagem tem os próprios Pokémon, itens, flags, vars e local. Ao trocar, o jogo volta para o mapa e a posição onde o outro personagem estava.

Feito e testado (compilação) sobre o commit `7e8facf` do Shiny-Miner/CFRU-expansion.

## Instalação

1. Copie todas as pastas deste zip para a raiz do seu projeto CFRU e substitua os arquivos.
2. Compile normalmente (`make.py` / `a_makepy.bat`).
3. **Comece um save NOVO.** A primeira troca usa as boxes 11 a 25, e o que estiver nelas será apagado.
4. Dê o item ao jogador em algum script: `giveitem ITEM_BLUE_FLUTE 1 MSG_OBTAIN`.

**Atualizando da v1:** basta substituir os arquivos. Saves da v1 continuam funcionando, porque os dados novos foram adicionados no final do armazenamento. Na primeira troca depois de atualizar, o personagem que estava guardado volta sem roamers e com as flags diárias zeradas (uma vez só).

Se o seu projeto já tem outras modificações nesses arquivos, use o `character_swap.patch` (`git apply character_swap.patch`) ou copie só os trechos indicados abaixo.

## Arquivos

| Arquivo | Tipo | O que faz |
|---|---|---|
| `src/character_swap.c` | novo | Toda a lógica da troca |
| `assembly/overworld_scripts/character_swap.s` | novo | Script executado ao usar a flauta |
| `strings/character_swap.string` | novo | Textos, descrição do item e nomes padrão |
| `src/config.h` | modificado | Blocos `NEW_GAME_START_LOCATION` e `CHARACTER_SWAP_SYSTEM` no final |
| `src/start_menu.c` | modificado | Opção TROCAR/SWITCH/CAMBIAR no Start Menu |
| `hooks` | modificado | Hook `DrawHelpMessageWindowWithText_CharSwap` (0x80F7974) e Hook `WarpToPlayersRoom_Custom` (0x80549F8), local de início do jogo |
| `include/pokemon_storage_system.h` | modificado | `TOTAL_BOXES_COUNT` 25 → 10, `TOTAL_BOXES_COUNT_1_LESS` 24 → 9, novo `PHYSICAL_TOTAL_BOXES_COUNT 25` |
| `src/pokemon_storage_system.c` | modificado | As 3 tabelas de boxes agora usam `[PHYSICAL_TOTAL_BOXES_COUNT]` |
| `src/Tables/item_tables.c` | modificado | Blue Flute vira Key Item com uso no campo |
| `include/new/item_tables.h` | modificado | Protótipos `FieldUseFunc_CharacterSwap` e `DESC_CHAR_SWAP_FLUTE` |

## Como funciona

Não há espaço livre no save do CFRU-expansion, então o sistema usa as boxes do PC como armazenamento. Existem 25 boxes físicas, mas agora só 10 aparecem no jogo.

- **Boxes 1 a 10:** PC do personagem ativo.
- **Boxes 11 a 20:** PC do personagem inativo (escondido).
- **Boxes 21 a 25:** todos os outros dados do personagem inativo (8421 de 8700 bytes usados).

Ao trocar, os dados do personagem atual e os guardados são permutados, o gênero é invertido e o jogo faz um warp para o local salvo do outro personagem.

**Separado por personagem:**
- **Identidade:** nome e gênero.
- **Pokémon:** party, as 10 boxes do PC, box atual, Day Care, Day Care da Route 5, mails e Pokémon fundidos (Reshiram, Zekrom, Solgaleo, Lunala, Spectrier, Glastrier).
- **Itens:** bag inteira, itens do PC, dinheiro, coins, itens registrados e flags de "item já obtido".
- **Progressão:** todas as flags e vars (vanilla e expandidas) e rematches de treinadores.
- **Local:** posição, mapa, continue warp, heal location e escape warp.
- **Pokédex:** seen e caught, se `CHAR_SWAP_SEPARATE_POKEDEX` estiver ligado.

**Compartilhado:** trainer ID, tempo de jogo, opções e as flags/vars listadas em `sSharedFlags` e `sSharedVars` no início de `character_swap.c`. Essa lista inclui dificuldade, Nuzlocke, level cap, randomizers, auto-run, opções do menu e a var de data do RTC. Edite a lista à vontade.

**Primeira troca:** o segundo personagem nasce como num New Game. Flags e vars zeradas, `EventScript_ResetAllMapFlags`, item inicial no PC, dinheiro inicial e bag vazia. Ele aparece no local definido em `config.h` (padrão: quarto em Pallet, mapa 4.1, x 6, y 6), com o nome padrão `RED` ou `LEAF` (editável em `character_swap.string`).

**A Blue Flute** é colocada automaticamente na bag do outro personagem, para ele sempre conseguir voltar.

## Novidades da v2

- **Flags diárias corrigidas:** as flags 0xE00–0xEFF do personagem inativo agora resetam quando ele volta num dia diferente. Antes elas ficavam presas, porque a var de data é compartilhada.
- **Roamers separados:** cada personagem tem seus próprios lendários errantes. Antes, se um capturasse, sumia para o outro também.
- **Tela de nome:** na primeira troca o jogador escolhe o nome do novo personagem (`CHAR_SWAP_NAMING_SCREEN`). Se deixar vazio, usa RED/LEAF.
- **Mapas grandes:** o warp agora aceita coordenadas acima de 127.
- **Novos bloqueios:** a troca também é bloqueada no Safari Zone, na Cycling Road e debaixo d'água.
- **Som:** a flauta toca `SE_FLUTE` ao trocar.

Espaço usado: 8665 de 8700 bytes. Sobram 35, então não cabe mais nada grande sem reduzir as boxes para 9 por personagem.

## Correção da v3.5 (flags 0x000–0x31F não eram trocadas)

A `struct SaveBlock1` do CFRU-expansion está deslocada 0x64 bytes em relação à ROM a partir de `trainerRematchStepCounter` (linhas `filler_478` e `safeBackupParty` em `include/global.h`). A troca usava a struct, então pegava a região errada: **as flags 0x000–0x31F ficavam iguais para os dois personagens** (inclusive a faixa 0x200–0x2FF), e Day Care, mails e rematches também eram lidos do lugar errado.

Agora esses dados são acessados nos endereços que a ROM realmente usa (flags em +0xEE0, vars em +0x1000, Day Care em +0x2F80 etc.). **Comece um save novo.**

## Correção da v3.3 (travamento ao capturar)

Com a party cheia, o Pokémon capturado vai para o PC pela função `SendMonToPC` do CFRU. Ela começa na box guardada na var `0x4037` (`VAR_PC_BOX_TO_SEND_MON`, a última box usada) e anda até achar espaço, parando só quando volta à box inicial. Se essa var tivesse um valor **10 ou maior** (por exemplo, num save feito quando o PC tinha 25 boxes), a busca **nunca terminava e o jogo travava**. Antes disso, ela ainda passaria pelas boxes escondidas do outro personagem.

- `SendMonToPC` agora corrige uma box inicial inválida para a Box 1, e o loop não passa mais do limite.
- `SendMonToBoxPos` (usada por specials de script) aceitava uma box e uma posição além do limite, por usar `>` em vez de `>=`. Corrigido.
- A troca de personagem também corrige essa var, se estiver inválida.

Essas correções continuam no `pokemon_storage_system.c` mesmo se você desinstalar a mecânica, porque também valem para o CFRU com 25 boxes.

## Novidades da v3.2

**Atenção: esta versão muda o layout do save. Comece um save NOVO.** Saves de versões anteriores tratariam o segundo personagem como novo.

- **Caixa de troca (`CHAR_SWAP_TRADE_BOX`):** a Box 10 é compartilhada pelos dois personagens e recebe o nome TROCA/TRADE/CAMBIO na primeira troca. Cada personagem fica com 9 boxes próprias.
  - Pokémon do outro personagem **obedecem e não ganham o bônus de EXP de troca**, via hook em `IsOtherTrainer` (0x8044288). O jogo reconhece os dois nomes como "seus", desde que o Trainer ID seja o mesmo.
  - Com Pokédex separada, os Pokémon da caixa de troca são registrados como vistos e capturados na Pokédex do personagem que entra, como numa troca de verdade.
- **Direção:** cada personagem volta olhando para onde estava.
- **Flash branco (`CHAR_SWAP_WHITE_FLASH`):** a troca clareia para branco em vez de escurecer. Usa a mesma proteção do CFRU para o DNS à noite (`gDontFadeWhite`) e mantém o hook do seguidor (`FollowMe_WarpSetEnd`). Comente o define para voltar ao escurecer normal.
- **Descrição do item nos três idiomas:** via hook em `ItemId_GetDescription` (0x809A96C).

Espaço usado: 8666 de 10440 bytes (6 boxes de armazenamento).

## Item "Switch" (v3.1)

A troca voltou a ser feita **só pelo item**. O item continua sendo o `ITEM_BLUE_FLUTE` (número 39), mas agora se chama **Switch**. As mensagens nos três idiomas e a descrição foram atualizadas.

A opção TROCAR/SWITCH/CAMBIAR do Start Menu foi **desligada**: com 8 opções, o menu não cabe acima da caixa de descrição. O código continua no `start_menu.c`, mas só volta a funcionar se você descomentar `CHAR_SWAP_MENU_FLAG` no `config.h`, e **não é recomendado**. Com a opção desligada, a flag 0x3E8 volta a ficar livre, e o Potion escondido da Viridian Forest não é mais afetado.

## Local de início do jogo (v2.2)

Onde **o primeiro personagem** aparece depois da intro do Oak:

```c
#define NEW_GAME_START_LOCATION
#define NEW_GAME_START_MAP_GROUP 4
#define NEW_GAME_START_MAP_NUM 1
#define NEW_GAME_START_X 6
#define NEW_GAME_START_Y 6
```

Onde **o segundo personagem** aparece na primeira troca: `CHAR_SWAP_START_*` (abaixo). São configurações separadas, então cada personagem pode começar num lugar diferente.

## Configurações (`src/config.h`)

```c
#define CHARACTER_SWAP_SYSTEM
#define CHAR_SWAP_DISABLE_FLAG 0xA0E      // com a flag setada, a flauta não funciona (use em cutscenes)
#define CHAR_SWAP_SEPARATE_POKEDEX        // comente para Pokédex compartilhada (muda o layout: decida antes de lançar o jogo)
#define CHAR_SWAP_NAMING_SCREEN           // tela de nome na primeira troca
#define CHAR_SWAP_START_MONEY 3000
#define CHAR_SWAP_START_MAP_GROUP 4       // onde o 2º personagem aparece na 1ª vez
#define CHAR_SWAP_START_MAP_NUM 1
#define CHAR_SWAP_START_X 6
#define CHAR_SWAP_START_Y 6
```

A troca é bloqueada quando há um NPC seguindo o jogador, quando ele está surfando ou mergulhando, no Safari Zone, na Cycling Road ou quando a `CHAR_SWAP_DISABLE_FLAG` está setada.

## Avisos

- **`TOTAL_BOXES_COUNT` precisa ser escrito direto como `10`** (sem `#ifdef`), porque o `insert.py` lê os defines de forma simples. Se desligar o sistema, volte manualmente para 25 e 24.
- Dá para usar menos boxes por personagem (por exemplo 8, com `TOTAL_BOXES_COUNT_1_LESS` 7). Mais de 10 não cabe; o código dá erro de compilação.
- O Day Care do personagem inativo fica "pausado" (ovos não andam).
- Os textos não usam ã/õ porque o charmap não tem esses caracteres.
- Os endereços vanilla usados (`RunScriptImmediately` 0x8069B48, `NewGameInitPCItems` 0x80EB658, `SetAllRenewableItemFlags` 0x815D838, `SetMoney` 0x809FD70, `EventScript_ResetAllMapFlags` 0x81A6481) são da BPRE 1.0, conforme os símbolos do pret/pokefirered.

## O que continua compartilhado (por design)

Hall of Fame, estatísticas do trainer card (`gameStats`), nome do rival, Trainer ID, tempo de jogo, níveis de busca do DexNav, última Poké Ball usada (botão L), último Repel, dados de Mystery Gift, Trainer Tower e registros da Battle Frontier.

## Bug do próprio CFRU-expansion (não é deste sistema)

`src/berry_tree.c` guarda as árvores em `0x203C000`, que fica **dentro da bag expandida**. As árvores 0–91 ocupam os slots 312–449 do bolso Items, e as árvores 92–127 ocupam os slots 0–53 do bolso Key Items. Com o Character Swap, as árvores acabam ficando separadas por personagem, porque são trocadas junto com a bag. Mas o conflito com a bag já existe no repositório original.
