# Desinstalador do Character Swap System

Remove a mecânica de troca de personagem do seu projeto CFRU-expansion sem apagar o resto do projeto.

## Como usar

1. Copie `desinstalar_character_swap.py` (e o `.bat`, se estiver no Windows) para a **raiz do projeto**, a pasta que tem `src`, `include` e `hooks`.
2. Rode de um destes jeitos:
   - Dê dois cliques no `desinstalar_character_swap.bat`, ou
   - No terminal: `python desinstalar_character_swap.py`
3. Confira a lista do que será feito e digite **S** para confirmar.
4. Apague a pasta `build` e compile o projeto de novo.

## O que ele faz

- **Apaga** os 3 arquivos criados pela mecânica: `src/character_swap.c`, `assembly/overworld_scripts/character_swap.s` e `strings/character_swap.string`.
- **Corrige** os 7 arquivos do CFRU que a mecânica modificou, tirando só as alterações dela: `src/config.h`, `hooks`, `src/Tables/item_tables.c`, `include/new/item_tables.h`, `src/start_menu.c`, `include/pokemon_storage_system.h` e `src/pokemon_storage_system.c`. O resto do conteúdo desses arquivos (as suas mudanças) não é tocado.
- Antes de mexer em qualquer coisa, **guarda uma cópia** de todos esses arquivos numa pasta `backup_character_swap_DATA_HORA`.

## Opções

| Comando | O que faz |
|---|---|
| `python desinstalar_character_swap.py` | Remove tudo e volta o PC para 25 boxes |
| `python desinstalar_character_swap.py --manter-boxes` | Remove tudo, mas mantém o PC com 10 boxes |
| `python desinstalar_character_swap.py --simular` | Só mostra o que seria feito, sem alterar nada |
| `python desinstalar_character_swap.py --restaurar PASTA` | Desfaz a desinstalação usando a pasta de backup |

## Sobre os saves

As boxes 11 a 25 guardam os dados do personagem que não está sendo usado. Se o PC voltar a ter 25 boxes, esses dados apareceriam como **Pokémon corrompidos** em saves feitos com a mecânica.

- Se o jogo ainda não foi lançado ou os saves podem ser descartados, use a opção normal e comece um save novo.
- Se precisar manter saves existentes, use **`--manter-boxes`**. O jogo continua com o personagem que estava ativo no save, e o outro fica guardado nas boxes escondidas, sem aparecer.

## Se aparecer um AVISO

Se alguma parte não for encontrada (por exemplo, porque você editou aquele trecho à mão), o programa pula essa parte e avisa qual arquivo ainda contém restos da mecânica. Abra esse arquivo e procure por `CharSwap`, `CHAR_SWAP`, `CHARACTER_SWAP` ou `NEW_GAME_START`.
