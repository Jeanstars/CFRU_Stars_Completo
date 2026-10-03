#include "defines.h"
#include "../include/event_data.h"
#include "../include/field_player_avatar.h"
#include "../include/event_object_movement.h"
#include "../include/field_screen_effect.h"
#include "../include/field_weather.h"
#include "../include/constants/field_weather.h"
#include "../include/item_use.h"
#include "../include/naming_screen.h"
#include "../include/overworld.h"
#include "../include/pokemon_storage_system.h"
#include "../include/script.h"
#include "../include/string_util.h"
#include "../include/constants/items.h"
#include "../include/constants/flags.h"
#include "../include/constants/vars.h"

#include "../include/new/follow_me.h"
#include "../include/new/item.h"
#include "../include/new/item_tables.h"
#include "../include/new/pokemon_storage_system.h"
#include "../include/new/roamer.h"
#include "../include/new/character_swap_screen.h"

/*
character_swap.c
	Character Swap System: the Blue Flute swaps between the male and the female character.

	Each character has their own: party, PC boxes, bag, PC items, money, coins, flags, vars,
	location, name, Day Care, mail, fused Pokemon, trainer rematches and (optionally) Pokedex.

	How it works (no free save space is needed):
		Only TOTAL_BOXES_COUNT (10) boxes are visible, but 25 boxes still exist in the save.
		- Physical boxes  1-10 : PC of the ACTIVE character
		- Physical boxes 11-20 : PC of the INACTIVE character
		- Physical boxes 21-25 : all the other data of the INACTIVE character
		Swapping characters simply swaps the live data with the stored data, then warps.
*/

/* ---------------------- New game start location ---------------------- */
//Replaces vanilla WarpToPlayersRoom (0x80549F8), called by NewGameInitData after Oak's intro
#ifdef NEW_GAME_START_LOCATION
#define WarpIntoMap_Vanilla ((void (*)(void)) (0x8055378 | 1))
#define sWarpDestination_NewGame ((struct WarpData*) 0x2031DBC)

extern void LanguageSelect_ReapplyAfterNewGame(void);

void WarpToPlayersRoom_Custom(void)
{
	LanguageSelect_ReapplyAfterNewGame(); //O Novo Jogo zerou as flags de idioma (0x260-0x262): liga de novo a escolhida
	SetWarpDestination(NEW_GAME_START_MAP_GROUP, NEW_GAME_START_MAP_NUM, -1, NEW_GAME_START_X, NEW_GAME_START_Y);
	sWarpDestination_NewGame->x = NEW_GAME_START_X; //Allow coords above 127
	sWarpDestination_NewGame->y = NEW_GAME_START_Y;
	WarpIntoMap_Vanilla();
}
#endif

#ifdef CHARACTER_SWAP_SYSTEM

#define CHAR_SWAP_MAGIC 0x43534157 //"CSAW" - stored data is valid

#define BOX_BYTES (sizeof(struct CompressedPokemon) * IN_BOX_COUNT) //1740
#ifdef CHAR_SWAP_TRADE_BOX
	#define NUM_PERSONAL_BOXES (TOTAL_BOXES_COUNT - 1) //The last visible box is shared by both characters
	#define TRADE_BOX_ID (TOTAL_BOXES_COUNT - 1)
#else
	#define NUM_PERSONAL_BOXES (TOTAL_BOXES_COUNT)
#endif
#define FIRST_INACTIVE_BOX (TOTAL_BOXES_COUNT)
#define FIRST_STORAGE_BOX (TOTAL_BOXES_COUNT + NUM_PERSONAL_BOXES)
#define NUM_STORAGE_BOXES (PHYSICAL_TOTAL_BOXES_COUNT - FIRST_STORAGE_BOX)

#if FIRST_STORAGE_BOX > PHYSICAL_TOTAL_BOXES_COUNT
	#error "Character Swap System: TOTAL_BOXES_COUNT is too big. Use 10 or less."
#endif

//CFRU RAM locations
#define BAG_RAM ((u8*) 0x203BB20)
#define BAG_RAM_SIZE ((450 + 75 + 50 + 128 + 75) * sizeof(struct ItemSlot)) //Whole bag expansion region
#define EXPANDED_FLAGS_SIZE 0x200
#define EXPANDED_VARS_SIZE (0x200 * sizeof(u16))
#define PC_CURRENT_BOX ((u8*) 0x2029314)

//Vanilla SaveBlock1 offsets. CFRU-expansion's "struct SaveBlock1" is shifted by 0x64 bytes from
//trainerRematchStepCounter onward (filler_478 + safeBackupParty), so these fields are accessed
//at the offsets the ROM really uses instead of through the struct.
#define SB1(offset) (((u8*) gSaveBlock1) + (offset))
#define SB1_FLAGS           SB1(0x0EE0)
#define SB1_FLAGS_SIZE      0x120
#define SB1_VARS            SB1(0x1000)
#define SB1_VARS_SIZE       0x200
#define SB1_REMATCH_COUNTER SB1(0x0638)
#define SB1_REMATCHES       SB1(0x063A)
#define SB1_REMATCHES_SIZE  100
#define SB1_MAIL            SB1(0x2CD0)
#define SB1_MAIL_SIZE       0x240
#define SB1_DAYCARE_OFFSET  0x2F80
#define SB1_DAYCARE_SIZE    0x11C
#define SB1_ROUTE5_DAYCARE  SB1(0x3C98)
#define SB1_ROUTE5_SIZE     0x8C

