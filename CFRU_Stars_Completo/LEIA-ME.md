# CFRU Stars — arquivos customizados + Troca de Personagem

Este pacote junta **os seus arquivos customizados** (repositório Jeanstars/Cfrustars) com a **mecânica de troca de personagem**. Os arquivos que os dois alteravam já estão juntados, sem perder nada de nenhum lado.

## Como instalar

1. Copie **todo o conteúdo da pasta `projeto/`** para a raiz do seu projeto CFRU (a pasta que tem `src`, `include`, `hooks`) e substitua os arquivos.
2. Apague a pasta `build`.
3. Compile normalmente.
4. **Comece um save novo** (a troca de personagem mudou o layout do save).

## Organização

```
projeto/                       -> vai para a raiz do projeto CFRU
  src/config.h                 -> JUNTADO: suas flags de dificuldade + troca de personagem
  hooks                        -> JUNTADO: seu ScrCmd_setvar_Custom + hooks da troca
  src/overworld.c              -> SEU + suporte a checkgender nos treinadores
  src/battle_start_turn_start.c-> SEU + correção da Terastalização
  ...                          -> demais arquivos (seus e da troca), veja a tabela abaixo
ferramentas/
  editores/                    -> seus editores (trainer, backsprite, battle background)
  desinstalar_troca_de_personagem/ -> remove só a troca de personagem, mantendo o resto
  corrigir_pokedex_rom/        -> corrige "Seen/Owned" da Pokédex na ROM base
documentacao/
  LEIA-ME_troca_de_personagem.md -> manual completo da troca (configurações, testes, histórico)
  LEIA-ME_flags_dificuldade.txt  -> seu LEIA-ME das flags
  LEIA-ME_correcao_tera.md       -> explicação do bug dos botões em batalha
  originais/                     -> seus patches/diffs originais, só para consulta
```

## De onde vem cada arquivo de `projeto/`

| Origem | Arquivos |
|---|---|
| **Juntados** (seu + troca) | `src/config.h`, `hooks` |
| **Seus, com correção** | `src/overworld.c` (checkgender), `src/battle_start_turn_start.c` (Tera) |
| **Seus — flags de dificuldade** | `include/constants/flags.h`, `include/new/build_pokemon_2.h`, `include/new/random_trainer_pool_full.h`, `src/Battle_AI/ai_master.c`, `src/Tables/random_trainer_pool.h`, `src/build_pokemon.c`, `src/bw_summary_screen.c`, `src/cmd49.c`, `src/damage_calc.c`, `src/end_battle.c`, `src/exp.c`, `src/mega.c`, `src/npc_sprite_change.c`, `src/wild_encounter.c` |
| **Seus — treinadores animados** | `a_makepy.bat`, `treinadores_animados.bat`, `graphics/animated_trainers/`, `include/new/animated_trainer_pics.h`, `scripts/animated_trainers.py`, `scripts/treinadores_animados.py`, `src/animated_trainer_pics.c`, `src/animated_trainer_pics_table.h`, `src/battle_controller_opponent.c` |
| **Troca de personagem** | `src/character_swap.c`, `assembly/overworld_scripts/character_swap.s`, `strings/character_swap.string`, `src/Tables/item_tables.c`, `include/new/item_tables.h`, `include/pokemon_storage_system.h`, `src/pokemon_storage_system.c`, `src/start_menu.c` |

## Verificação feita

- Montei o projeto completo (CFRU-expansion original + tudo deste pacote) e compilei **todos os 104 arquivos `.c`**: nenhum erro.
- Todos os hooks novos (`ScrCmd_setvar_Custom`, `WarpToPlayersRoom_Custom`, `ItemId_GetDescription_CharSwap`, `IsOtherTrainer_CharSwap`) têm a função correspondente.
- O script e os textos da troca foram convertidos e montados sem erro.
- O desinstalador, rodado nesse projeto completo, remove só a troca de personagem e mantém as suas flags e o seu hook de `setvar`.

Ainda não foi testado no emulador depois de compilado. Faça um save novo e siga o roteiro de testes do `documentacao/LEIA-ME_troca_de_personagem.md`.
