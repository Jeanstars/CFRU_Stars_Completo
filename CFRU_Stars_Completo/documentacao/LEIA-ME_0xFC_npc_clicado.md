# 0xFC = NPC que o jogador clicou

Em scripts, use **0xFC** no lugar do número do NPC para falar do NPC com quem o jogador acabou de conversar
(o mesmo que `LASTTALKED`). Assim você não precisa colocar o número do NPC em cada script.

    lock
    faceplayer
    applymovement 0xFC @mov
    waitmovement 0x0
    release
    end

Vale também para `turnobject 0xFC` e para os outros comandos que aceitam o número de um NPC.

- Arquivo: `projeto/src/Tables/movement_action.tables.c` (função `GetEventObjectIdByLocalIdAndMap`, que o CFRU já reescreve
  pelo `functionrewrites`, então não há gancho novo).
- Números já ocupados no CFRU: 0xFF jogador, 0xFE seguidor (Follow Me), 0xFD marcador interno, 0x7F câmera.
  O 0xFE continua sendo o seguidor, como no CFRU original.
- Para trocar o número, mude só a linha `#define LOCAL_ID_NPC_CLICADO 0xFC` nesse arquivo.
- Só funciona depois de falar com um NPC (se nenhum foi clicado, o comando não faz nada).
- A correção do sprite do NPC (hidesprite/showsprite) que eu tinha feito antes **foi retirada**: `npc_sprite_change.c`
  voltou a ser exatamente o do seu repositório.