//CFRU's itemObtainedFlags (accessed through the struct by item.c) may overlap the vanilla Day Care.
//Both are swapped as one block so no byte is swapped twice.
#define ITEM_FLAGS_OFFSET   __builtin_offsetof(struct SaveBlock1, itemObtainedFlags)
#define ITEM_FLAGS_SIZE     sizeof(((struct SaveBlock1*)0)->itemObtainedFlags)
#define DAYCARE_BLOCK_START (ITEM_FLAGS_OFFSET < SB1_DAYCARE_OFFSET ? ITEM_FLAGS_OFFSET : SB1_DAYCARE_OFFSET)
#define DAYCARE_BLOCK_END   ((ITEM_FLAGS_OFFSET + ITEM_FLAGS_SIZE) > (SB1_DAYCARE_OFFSET + SB1_DAYCARE_SIZE) ? (ITEM_FLAGS_OFFSET + ITEM_FLAGS_SIZE) : (SB1_DAYCARE_OFFSET + SB1_DAYCARE_SIZE))
#define ITEM_DAYCARE_OVERLAP (ITEM_FLAGS_OFFSET < SB1_DAYCARE_OFFSET + SB1_DAYCARE_SIZE && SB1_DAYCARE_OFFSET < ITEM_FLAGS_OFFSET + ITEM_FLAGS_SIZE)

//Vanilla FR functions/data not in BPRE.ld (addresses from pret/pokefirered symbols)
#define RunScriptImmediately ((void (*)(const u8*)) (0x8069B48 | 1))
#define NewGameInitPCItems ((void (*)(void)) (0x80EB658 | 1))
#define SetAllRenewableItemFlags ((void (*)(void)) (0x815D838 | 1))
#define SetMoneyVanilla ((void (*)(u32*, u32)) (0x809FD70 | 1))
#define EventScript_ResetAllMapFlags ((const u8*) 0x81A6481)
#define sWarpDestination ((struct WarpData*) 0x2031DBC)
#define SetInitialPlayerAvatarStateWithDirection ((void (*)(u8)) (0x80559F8 | 1))
#define palette_bg_faded_fill_white ((void (*)(void)) (0x807DB14 | 1))
#define Task_Teleport2Warp ((TaskFunc) (0x807E718 | 1))
#define Task_ExitNonDoor ((TaskFunc) (0x807E2CC | 1))

#define ROAMERS_SIZE (sizeof(struct Roamer) * MAX_NUM_ROAMERS)

//From pokemon_storage_system.c
extern struct CompressedPokemon* const sPokemonBoxPtrs[];
extern u8 (* const sPokemonBoxNamePtrs[])[9];
extern u8* const sPokemonBoxWallpaperPtrs[];

extern const u8 SystemScript_CharacterSwap[];
extern const u8 gText_CharSwap_DefaultMaleName[];
extern const u8 gText_CharSwap_DefaultFemaleName[];
extern const u8 gText_CharSwap_TradeBoxName[];
extern const u8 gText_CharSwap_TradeBoxName_EN[];
extern const u8 gText_CharSwap_TradeBoxName_ES[];
extern const u8 DESC_CHAR_SWAP_FLUTE[];
extern const u8 DESC_CHAR_SWAP_FLUTE_EN[];
extern const u8 DESC_CHAR_SWAP_FLUTE_ES[];
extern bool8 gDontFadeWhite;
extern void FollowMe_WarpSetEnd(void);

/* ------------------------------------------------------------------ */
/* Flags/Vars that are SHARED by both characters (game settings).     */
/* Add or remove whatever you want here.                              */
/* ------------------------------------------------------------------ */
static const u16 sSharedFlags[] =
{
	0x260, //Language
	0x261, //Language
	0x262, //Language
	#ifdef FLAG_NUZLOCKE
	FLAG_NUZLOCKE,
	#endif
	#ifdef FLAG_HARD_LEVEL_CAP
	FLAG_HARD_LEVEL_CAP,
	#endif
	#ifdef FLAG_KEPT_LEVEL_CAP_ON
	FLAG_KEPT_LEVEL_CAP_ON,
	#endif
	#ifdef FLAG_SANDBOX_MODE
	FLAG_SANDBOX_MODE,
	#endif
	#ifdef FLAG_POKEMON_RANDOMIZER
	FLAG_POKEMON_RANDOMIZER,
	#endif
	#ifdef FLAG_POKEMON_LEARNSET_RANDOMIZER
	FLAG_POKEMON_LEARNSET_RANDOMIZER,
	#endif
	#ifdef FLAG_ABILITY_RANDOMIZER
	FLAG_ABILITY_RANDOMIZER,
	#endif
	#ifdef FLAG_AUTO_RUN
	FLAG_AUTO_RUN,
	#endif
	#ifdef CHAR_SWAP_DISABLE_FLAG
	CHAR_SWAP_DISABLE_FLAG,
	#endif
	#ifdef CHAR_SWAP_MENU_FLAG
	CHAR_SWAP_MENU_FLAG, //Start Menu option: must be shared, or the other character couldn't swap back
	#endif
	0xFFFF, //Terminator (don't remove)
};

