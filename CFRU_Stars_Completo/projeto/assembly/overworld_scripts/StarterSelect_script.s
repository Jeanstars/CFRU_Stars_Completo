.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

@ ============================================================
@ Exemplo de uso da tela de selecao de inicial
@ special 0xF0  -> abre a tela   (sempre seguido de waitstate)
@
@ Var8000/8001/8002 = especies das opcoes (esquerda/meio/direita)
@ Var8003 = level   | Var8004 = item   | Var8005 = 1 shiny
@ Var8006 = 1 -> so escolhe, NAO entrega o Pokemon
@
@ Depois do waitstate:
@   LASTRESULT = 0, 1 ou 2 (opcao escolhida)
@   Var8006    = especie escolhida
@   Var8007    = 0 party, 1 PC, 2 nao entregue
@ ============================================================
.global EventScript_StarterSelect
EventScript_StarterSelect:
	lock
	setvar 0x8000 0x1       @ Bulbasaur
	setvar 0x8001 0x4       @ Charmander
	setvar 0x8002 0x7       @ Squirtle
	setvar 0x8003 0x5       @ level 5
	setvar 0x8004 0x0       @ sem item
	setvar 0x8005 0x0       @ nao shiny
	setvar 0x8006 0x0       @ entregar o Pokemon
	special 0xF0
	waitstate
	release
	end
