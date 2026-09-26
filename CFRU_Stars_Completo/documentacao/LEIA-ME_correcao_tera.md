# Correção do bug de botões "que não funcionam" em batalha (CFRU-expansion)

**Não é da troca de personagem.** O bug está no código de Terastalização do CFRU-expansion (`src/battle_start_turn_start.c`, função `RunTurnActionsFunctions`).

## O problema

A máquina de estados da Terastalização volta para `Tera_Check` a cada quadro, e `Tera_Check` sempre termina com `return`. Resultado: durante as ações do turno, o script de batalha só roda **em quadros alternados** (30 vezes por segundo em vez de 60). Como o jogo só reconhece um botão como "apertado agora" durante 1 quadro, **metade dos toques cai no quadro em que o script não roda e é perdida**.

Isso afeta qualquer escolha feita dentro de um script de batalha: "Dar apelido ao Pokémon capturado?", "Trocar de Pokémon?" e similares. Confirmado no emulador com o seu `test.gba`.

## A correção

Em `src/battle_start_turn_start.c`, procure:

```c
		case Tera_End:
			gNewBS->teraData.state = 0;
			gNewBS->teraData.teraInProgress = FALSE;
```

e troque por:

```c
		case Tera_End:
			if (gCurrentActionFuncId != ACTION_USE_MOVE && gCurrentActionFuncId != ACTION_RUN_BATTLESCRIPT)
				gNewBS->teraData.state = 0;
			gNewBS->teraData.teraInProgress = FALSE;
```

É a mesma regra que o `Mega_End` (logo acima, no mesmo arquivo) já usa. O `teraData.state` continua sendo zerado no fim de cada turno em `src/end_turn.c`, como antes.

Edite à mão (não substitua o arquivo inteiro), porque o seu `battle_start_turn_start.c` tem alterações próprias, como as da `FLAG_WILD_MEGA_EVOLUTION`. O `tera_fix.patch` mostra a mesma mudança em formato diff.