static const u16 sSharedVars[] =
{
	#ifdef VAR_GAME_DIFFICULTY
	VAR_GAME_DIFFICULTY,
	#endif
	#ifdef VAR_R_BUTTON_MODE
	VAR_R_BUTTON_MODE,
	#endif
	#ifdef VAR_BATTLE_MUSIC
	VAR_BATTLE_MUSIC,
	#endif
	#ifdef VAR_WILD_LEVEL_SCALING
	VAR_WILD_LEVEL_SCALING,
	#endif
	#ifdef VAR_AUTO_SORT_BAG
	VAR_AUTO_SORT_BAG,
	#endif
	#ifdef VAR_SWARM_INDEX
	VAR_SWARM_INDEX,
	#endif
	#ifdef VAR_SWARM_DAILY_EVENT
	VAR_SWARM_DAILY_EVENT, //Also used by the RTC "time set in the future" check
	VAR_SWARM_DAILY_EVENT + 1,
	#endif
	#ifdef VAR_RAID_PARTNER_RANDOM_NUM
	VAR_RAID_PARTNER_RANDOM_NUM,
	#endif
	0xFFFF, //Terminator (don't remove)
};

#define MAX_SHARED 32
_Static_assert(NELEMS(sSharedFlags) <= MAX_SHARED + 1 && NELEMS(sSharedVars) <= MAX_SHARED + 1, "Character Swap System: too many shared flags/vars (max 32 each)");
#define CHAR_SWAP_NAME_BUFFER gStringVarC //Temporary name typed in the naming screen

#ifdef VAR_SWARM_DAILY_EVENT
	#define GET_CURRENT_DAY_STAMP() (VarGet(VAR_SWARM_DAILY_EVENT) | (VarGet(VAR_SWARM_DAILY_EVENT + 1) << 16))
#else
	#define GET_CURRENT_DAY_STAMP() 0
#endif

enum
{
	MODE_SWAP, //Live <-> Stored
	MODE_SAVE, //Live -> Stored
	MODE_PEEK, //Stored -> small buffers (read-only: changes nothing). Used by the character selection screen.
};

enum
{
	CHAR_SWAP_OK,
	CHAR_SWAP_CANT_FOLLOWER,
	CHAR_SWAP_CANT_SURFING,
	CHAR_SWAP_CANT_DISABLED,
};

//MODE_PEEK: "copy bytes [offset, offset + length) of the block whose live address is 'live' into 'dest'".
//The list ends with live == NULL. It is passed by pointer (this build has no writable globals).
struct PeekRequest
{
	const void* live;
	u16 offset;
	u16 length;
	u8* dest;
};

struct StorageStream
{
	u8 box;
	u16 offset;
	const struct PeekRequest* peek; //Only used by MODE_PEEK
};

//This file's functions:
static u8* StreamNextByte(struct StorageStream* s);
static void ProcessBlock(struct StorageStream* s, void* live, u32 size, u8 mode);
static void ProcessAllData(u8 mode, u32* dayStamp, u8* facing, const struct PeekRequest* peek);
static void PeekBlock(struct StorageStream* s, const void* live, u32 size);
static void SwapMemory(u8* a, u8* b, u32 size);
static void SwapPCBoxes(void);
static void ClearInactivePCBoxes(void);
static void InitFreshCharacter(void);
static void CharSwap_SetWarpData(struct WarpData* warp, s8 mapGroup, s8 mapNum, s16 x, s16 y);
static u32* GetStorageMagicPtr(void);

/* ------------------------- Storage stream ------------------------- */

static u32* GetStorageMagicPtr(void)
{
	return (u32*) sPokemonBoxPtrs[FIRST_STORAGE_BOX]; //First 4 bytes of the storage
}

static u8* StreamNextByte(struct StorageStream* s)
{
	if (s->offset >= BOX_BYTES)
	{
		s->box++;
		s->offset = 0;
	}

	return ((u8*) sPokemonBoxPtrs[s->box]) + s->offset++;
}

//Reads one block of the stream without touching the live data or the stored data
static void PeekBlock(struct StorageStream* s, const void* live, u32 size)
{
	for (u32 i = 0; i < size; ++i)
	{
		u8 value = *StreamNextByte(s);

		for (const struct PeekRequest* req = s->peek; req != NULL && req->live != NULL; ++req)
		{
			if (req->live == live && i >= req->offset && i < (u32) req->offset + req->length)
				req->dest[i - req->offset] = value;
		}
	}
}

static void ProcessBlock(struct StorageStream* s, void* live, u32 size, u8 mode)
{
	u8* p = (u8*) live;

	if (mode == MODE_PEEK)
	{
		PeekBlock(s, live, size);
		return;
	}

	while (size-- > 0)
	{
		u8* stored = StreamNextByte(s);

		if (mode == MODE_SWAP)
		{
			u8 temp = *stored;
			*stored = *p;
			*p = temp;
		}
		else //MODE_SAVE
			*stored = *p;

		++p;
	}
}

#define DATA(ptr, size) ProcessBlock(&s, (void*) (ptr), (size), mode)

