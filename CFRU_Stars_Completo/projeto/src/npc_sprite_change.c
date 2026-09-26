#include "defines.h"
#include "../include/script.h"
#include "../include/event_data.h"
#include "../include/event_object_movement.h"

/*
 * NOVA MECANICA: trocar o sprite do NPC que o jogador clicou
 *
 * Uso no script:
 *     lock
 *     faceplayer
 *     setvar 0x7000 0x[numero do OW]
 *
 * O setvar nessa var NAO salva nada: ele so troca o sprite do NPC.
 * Qualquer outra var continua funcionando normalmente.
 *
 * Instalacao: adicionar no arquivo "hooks" do CFRU:
 *     ScrCmd_setvar_Custom 0806A390 1
 */

#define VAR_TROCAR_SPRITE_NPC 0x7000   // mude aqui se quiser outro numero

// Enderecos do FireRed (BPRE 1.0), conferidos na ROM
#define SELECTED_EVENT_OBJECT (*(u8*) 0x03005074)

// Templates dos NPCs do mapa atual (SaveBlock1 + 0x8E0 no FireRed).
// O offset e fixo porque o struct SaveBlock1 do CFRU nao bate com a ROM nesse ponto.
#define MAP_EVENT_OBJECT_TEMPLATES ((struct EventObjectTemplate*) ((u8*) gSaveBlock1 + 0x8E0))
#define MAP_EVENT_OBJECT_TEMPLATES_COUNT 64

// Rotina que o jogo usa para recriar o sprite de um NPC quando voltamos
// de um menu. O CFRU ja adaptou essa rotina para OWs expandidos (LinkNpcFix)
// e para as paletas dinamicas (SetPalNPC2), por isso ela e usada aqui.
#define SpawnEventObjectSpriteOnReturnToField ((void (*)(u8 eventObjectId, s16 x, s16 y)) (0x0805EE3C | 1))

extern const struct EventObjectGraphicsInfo* GetEventObjectGraphicsInfo(u16 graphicsId);

static void AtualizarTemplateDoNpc(struct EventObject* npc, u16 owId)
{
	// Faz o NPC continuar com o sprite novo se sair da tela e voltar
	// (vale ate o jogador sair do mapa)
	if (npc->mapNum != gSaveBlock1->location.mapNum
	||  npc->mapGroup != gSaveBlock1->location.mapGroup)
		return;

	for (u32 i = 0; i < MAP_EVENT_OBJECT_TEMPLATES_COUNT; ++i)
	{
		struct EventObjectTemplate* template = &MAP_EVENT_OBJECT_TEMPLATES[i];

		if (template->localId == npc->localId)
		{
			template->graphicsIdLowerByte = owId & 0xFF;
			template->graphicsIdUpperByte = owId >> 8;
			break;
		}
	}
}

static void TrocarSpriteDoNpcClicado(u16 owId)
{
	u8 objId = SELECTED_EVENT_OBJECT;
	struct EventObject* npc;
	struct Sprite* oldSprite;
	bool8 animPaused, affineAnimPaused;
	s16 savedData[8];
	u8 heldMovementActive, heldMovementFinished, singleMovementActive, movementActionId;

	if (objId >= EVENT_OBJECTS_COUNT)
		return;

	npc = &gEventObjects[objId];
	if (!npc->active || npc->isPlayer || npc->spriteId >= MAX_SPRITES)
		return;

	// Garante que e o NPC com quem o jogador esta falando
	if (npc->localId != gSpecialVar_LastTalked)
		return;

	oldSprite = &gSprites[npc->spriteId];
	animPaused = oldSprite->animPaused;
	affineAnimPaused = oldSprite->affineAnimPaused;

	// Guarda o estado dos movimentos do NPC (ex: o faceplayer em andamento),
	// que fica salvo dentro do sprite
	for (u32 i = 0; i < NELEMS(savedData); ++i)
		savedData[i] = oldSprite->data[i];

	heldMovementActive = npc->heldMovementActive;
	heldMovementFinished = npc->heldMovementFinished;
	singleMovementActive = npc->singleMovementActive;
	movementActionId = npc->movementActionId;

	// 1. Destroi o sprite antigo (o CFRU libera os tiles e a paleta dele)
	DestroySprite(oldSprite);
	npc->spriteId = MAX_SPRITES;

	// 2. Troca o OW do NPC
	npc->graphicsIdLowerByte = owId & 0xFF;
	npc->graphicsIdUpperByte = owId >> 8;
	npc->inanimate = GetEventObjectGraphicsInfo(owId)->inanimate;
	AtualizarTemplateDoNpc(npc, owId);

	// 3. Cria o sprite novo com tiles, paleta, sombra e reflexo corretos
	SpawnEventObjectSpriteOnReturnToField(objId, 0, 0);

	// 4. Restaura o estado dos movimentos e o "congelado" do NPC
	//    (a rotina de recriar o sprite apaga o movimento em andamento)
	npc->heldMovementActive = heldMovementActive;
	npc->heldMovementFinished = heldMovementFinished;
	npc->singleMovementActive = singleMovementActive;
	npc->movementActionId = movementActionId;

	if (npc->spriteId < MAX_SPRITES)
	{
		struct Sprite* newSprite = &gSprites[npc->spriteId];

		for (u32 i = 0; i < NELEMS(savedData); ++i)
			newSprite->data[i] = savedData[i];

		newSprite->animPaused = animPaused;
		newSprite->affineAnimPaused = affineAnimPaused;
	}
}

// Substitui o comando setvar (0x16) original
bool8 ScrCmd_setvar_Custom(struct ScriptContext* ctx)
{
	u16 varId = ScriptReadHalfword(ctx);
	u16 value = ScriptReadHalfword(ctx);

	if (varId == VAR_TROCAR_SPRITE_NPC)
	{
		TrocarSpriteDoNpcClicado(value);
		return FALSE;
	}

	// Comportamento normal do setvar
	u16* ptr = GetVarPointer(varId);
	if (ptr != NULL)
		*ptr = value;

	return FALSE;
}