//Every piece of data that belongs to one character. Total must fit in NUM_STORAGE_BOXES * BOX_BYTES.
static void ProcessAllData(u8 mode, u32* dayStamp, u8* facing, const struct PeekRequest* peek)
{
	struct StorageStream s;
	s.box = FIRST_STORAGE_BOX;
	s.offset = sizeof(u32); //Skip the magic number
	s.peek = peek;

	//Identity
	DATA(gSaveBlock2->playerName, sizeof(gSaveBlock2->playerName));

	//Pokemon
	DATA(&gPlayerPartyCount, sizeof(gPlayerPartyCount));
	DATA(gPlayerParty, sizeof(struct Pokemon) * PARTY_SIZE);
	DATA(PC_CURRENT_BOX, 1);
	if (ITEM_DAYCARE_OVERLAP)
		DATA(SB1(DAYCARE_BLOCK_START), DAYCARE_BLOCK_END - DAYCARE_BLOCK_START); //Day Care + item obtained flags
	else
	{
		DATA(SB1(SB1_DAYCARE_OFFSET), SB1_DAYCARE_SIZE);
		DATA(SB1(ITEM_FLAGS_OFFSET), ITEM_FLAGS_SIZE);
	}
	DATA(SB1_ROUTE5_DAYCARE, SB1_ROUTE5_SIZE);
	DATA(SB1_MAIL, SB1_MAIL_SIZE);
	DATA(&gSaveBlock1->fusedReshiram, sizeof(struct Pokemon));
	DATA(&gSaveBlock1->fusedZekrom, sizeof(struct Pokemon));
	DATA(&gSaveBlock1->fusedSolgaleo, sizeof(struct Pokemon));
	DATA(&gSaveBlock1->fusedLunala, sizeof(struct Pokemon));
	DATA(&gSaveBlock2->fusedSpectrier, sizeof(struct Pokemon));
	DATA(&gSaveBlock2->fusedGlastrier, sizeof(struct Pokemon));

	//Items
	DATA(BAG_RAM, BAG_RAM_SIZE);
	DATA(gSaveBlock1->pcItems, sizeof(gSaveBlock1->pcItems));
	DATA(&gSaveBlock1->money, sizeof(gSaveBlock1->money));
	DATA(&gSaveBlock1->coins, sizeof(gSaveBlock1->coins));
	DATA(&gPlayerCoins, sizeof(u32));
	DATA(&gSaveBlock1->oldRegisteredItem, sizeof(gSaveBlock1->oldRegisteredItem));
	DATA(gSaveBlock1->registeredItems, sizeof(gSaveBlock1->registeredItems));

	//Progression
	DATA(SB1_FLAGS, SB1_FLAGS_SIZE);
	DATA(gExpandedFlags, EXPANDED_FLAGS_SIZE);
	DATA(SB1_VARS, SB1_VARS_SIZE);
	DATA(gExpandedVars, EXPANDED_VARS_SIZE);
	DATA(SB1_REMATCH_COUNTER, sizeof(u16));
	DATA(SB1_REMATCHES, SB1_REMATCHES_SIZE);

	//Location
	DATA(&gSaveBlock1->pos, sizeof(gSaveBlock1->pos));
	DATA(&gSaveBlock1->location, sizeof(struct WarpData));
	DATA(&gSaveBlock1->continueGameWarp, sizeof(struct WarpData));
	DATA(&gSaveBlock1->dynamicWarp, sizeof(struct WarpData));
	DATA(&gSaveBlock1->lastHealLocation, sizeof(struct WarpData));
	DATA(&gSaveBlock1->escapeWarp, sizeof(struct WarpData));
	DATA(&gSaveBlock1->lastHealingSpot, sizeof(gSaveBlock1->lastHealingSpot));

	#ifdef CHAR_SWAP_SEPARATE_POKEDEX
	DATA(gSaveBlock1->dexSeenFlags, sizeof(gSaveBlock1->dexSeenFlags));
	DATA(gSaveBlock1->dexCaughtFlags, sizeof(gSaveBlock1->dexCaughtFlags));
	#endif

	//v2 - New data is always added at the end so older saves keep working
	DATA(dayStamp, sizeof(u32)); //Last day this character was played (for daily events)
	DATA(gRoamers, ROAMERS_SIZE); //Roaming legendaries

	//v3.2
	DATA(facing, sizeof(u8)); //Direction the character was facing
}

#undef DATA

//Compile-time check that everything fits in the storage boxes
#define CHAR_SWAP_DATA_SIZE (sizeof(u32) \
	+ sizeof(((struct SaveBlock2*)0)->playerName) \
	+ 1 + sizeof(struct Pokemon) * PARTY_SIZE + 1 \
	+ (DAYCARE_BLOCK_END - DAYCARE_BLOCK_START) + ITEM_FLAGS_SIZE + SB1_ROUTE5_SIZE + SB1_MAIL_SIZE \
	+ sizeof(struct Pokemon) * 6 \
	+ BAG_RAM_SIZE + sizeof(((struct SaveBlock1*)0)->pcItems) + 4 + 2 + 4 + 2 \
	+ sizeof(((struct SaveBlock1*)0)->registeredItems) \
	+ SB1_FLAGS_SIZE + EXPANDED_FLAGS_SIZE \
	+ SB1_VARS_SIZE + EXPANDED_VARS_SIZE \
	+ 2 + SB1_REMATCHES_SIZE \
	+ sizeof(struct Coords16) + sizeof(struct WarpData) * 5 + 1 \
	+ sizeof(((struct SaveBlock1*)0)->dexSeenFlags) * 2 \
	+ sizeof(u32) + ROAMERS_SIZE + 1)

_Static_assert(CHAR_SWAP_DATA_SIZE <= NUM_STORAGE_BOXES * BOX_BYTES, "Character Swap System: data doesn't fit in the storage boxes!");

/* ---------------------------- PC boxes ---------------------------- */

static void SwapMemory(u8* a, u8* b, u32 size)
{
	while (size-- > 0)
	{
		u8 temp = *a;
		*a++ = *b;
		*b++ = temp;
	}
}

//Visible boxes (1-10) <-> inactive character boxes (11-20)
static void SwapPCBoxes(void)
{
	for (u32 i = 0; i < NUM_PERSONAL_BOXES; ++i)
	{
		u32 j = i + FIRST_INACTIVE_BOX;
		SwapMemory((u8*) sPokemonBoxPtrs[i], (u8*) sPokemonBoxPtrs[j], BOX_BYTES);
		SwapMemory((u8*) sPokemonBoxNamePtrs[i], (u8*) sPokemonBoxNamePtrs[j], 9);
		SwapMemory(sPokemonBoxWallpaperPtrs[i], sPokemonBoxWallpaperPtrs[j], 1);
	}
}

//Empty PC for a character used for the first time: "BOX1".."BOX10"
static void ClearInactivePCBoxes(void)
{
	for (u32 i = 0; i < NUM_PERSONAL_BOXES; ++i)
	{
		u32 j = i + FIRST_INACTIVE_BOX;
		u8* name = (u8*) sPokemonBoxNamePtrs[j];
		u32 num = i + 1;

		Memset(sPokemonBoxPtrs[j], 0, BOX_BYTES);

		name[0] = 0xBC; //B
		name[1] = 0xC9; //O
		name[2] = 0xD2; //X
		if (num >= 10)
		{
			name[3] = 0xA1 + (num / 10);
			name[4] = 0xA1 + (num % 10);
			name[5] = EOS;
		}
		else
		{
			name[3] = 0xA1 + num;
			name[4] = EOS;
		}

		*sPokemonBoxWallpaperPtrs[j] = i % 4;
	}
}

/* ------------------------ First time setup ------------------------ */

static void CharSwap_SetWarpData(struct WarpData* warp, s8 mapGroup, s8 mapNum, s16 x, s16 y)
{
	warp->mapGroup = mapGroup;
	warp->mapNum = mapNum;
	warp->warpId = -1;
	warp->x = x;
	warp->y = y;
}

//Leaves the live data like a brand new game (called after the old character was saved)
static void InitFreshCharacter(void)
{
	//Name (gender was already changed)
	StringCopy(gSaveBlock2->playerName, (gSaveBlock2->playerGender == MALE) ? gText_CharSwap_DefaultMaleName : gText_CharSwap_DefaultFemaleName);

	//Pokemon
	ZeroPlayerPartyMons();
	gPlayerPartyCount = 0;
	*PC_CURRENT_BOX = 0;
	Memset(SB1(DAYCARE_BLOCK_START), 0, DAYCARE_BLOCK_END - DAYCARE_BLOCK_START); //Day Care (+ item obtained flags if they overlap)
	Memset(SB1(ITEM_FLAGS_OFFSET), 0, ITEM_FLAGS_SIZE);
	Memset(SB1_ROUTE5_DAYCARE, 0, SB1_ROUTE5_SIZE);
	Memset(SB1_MAIL, 0, SB1_MAIL_SIZE);
	Memset(&gSaveBlock1->fusedReshiram, 0, sizeof(struct Pokemon));
	Memset(&gSaveBlock1->fusedZekrom, 0, sizeof(struct Pokemon));
	Memset(&gSaveBlock1->fusedSolgaleo, 0, sizeof(struct Pokemon));
	Memset(&gSaveBlock1->fusedLunala, 0, sizeof(struct Pokemon));
	Memset(&gSaveBlock2->fusedSpectrier, 0, sizeof(struct Pokemon));
	Memset(&gSaveBlock2->fusedGlastrier, 0, sizeof(struct Pokemon));

	//Items
	Memset(BAG_RAM, 0, BAG_RAM_SIZE);
	Memset(gSaveBlock1->pcItems, 0, sizeof(gSaveBlock1->pcItems));
	NewGameInitPCItems();
	SetMoneyVanilla(&gSaveBlock1->money, CHAR_SWAP_START_MONEY);
	gSaveBlock1->coins = 0;
	gPlayerCoins = 0;
	gSaveBlock1->oldRegisteredItem = 0;
	Memset(gSaveBlock1->registeredItems, 0, sizeof(gSaveBlock1->registeredItems));

	//Progression - same as a new game
	Memset(SB1_FLAGS, 0, SB1_FLAGS_SIZE);
	Memset(gExpandedFlags, 0, EXPANDED_FLAGS_SIZE);
	Memset(SB1_VARS, 0, SB1_VARS_SIZE);
	Memset(gExpandedVars, 0, EXPANDED_VARS_SIZE);
	Memset(SB1_REMATCH_COUNTER, 0, sizeof(u16));
	Memset(SB1_REMATCHES, 0, SB1_REMATCHES_SIZE);
	SetAllRenewableItemFlags();
	RunScriptImmediately(EventScript_ResetAllMapFlags); //Hides the NPCs/items like at the start of the game

	//Location
	gSaveBlock1->pos.x = CHAR_SWAP_START_X;
	gSaveBlock1->pos.y = CHAR_SWAP_START_Y;
	CharSwap_SetWarpData(&gSaveBlock1->location, CHAR_SWAP_START_MAP_GROUP, CHAR_SWAP_START_MAP_NUM, CHAR_SWAP_START_X, CHAR_SWAP_START_Y);
	CharSwap_SetWarpData(&gSaveBlock1->continueGameWarp, CHAR_SWAP_START_MAP_GROUP, CHAR_SWAP_START_MAP_NUM, CHAR_SWAP_START_X, CHAR_SWAP_START_Y);
	CharSwap_SetWarpData(&gSaveBlock1->dynamicWarp, CHAR_SWAP_START_MAP_GROUP, CHAR_SWAP_START_MAP_NUM, CHAR_SWAP_START_X, CHAR_SWAP_START_Y);
	CharSwap_SetWarpData(&gSaveBlock1->lastHealLocation, CHAR_SWAP_START_MAP_GROUP, CHAR_SWAP_START_MAP_NUM, CHAR_SWAP_START_X, CHAR_SWAP_START_Y);
	CharSwap_SetWarpData(&gSaveBlock1->escapeWarp, CHAR_SWAP_START_MAP_GROUP, CHAR_SWAP_START_MAP_NUM, CHAR_SWAP_START_X, CHAR_SWAP_START_Y);
	gSaveBlock1->lastHealingSpot = 0;

	#ifdef CHAR_SWAP_SEPARATE_POKEDEX
	Memset(gSaveBlock1->dexSeenFlags, 0, sizeof(gSaveBlock1->dexSeenFlags));
	Memset(gSaveBlock1->dexCaughtFlags, 0, sizeof(gSaveBlock1->dexCaughtFlags));
	#endif

	Memset(gRoamers, 0, ROAMERS_SIZE);

	#ifdef CHAR_SWAP_NAMING_SCREEN
	if (CHAR_SWAP_NAME_BUFFER[0] != EOS) //Name chosen in the naming screen
	{
		//Bounded copy: never write past playerName even if the buffer has no terminator
		u32 i;
		for (i = 0; i < PLAYER_NAME_LENGTH && CHAR_SWAP_NAME_BUFFER[i] != EOS; ++i)
			gSaveBlock2->playerName[i] = CHAR_SWAP_NAME_BUFFER[i];
		gSaveBlock2->playerName[i] = EOS;
	}
	#endif
}

/* ---------------------------- Helpers ----------------------------- */

static const u8* GetLanguageText(const u8* pt, const u8* en, const u8* es)
{
	#ifdef CHAR_SWAP_FLAG_LANG_ENGLISH
	if (FlagGet(CHAR_SWAP_FLAG_LANG_ENGLISH))
		return en;
	#endif
	#ifdef CHAR_SWAP_FLAG_LANG_SPANISH
	if (FlagGet(CHAR_SWAP_FLAG_LANG_SPANISH))
		return es;
	#endif
	(void) en; (void) es;
	return pt;
}

#if defined(CHAR_SWAP_TRADE_BOX) && defined(CHAR_SWAP_SEPARATE_POKEDEX)
static void RegisterTradeBoxInPokedex(void)
{
	for (u32 i = 0; i < IN_BOX_COUNT; ++i)
	{
		u16 species = GetBoxMonDataAt(TRADE_BOX_ID, i, MON_DATA_SPECIES);
		if (species != SPECIES_NONE && !GetBoxMonDataAt(TRADE_BOX_ID, i, MON_DATA_IS_EGG))
		{
			u16 dexNum = SpeciesToNationalPokedexNum(species);
			GetSetPokedexFlag(dexNum, FLAG_SET_SEEN);
			GetSetPokedexFlag(dexNum, FLAG_SET_CAUGHT);
		}
	}
}
#endif

#ifdef CHAR_SWAP_WHITE_FLASH
//Arrival: same as FieldCB_DefaultWarpExit (including CFRU's follower hook), but fading in from white
static void FieldCB_CharSwapWhiteIn(void)
{
	Overworld_PlaySpecialMapMusic();
	FollowMe_WarpSetEnd();
	palette_bg_faded_fill_white();
	FadeScreen(FADE_FROM_WHITE, 0);
	palette_bg_faded_fill_white();
	CreateTask(Task_ExitNonDoor, 10);
	ScriptContext2_Enable();
}

//Departure: same as DoWarp, but fading out to white
static void DoWhiteFlashWarp(void)
{
	ScriptContext2_Enable();
	TryFadeOutOldMapMusic();
	gDontFadeWhite = TRUE; //Same protection CFRU uses for white warps (DNS at night)
	FadeScreen(FADE_TO_WHITE, 0);
	PlayRainStoppingSoundEffect();
	gFieldCallback = FieldCB_CharSwapWhiteIn;
	CreateTask(Task_Teleport2Warp, 10);
}
#endif

/* -------------------------- Script API ---------------------------- */

//callasm CharSwap_CheckCanSwap -> LASTRESULT
void CharSwap_CheckCanSwap(void)
{
	gSpecialVar_LastResult = CHAR_SWAP_OK;

	#ifdef CHAR_SWAP_DISABLE_FLAG
	if (FlagGet(CHAR_SWAP_DISABLE_FLAG))
		gSpecialVar_LastResult = CHAR_SWAP_CANT_DISABLED;
	else
	#endif
	if (gFollowerState.inProgress && !IsFollowerPokemon())
		gSpecialVar_LastResult = CHAR_SWAP_CANT_FOLLOWER; //An NPC is following the player
	else if (TestPlayerAvatarFlags(PLAYER_AVATAR_FLAG_SURFING | PLAYER_AVATAR_FLAG_UNDERWATER))
		gSpecialVar_LastResult = CHAR_SWAP_CANT_SURFING;
	else if (FlagGet(FLAG_SYS_SAFARI_MODE) || FlagGet(FLAG_SYS_ON_CYCLING_ROAD))
		gSpecialVar_LastResult = CHAR_SWAP_CANT_DISABLED; //Safari Zone or Cycling Road
}

//callasm CharSwap_ShouldAskName -> LASTRESULT 1 if the other character is used for the first time
void CharSwap_ShouldAskName(void)
{
	gSpecialVar_LastResult = FALSE;
	CHAR_SWAP_NAME_BUFFER[0] = EOS;

	#ifdef CHAR_SWAP_NAMING_SCREEN
	if (*GetStorageMagicPtr() != CHAR_SWAP_MAGIC)
	{
		StringCopy(CHAR_SWAP_NAME_BUFFER, (gSaveBlock2->playerGender == MALE) ? gText_CharSwap_DefaultFemaleName : gText_CharSwap_DefaultMaleName);
		gSpecialVar_LastResult = TRUE;
	}
	#endif
}

//callasm CharSwap_OpenNamingScreen (after fadescreen, follow with waitstate)
void CharSwap_OpenNamingScreen(void)
{
	DoNamingScreen(NAMING_SCREEN_PLAYER, CHAR_SWAP_NAME_BUFFER, gSaveBlock2->playerGender ^ 1, 0, 0, CB2_ReturnToFieldContinueScript);
}

//callasm CharSwap_SwapAndWarp (follow with waitstate)
void CharSwap_SwapAndWarp(void)
{
	u32 i;
	u8 sharedFlags[MAX_SHARED];
	u16 sharedVars[MAX_SHARED];
	bool8 firstTime = (*GetStorageMagicPtr() != CHAR_SWAP_MAGIC);
	u32 today = GET_CURRENT_DAY_STAMP();
	u32 dayStamp = today;
	u8 facing = GetPlayerFacingDirection();

	//Remember the settings shared by both characters
	for (i = 0; i < MAX_SHARED && sSharedFlags[i] != 0xFFFF; ++i)
		sharedFlags[i] = FlagGet(sSharedFlags[i]);
	for (i = 0; i < MAX_SHARED && sSharedVars[i] != 0xFFFF; ++i)
		sharedVars[i] = VarGet(sSharedVars[i]);

	//Store the current position (player's real coords)
	PlayerGetDestCoords(&gSaveBlock1->pos.x, &gSaveBlock1->pos.y);
	gSaveBlock1->pos.x -= 7; //MAP_OFFSET
	gSaveBlock1->pos.y -= 7;

	//PC
	if (firstTime)
		ClearInactivePCBoxes();
	SwapPCBoxes();

	//Everything else
	if (firstTime)
	{
		ProcessAllData(MODE_SAVE, &dayStamp, &facing, NULL); //Current character goes to storage
		*GetStorageMagicPtr() = CHAR_SWAP_MAGIC;
		facing = DIR_SOUTH;
		#ifdef CHAR_SWAP_TRADE_BOX
		StringCopy((u8*) sPokemonBoxNamePtrs[TRADE_BOX_ID], GetLanguageText(gText_CharSwap_TradeBoxName, gText_CharSwap_TradeBoxName_EN, gText_CharSwap_TradeBoxName_ES));
		#endif
		gSaveBlock2->playerGender ^= 1;
		InitFreshCharacter();
	}
	else
	{
		ProcessAllData(MODE_SWAP, &dayStamp, &facing, NULL);
		gSaveBlock2->playerGender ^= 1;
	}

	//Restore the shared settings
	for (i = 0; i < MAX_SHARED && sSharedFlags[i] != 0xFFFF; ++i)
	{
		if (sharedFlags[i])
			FlagSet(sSharedFlags[i]);
		else
			FlagClear(sSharedFlags[i]);
	}
	for (i = 0; i < MAX_SHARED && sSharedVars[i] != 0xFFFF; ++i)
		VarSet(sSharedVars[i], sharedVars[i]);

	//Daily events: the inactive character's daily flags must reset if a day passed
	#if defined(TIME_ENABLED) && defined(FLAG_DAILY_EVENTS_START)
	if (!firstTime && dayStamp != today)
	{
		for (i = FLAG_DAILY_EVENTS_START; i < FLAG_DAILY_EVENTS_START + 0x100; ++i)
			FlagClear(i);
	}
	#endif

	if (*PC_CURRENT_BOX >= TOTAL_BOXES_COUNT)
		*PC_CURRENT_BOX = 0;
	if (VarGet(VAR_PC_BOX_TO_SEND_MON) >= TOTAL_BOXES_COUNT) //Box where caught Pokemon are sent
		VarSet(VAR_PC_BOX_TO_SEND_MON, 0);

	#if defined(CHAR_SWAP_TRADE_BOX) && defined(CHAR_SWAP_SEPARATE_POKEDEX)
	RegisterTradeBoxInPokedex(); //Like a real trade: the new character's Pokedex learns about the Pokemon in the Trade Box
	#endif

	//Both characters always keep the Blue Flute
	if (!CheckBagHasItem(ITEM_BLUE_FLUTE, 1))
		AddBagItem(ITEM_BLUE_FLUTE, 1);
	StoreBagItemCount();

	//Warp to where the new character was (same as the "warp" script command)
	SetWarpDestination(gSaveBlock1->location.mapGroup, gSaveBlock1->location.mapNum, -1, gSaveBlock1->pos.x, gSaveBlock1->pos.y);
	sWarpDestination->x = gSaveBlock1->pos.x; //SetWarpDestination only takes s8 coords, so fix big maps
	sWarpDestination->y = gSaveBlock1->pos.y;
	#ifdef CHAR_SWAP_WHITE_FLASH
	DoWhiteFlashWarp();
	#else
	DoWarp();
	#endif
	ResetInitialPlayerAvatarState();

	//Keep the direction the new character was facing
	if (facing < DIR_SOUTH || facing > DIR_EAST)
		facing = DIR_SOUTH;
	SetInitialPlayerAvatarStateWithDirection(facing);
}

/* ------------- Read-only view of the stored (inactive) character ------------- */

//TRUE if the other character was never used (nothing is stored yet)
bool8 CharSwap_IsOtherCharacterNew(void)
{
	return *GetStorageMagicPtr() != CHAR_SWAP_MAGIC;
}

#define CHAR_SWAP_BADGE_FLAGS_OFFSET (FLAG_BADGE01_GET / 8)
_Static_assert(FLAG_BADGE01_GET % 8 == 0 && CHAR_SWAP_BADGE_FLAGS_OFFSET < SB1_FLAGS_SIZE, "Character Swap: the 8 badge flags must be one byte of the vanilla flags");
_Static_assert(VAR_PLAYER_WALKRUN >= 0x5000 && VAR_PLAYER_WALKRUN < 0x5000 + EXPANDED_VARS_SIZE / 2, "Character Swap: VAR_PLAYER_WALKRUN must be an expanded var");

//Fills 'out' with a few fields of the stored character (name, money, location, badges, Pokedex...).
//Reads through the same ProcessAllData sequence the swap uses, so the offsets can never get out of sync.
//Nothing is written to the live data or to the storage. Returns FALSE (and out->exists = FALSE) if nothing is stored.
bool8 CharSwap_PeekOtherCharacter(struct CharSwapPeek* out)
{
	u32 dayStamp = 0;
	u8 facing = 0;
	struct PeekRequest requests[] =
	{
		{gSaveBlock2->playerName,        0, sizeof(gSaveBlock2->playerName), out->name},
		{&gPlayerPartyCount,             0, sizeof(u8),                      &out->partyCount},
		{&gSaveBlock1->money,            0, sizeof(u32),                     (u8*) &out->moneyRaw},
		{&gSaveBlock1->location,         0, sizeof(struct WarpData),         (u8*) &out->location},
		{SB1_FLAGS,                      CHAR_SWAP_BADGE_FLAGS_OFFSET, 1,    &out->badgeFlags},
		{gExpandedVars,                  (VAR_PLAYER_WALKRUN - 0x5000) * sizeof(u16), sizeof(u16), (u8*) &out->walkSpriteVar},
		#ifdef CHAR_SWAP_SEPARATE_POKEDEX
		{gSaveBlock1->dexCaughtFlags,    0, sizeof(gSaveBlock1->dexCaughtFlags), out->dexCaughtFlags},
		#endif
		{NULL, 0, 0, NULL}, //End of the list
	};

	Memset(out, 0, sizeof(struct CharSwapPeek));
	if (CharSwap_IsOtherCharacterNew())
		return FALSE;

	ProcessAllData(MODE_PEEK, &dayStamp, &facing, requests);
	out->name[PLAYER_NAME_LENGTH] = EOS;
	out->exists = TRUE;
	return TRUE;
}

/* ------------------------------ Hooks ------------------------------ */

//Hooked at 0x809A96C (ItemId_GetDescription): the swap item's description follows the language flags
const u8* ItemId_GetDescription_CharSwap(u16 itemId)
{
	if (itemId == ITEM_BLUE_FLUTE)
		return GetLanguageText(DESC_CHAR_SWAP_FLUTE, DESC_CHAR_SWAP_FLUTE_EN, DESC_CHAR_SWAP_FLUTE_ES);

	return gItems[SanitizeItemId(itemId)].description;
}

//Hooked at 0x8044288 (IsOtherTrainer): Pokemon caught by the other character (same Trainer ID) aren't treated
//as traded, so they obey and don't get the traded Exp. boost when moved through the Trade Box.
static bool8 OTNameMatches(const u8* otName, const u8* playerName)
{
	for (u32 i = 0; otName[i] != EOS; ++i)
	{
		if (otName[i] != playerName[i])
			return FALSE;
	}
	return TRUE;
}

bool8 IsOtherTrainer_CharSwap(u32 otId, u8* otName)
{
	u32 playerId = gSaveBlock2->playerTrainerId[0]
				 | (gSaveBlock2->playerTrainerId[1] << 8)
				 | (gSaveBlock2->playerTrainerId[2] << 16)
				 | (gSaveBlock2->playerTrainerId[3] << 24);

	if (otId != playerId)
		return TRUE;

	if (OTNameMatches(otName, gSaveBlock2->playerName))
		return FALSE;

	if (*GetStorageMagicPtr() == CHAR_SWAP_MAGIC
	&& OTNameMatches(otName, ((u8*) sPokemonBoxPtrs[FIRST_STORAGE_BOX]) + sizeof(u32))) //Stored name of the other character
		return FALSE;

	return TRUE;
}

/* ------------------------- Item field use ------------------------- */

static void Task_CharacterSwapField(u8 taskId)
{
	ScriptContext1_SetupScript(SystemScript_CharacterSwap);
	DestroyTask(taskId);
}

void FieldUseFunc_CharacterSwap(u8 taskId)
{
	sItemUseOnFieldCB = Task_CharacterSwapField;
	SetUpItemUseOnFieldCallback(taskId);
}

#endif //CHARACTER_SWAP_SYSTEM
