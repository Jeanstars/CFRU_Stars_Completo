#include "defines.h"
#include "../include/bg.h"
#include "../include/event_data.h"
#include "../include/event_object_movement.h"
#include "../include/field_player_avatar.h"
#include "../include/gpu_regs.h"
#include "../include/main.h"
#include "../include/malloc.h"
#include "../include/menu.h"
#include "../include/money.h"
#include "../include/overworld.h"
#include "../include/palette.h"
#include "../include/region_map.h"
#include "../include/scanline_effect.h"
#include "../include/sound.h"
#include "../include/sprite.h"
#include "../include/string_util.h"
#include "../include/task.h"
#include "../include/text.h"
#include "../include/text_window.h"
#include "../include/window.h"
#include "../include/constants/event_objects.h"
#include "../include/constants/flags.h"
#include "../include/constants/rgb.h"
#include "../include/constants/songs.h"
#include "../include/new/Vanilla_functions.h"
#include "../include/new/character_swap_screen.h"
#include "../include/new/dns.h"
#include "../include/new/ram_locs.h"

/*
character_swap_screen.c
	Character Swap System: selection screen shown when the Switch (Blue Flute) is used.

	Shows both characters side by side (walking in place, facing down), with the names the player chose.
	  Left/Right = choose | A = switch | B or R = back | L = info about the selected character.
	The actual swap (sound, naming screen, white flash, warp) is still done by character_swap.c:
	this screen only asks which character to use, and returns in LASTRESULT: 1 = switch, 0 = cancel.

	Script: callasm CharSwap_OpenSwapScreen / waitstate / compare LASTRESULT 0x1 (see character_swap.s)

	Texts: Portuguese (flag 0x260), English (0x261) and Spanish (0x262), like character_swap.string.
	Graphics: graphics/CharSwapScreen/bgCharSwapScreen.png (8bpp, up to 240 colours).
	ATTENTION: in this build .bss is in the ROM, so there are NO writable globals here: the state lives in
	the heap (struct CharSwapScreenState) and in the task data.
*/

#ifdef CHARACTER_SWAP_SYSTEM

#define NUM_SLOTS           2                   //Slot 0 = male character (left), slot 1 = female character (right)
#define SPRITE_CENTER_Y     65                  //Centre of the 16x32 overworld sprite (feet on the platform)
#define OW_ANIM_WALK_SOUTH  4                   //ANIM_STD_GO_SOUTH: walking in place, facing down
#define FONT_SMALL          0
#define FONT_NORMAL         2

//Background palette (8bpp): 0-191 fixed art | 192-207 left panel ramp | 208-223 right panel ramp | 224-227 selection rings
#define RAMP_PAL_LEFT       192
#define RAMP_PAL_RIGHT      208
#define RING_PAL            224
#define BG_PAL_BYTES        (240 * 2)

#define RAMP_DARK  RGB(1, 2, 3)

enum
{
	SLOT_ACTIVE,        //The character being played now
	SLOT_AVAILABLE,     //The other character (already used before)
	SLOT_NEW,           //The other character was never used: switching starts a new adventure
};

enum
{
	WIN_TITLE,
	WIN_NAMES,
	WIN_STATUS,
	WIN_DIALOG,
	WIN_HINT,
};

enum
{
	PHASE_CHOOSING,
	PHASE_FADE_OUT,
};

extern const u8 bgCharSwapScreenTiles[];
extern const u8 bgCharSwapScreenMap[];
extern const u8 bgCharSwapScreenPal[];

extern const u8 gText_CharSwap_DefaultMaleName[];
extern const u8 gText_CharSwap_DefaultFemaleName[];

//Each text exists in 3 languages: name (Portuguese), name_EN and name_ES
#define DECLARE_TEXT(sym) extern const u8 sym[]; extern const u8 sym##_EN[]; extern const u8 sym##_ES[];
DECLARE_TEXT(gText_CswUi_Title)
DECLARE_TEXT(gText_CswUi_Ready1)
DECLARE_TEXT(gText_CswUi_Ready2)
DECLARE_TEXT(gText_CswUi_New1)
DECLARE_TEXT(gText_CswUi_New2)
DECLARE_TEXT(gText_CswUi_Active1)
DECLARE_TEXT(gText_CswUi_Active2)
DECLARE_TEXT(gText_CswUi_StatusActive)
DECLARE_TEXT(gText_CswUi_StatusAvailable)
DECLARE_TEXT(gText_CswUi_StatusNew)
DECLARE_TEXT(gText_CswUi_HintLeft)
DECLARE_TEXT(gText_CswUi_HintRight)
DECLARE_TEXT(gText_CswUi_InfoLocation)
DECLARE_TEXT(gText_CswUi_InfoMoney)
DECLARE_TEXT(gText_CswUi_InfoBadges)
DECLARE_TEXT(gText_CswUi_InfoDex)
DECLARE_TEXT(gText_CswUi_InfoParty)
DECLARE_TEXT(gText_CswUi_InfoNoData1)
DECLARE_TEXT(gText_CswUi_InfoNoData2)

//Same rule as GetLanguageText in character_swap.c (language flags)
static const u8* GetLangText(const u8* pt, const u8* en, const u8* es)
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
#define TXT(sym) GetLangText(sym, sym##_EN, sym##_ES)

static const struct BgTemplate sCharSwapScreenBgTemplates[] =
{
	{ .bg = 0, .charBaseIndex = 0, .mapBaseIndex = 31, .screenSize = 0, .paletteMode = 0, .priority = 0, .baseTile = 1 }, //Text (4bpp)
	{ .bg = 1, .charBaseIndex = 1, .mapBaseIndex = 30, .screenSize = 0, .paletteMode = 1, .priority = 2, .baseTile = 0 }, //Art (8bpp)
};

#define WIN_TITLE_W    22
#define WIN_TITLE_H    2
#define WIN_NAMES_W    22
#define WIN_NAMES_H    2
#define WIN_STATUS_W   22
#define WIN_STATUS_H   3
#define WIN_DIALOG_W   20
#define WIN_DIALOG_H   5
#define WIN_HINT_W     30
#define WIN_HINT_H     2

#define WIN_TITLE_BASE   0
#define WIN_NAMES_BASE   (WIN_TITLE_BASE  + WIN_TITLE_W  * WIN_TITLE_H)
#define WIN_STATUS_BASE  (WIN_NAMES_BASE  + WIN_NAMES_W  * WIN_NAMES_H)
#define WIN_DIALOG_BASE  (WIN_STATUS_BASE + WIN_STATUS_W * WIN_STATUS_H)
#define WIN_HINT_BASE    (WIN_DIALOG_BASE + WIN_DIALOG_W * WIN_DIALOG_H)

//Pixel positions: window left edge (x = tile * 8) and the centre of each panel
#define WIN_TITLE_X    32
#define WIN_NAMES_X    32
#define WIN_STATUS_X   32
#define WIN_DIALOG_X   40
static const s16 sPanelCenterX[NUM_SLOTS] = {76, 163};

static const struct WindowTemplate sCharSwapScreenWindowTemplates[] =
{
	{ .bg = 0, .tilemapLeft = 4,  .tilemapTop = 1,  .width = WIN_TITLE_W,  .height = WIN_TITLE_H,  .paletteNum = 15, .baseBlock = WIN_TITLE_BASE },
	{ .bg = 0, .tilemapLeft = 4,  .tilemapTop = 3,  .width = WIN_NAMES_W,  .height = WIN_NAMES_H,  .paletteNum = 15, .baseBlock = WIN_NAMES_BASE },
	{ .bg = 0, .tilemapLeft = 4,  .tilemapTop = 9,  .width = WIN_STATUS_W, .height = WIN_STATUS_H, .paletteNum = 15, .baseBlock = WIN_STATUS_BASE },
	{ .bg = 0, .tilemapLeft = 5,  .tilemapTop = 12, .width = WIN_DIALOG_W, .height = WIN_DIALOG_H, .paletteNum = 15, .baseBlock = WIN_DIALOG_BASE },
	{ .bg = 0, .tilemapLeft = 0,  .tilemapTop = 18, .width = WIN_HINT_W,   .height = WIN_HINT_H,   .paletteNum = 15, .baseBlock = WIN_HINT_BASE },
	DUMMY_WIN_TEMPLATE
};

//Text colours: {background, foreground, shadow}
static const u8 sTextWhite[3] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_WHITE,     TEXT_COLOR_DARK_GREY};
static const u8 sTextDark[3]  = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_DARK_GREY, TEXT_COLOR_LIGHT_GREY};
static const u16 sTextBlackColor = RGB(2, 2, 4); //Replaces "dark grey" so the dark text is crisp

//Base colour of each panel (left = blue, right = green)
static const u16 sPanelBaseColors[NUM_SLOTS] = {RGB(9, 16, 25), RGB(6, 16, 14)};

//Selection ring glow: {outer, inner} for 4 animation steps
static const u16 sRingGlow[4][2] =
{
	{RGB(23, 31, 7),  RGB(15, 28, 5)},
	{RGB(26, 31, 12), RGB(19, 30, 8)},
	{RGB(29, 31, 19), RGB(22, 31, 13)},
	{RGB(26, 31, 12), RGB(19, 30, 8)},
};
static const u16 sRingIdle[2] = {RGB(7, 10, 15), RGB(5, 7, 12)};

struct SlotInfo
{
	u8 state;                           //SLOT_ACTIVE / SLOT_AVAILABLE / SLOT_NEW
	u8 name[PLAYER_NAME_LENGTH + 1];    //Name chosen by the player
	u16 graphicsId;                     //Overworld sprite
	s8 mapGroup;
	s8 mapNum;
	u32 money;
	u8 badges;
	u16 dexCaught;
	u8 partyCount;
};

struct CharSwapScreenState
{
	u16 tilemapBuffer[0x400];
	struct CharSwapPeek other;
	struct SlotInfo slots[NUM_SLOTS];
	u8 spriteIds[NUM_SLOTS];
	u8 activeSlot;
};

//Task data
#define tStep       data[0]
#define tGfxStep    data[1]
#define tCursor     data[2]
#define tInfo       data[3]
#define tFrame      data[4]
#define tPhase      data[5]
#define tResult     data[6]
#define tStatePtrLo data[14]
#define tStatePtrHi data[15]

static void CB2_CharSwapScreenInit(void);
static void CB2_CharSwapScreen(void);
static void VBlankCB_CharSwapScreen(void);
static void Task_CharSwapScreenFadeOutField(u8 taskId);
static void Task_CharSwapScreenInit(u8 taskId);
static void Task_CharSwapScreenMain(u8 taskId);
static void Task_CharSwapScreenExit(u8 taskId);

static struct CharSwapScreenState* GetState(u8 taskId)
{
	struct Task* task = &gTasks[taskId];
	u32 ptr = ((u32)(u16) task->tStatePtrHi << 16) | (u32)(u16) task->tStatePtrLo;
	return (struct CharSwapScreenState*) ptr;
}

static void SetState(u8 taskId, struct CharSwapScreenState* state)
{
	u32 ptr = (u32) state;
	gTasks[taskId].tStatePtrLo = (s16)(ptr & 0xFFFF);
	gTasks[taskId].tStatePtrHi = (s16)(ptr >> 16);
}

/* ------------------------------------------------------------------ */
/* Data of each character                                              */
/* ------------------------------------------------------------------ */

//Number of bits set (Pokedex entries, badges)
static u16 CountDexBits(const u8* data, u32 size)
{
	u32 count = 0;
	for (u32 i = 0; i < size; ++i)
	{
		u8 b = data[i];
		while (b != 0)
		{
			count += b & 1;
			b >>= 1;
		}
	}
	return count;
}

static void CopyName(u8* dest, const u8* src)
{
	u32 i;
	for (i = 0; i < PLAYER_NAME_LENGTH && src[i] != EOS; ++i)
		dest[i] = src[i];
	dest[i] = EOS;
}

static void FillActiveSlot(struct SlotInfo* slot, u8 gender)
{
	slot->state = SLOT_ACTIVE;
	CopyName(slot->name, gSaveBlock2->playerName);
	slot->graphicsId = GetPlayerAvatarGraphicsIdByStateIdAndGender(PLAYER_AVATAR_STATE_NORMAL, gender); //Includes the customised sprite
	slot->mapGroup = gSaveBlock1->location.mapGroup;
	slot->mapNum = gSaveBlock1->location.mapNum;
	slot->money = GetMoney(&gSaveBlock1->money);
	slot->badges = 0;
	for (u32 i = 0; i < 8; ++i)
		slot->badges += FlagGet(FLAG_BADGE01_GET + i) ? 1 : 0;
	slot->dexCaught = CountDexBits(gSaveBlock1->dexCaughtFlags, sizeof(gSaveBlock1->dexCaughtFlags));
	slot->partyCount = gPlayerPartyCount;
}

static void FillOtherSlot(struct SlotInfo* slot, const struct CharSwapPeek* other, u8 gender)
{
	if (!other->exists)
	{
		//Never used: the name is the default one (the player chooses the real name after confirming)
		slot->state = SLOT_NEW;
		CopyName(slot->name, gender == MALE ? gText_CharSwap_DefaultMaleName : gText_CharSwap_DefaultFemaleName);
		slot->graphicsId = (gender == MALE) ? EVENT_OBJ_GFX_RED_NORMAL : EVENT_OBJ_GFX_LEAF_NORMAL;
		return;
	}

	slot->state = SLOT_AVAILABLE;
	CopyName(slot->name, other->name);
	slot->graphicsId = (other->walkSpriteVar != 0) ? other->walkSpriteVar //Sprite customised by that character
					 : (gender == MALE) ? EVENT_OBJ_GFX_RED_NORMAL : EVENT_OBJ_GFX_LEAF_NORMAL;
	slot->mapGroup = other->location.mapGroup;
	slot->mapNum = other->location.mapNum;
	slot->money = GetMoney((u32*) &other->moneyRaw);
	slot->badges = CountDexBits(&other->badgeFlags, 1);
	#ifdef CHAR_SWAP_SEPARATE_POKEDEX
	slot->dexCaught = CountDexBits(other->dexCaughtFlags, sizeof(other->dexCaughtFlags));
	#else
	slot->dexCaught = CountDexBits(gSaveBlock1->dexCaughtFlags, sizeof(gSaveBlock1->dexCaughtFlags)); //Shared Pokedex
	#endif
	slot->partyCount = other->partyCount;
}

static void LoadSlots(struct CharSwapScreenState* state)
{
	u8 activeGender = gSaveBlock2->playerGender;
	u8 otherGender = activeGender ^ 1;

	state->activeSlot = activeGender;
	CharSwap_PeekOtherCharacter(&state->other); //Read-only
	FillActiveSlot(&state->slots[activeGender], activeGender);
	FillOtherSlot(&state->slots[otherGender], &state->other, otherGender);
}

/* ------------------------------------------------------------------ */
/* Init                                                                */
/* ------------------------------------------------------------------ */

//callasm CharSwap_OpenSwapScreen + waitstate
void CharSwap_OpenSwapScreen(void)
{
	gSpecialVar_LastResult = 0;
	BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
	CreateTask(Task_CharSwapScreenFadeOutField, 0);
}

static void Task_CharSwapScreenFadeOutField(u8 taskId)
{
	if (!gPaletteFade->active)
	{
		DestroyTask(taskId);
		SetMainCallback2(CB2_CharSwapScreenInit);
	}
}

static void CB2_CharSwapScreenInit(void)
{
	ResetSpriteData();
	ResetPaletteFade();
	FreeAllSpritePalettes();
	ResetTasks();
	ScanlineEffect_Stop();
	CreateTask(Task_CharSwapScreenInit, 0);
	SetMainCallback2(CB2_CharSwapScreen);
}

static void CB2_CharSwapScreen(void)
{
	RunTasks();
	AnimateSprites();
	BuildOamBuffer();
	UpdatePaletteFade();
}

static void VBlankCB_CharSwapScreen(void)
{
	LoadOam();
	ProcessSpriteCopyRequests();
	TransferPlttBuffer();
}

static void ResetGpuForCharSwapScreen(void)
{
	void *vram = (void *)VRAM;
	DmaClearLarge16(3, vram, VRAM_SIZE, 0x1000);
	DmaClear32(3, (void *)OAM, OAM_SIZE);
	DmaClear16(3, (void *)PLTT, PLTT_SIZE);
	SetGpuReg(REG_OFFSET_DISPCNT, 0);
	SetGpuReg(REG_OFFSET_BG0CNT, 0);
	SetGpuReg(REG_OFFSET_BG0HOFS, 0);
	SetGpuReg(REG_OFFSET_BG0VOFS, 0);
	SetGpuReg(REG_OFFSET_BG1CNT, 0);
	SetGpuReg(REG_OFFSET_BG1HOFS, 0);
	SetGpuReg(REG_OFFSET_BG1VOFS, 0);
	SetGpuReg(REG_OFFSET_BG2CNT, 0);
	SetGpuReg(REG_OFFSET_BG2HOFS, 0);
	SetGpuReg(REG_OFFSET_BG2VOFS, 0);
	SetGpuReg(REG_OFFSET_BG3CNT, 0);
	SetGpuReg(REG_OFFSET_BG3HOFS, 0);
	SetGpuReg(REG_OFFSET_BG3VOFS, 0);
	SetGpuReg(REG_OFFSET_WIN0H, 0);
	SetGpuReg(REG_OFFSET_WIN0V, 0);
	SetGpuReg(REG_OFFSET_WININ, 0);
	SetGpuReg(REG_OFFSET_WINOUT, 0);
	SetGpuReg(REG_OFFSET_BLDCNT, 0);
	SetGpuReg(REG_OFFSET_BLDALPHA, 0);
	SetGpuReg(REG_OFFSET_BLDY, 0);
}

static void CharSwapScreen_ResetBgPositions(void)
{
	for (u8 i = 0; i < 4; ++i)
	{
		ChangeBgX(i, 0, 0);
		ChangeBgY(i, 0, 0);
	}
}

static void SetupBgsAndWindows(struct CharSwapScreenState* state)
{
	ResetGpuForCharSwapScreen();
	ResetBgsAndClearDma3BusyFlags(0);
	InitBgsFromTemplates(0, sCharSwapScreenBgTemplates, 2);
	CharSwapScreen_ResetBgPositions();
	InitWindows(sCharSwapScreenWindowTemplates);
	DeactivateAllTextPrinters();
	SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_1D_MAP | DISPCNT_OBJ_ON);
	SetBgTilemapBuffer(1, state->tilemapBuffer);
	ShowBg(0);
	ShowBg(1);
	FillBgTilemapBufferRect_Palette0(0, 0, 0, 0, 30, 20);
	FillBgTilemapBufferRect_Palette0(1, 0, 0, 0, 30, 20);
}

static bool8 LoadCharSwapScreenGfx(u8 taskId)
{
	struct Task* task = &gTasks[taskId];

	switch (task->tGfxStep)
	{
	case 0:
		ResetTempTileDataBuffers();
		break;
	case 1:
		DecompressAndCopyTileDataToVram(1, bgCharSwapScreenTiles, 0, 0, 0);
		break;
	case 2:
		if (FreeTempTileDataBuffersIfPossible() == 1) //TRUE while the copy is still busy
			return FALSE;
		break;
	case 3:
		LoadCompressedPalette(bgCharSwapScreenPal, 0, BG_PAL_BYTES);        //240 colours (8bpp)
		LoadPalette(stdpal_get(0), 0xF0, 0x20);                             //Text palette (BG0)
		LoadPalette(&sTextBlackColor, 0xF0 + TEXT_COLOR_DARK_GREY, 2);      //Crisp dark text
		break;
	default:
		return TRUE;
	}
	task->tGfxStep++;
	return FALSE;
}

/* ------------------------------------------------------------------ */
/* Colours                                                             */
/* ------------------------------------------------------------------ */

//t = 0..32
static u16 Mix15(u16 a, u16 b, u8 t)
{
	u16 r  = (a & 31)          + ((((b & 31)          - (a & 31))          * t) >> 5);
	u16 g  = ((a >> 5) & 31)   + (((((b >> 5) & 31)   - ((a >> 5) & 31))   * t) >> 5);
	u16 bl = ((a >> 10) & 31)  + (((((b >> 10) & 31)  - ((a >> 10) & 31))  * t) >> 5);
	return RGB(r & 31, g & 31, bl & 31);
}

//Same formula as tools/gerar_fundo (gen_swap_bg.py). 0 = name band | 1 = band line | 2-9 = body | 10-11 = status strip | 12 = platform
static void BuildPanelRamp(u16* out, u16 baseColor, bool8 selected)
{
	u16 dark = RAMP_DARK;
	u16 b = selected ? baseColor : Mix15(dark, baseColor, 28); //Not selected: slightly darker
	u8 k;

	out[0] = Mix15(dark, b, 22);
	out[1] = Mix15(dark, b, 28);
	for (k = 2; k <= 9; ++k)
		out[k] = Mix15(Mix15(dark, b, 28), Mix15(dark, b, 32), ((k - 2) * 32) / 7);
	out[10] = Mix15(dark, b, 18);
	out[11] = Mix15(dark, b, 12);
	out[12] = Mix15(b, RGB(31, 31, 31), 12);
	out[13] = out[14] = out[15] = b;
}

static void UpdatePanelPalettes(u8 cursor, u8 glowStep)
{
	u16 ramp[16];

	for (u8 slot = 0; slot < NUM_SLOTS; ++slot)
	{
		u8 selected = (slot == cursor);
		u16 ring[2];

		BuildPanelRamp(ramp, sPanelBaseColors[slot], selected);
		LoadPalette(ramp, slot == 0 ? RAMP_PAL_LEFT : RAMP_PAL_RIGHT, 32);

		ring[0] = selected ? sRingGlow[glowStep & 3][0] : sRingIdle[0];
		ring[1] = selected ? sRingGlow[glowStep & 3][1] : sRingIdle[1];
		LoadPalette(ring, RING_PAL + slot * 2, 4);
	}
}

/* ------------------------------------------------------------------ */
/* Text                                                                */
/* ------------------------------------------------------------------ */

static s16 CenterX(u8 font, const u8* str, s16 centerX)
{
	s16 x = centerX - (GetStringWidth(font, str, 0) / 2);
	return x < 0 ? 0 : x;
}

//Removes letters from the end until the text fits in maxWidth pixels (so a long translation never leaves the box)
static void TrimToWidth(u8* str, u8 font, u8 maxWidth)
{
	u16 len = StringLength(str);

	while (len > 1 && GetStringWidth(font, str, 0) > maxWidth)
		str[--len] = EOS;
}

//Prints with the normal font if it fits in maxWidth, otherwise with the small one (long translations)
//'str' must be a writable buffer: if it does not fit even with the small font, the end is cut.
static void PrintFit(u8 windowId, u8* str, s16 x, u8 y, u8 maxWidth, const u8* color)
{
	u8 font = FONT_NORMAL;
	u8 yOffset = 0;

	if (GetStringWidth(font, str, 0) > maxWidth)
	{
		font = FONT_SMALL;
		yOffset = 2;
		TrimToWidth(str, font, maxWidth);
	}
	AddTextPrinterParameterized3(windowId, font, x, y + yOffset, color, 0, str);
}

static void PrintTitle(void)
{
	const u8* title = TXT(gText_CswUi_Title);

	FillWindowPixelBuffer(WIN_TITLE, 0);
	AddTextPrinterParameterized3(WIN_TITLE, FONT_SMALL, CenterX(FONT_SMALL, title, (WIN_TITLE_W * 8) / 2), 0, sTextDark, 0, title);
	PutWindowTilemap(WIN_TITLE);
	CopyWindowToVram(WIN_TITLE, 3);
}

static void PrintNamesAndStatus(struct CharSwapScreenState* state)
{
	FillWindowPixelBuffer(WIN_NAMES, 0);
	FillWindowPixelBuffer(WIN_STATUS, 0);

	for (u8 slot = 0; slot < NUM_SLOTS; ++slot)
	{
		const u8* status;
		s16 centerX = sPanelCenterX[slot] - WIN_NAMES_X;

		//Name chosen by the player
		AddTextPrinterParameterized3(WIN_NAMES, FONT_SMALL, CenterX(FONT_SMALL, state->slots[slot].name, centerX), 2, sTextWhite, 0, state->slots[slot].name);

		switch (state->slots[slot].state)
		{
			case SLOT_ACTIVE:    status = TXT(gText_CswUi_StatusActive); break;
			case SLOT_NEW:       status = TXT(gText_CswUi_StatusNew); break;
			default:             status = TXT(gText_CswUi_StatusAvailable); break;
		}
		AddTextPrinterParameterized3(WIN_STATUS, FONT_SMALL, CenterX(FONT_SMALL, status, sPanelCenterX[slot] - WIN_STATUS_X), 7, sTextWhite, 0, status);
	}

	PutWindowTilemap(WIN_NAMES);
	CopyWindowToVram(WIN_NAMES, 3);
	PutWindowTilemap(WIN_STATUS);
	CopyWindowToVram(WIN_STATUS, 3);
}

static void PrintHint(void)
{
	const u8* left = TXT(gText_CswUi_HintLeft);
	const u8* right = TXT(gText_CswUi_HintRight);

	FillWindowPixelBuffer(WIN_HINT, 0);
	AddTextPrinterParameterized3(WIN_HINT, FONT_SMALL, 6, 1, sTextWhite, 0, left);
	AddTextPrinterParameterized3(WIN_HINT, FONT_SMALL, (WIN_HINT_W * 8) - 6 - GetStringWidth(FONT_SMALL, right, 0), 1, sTextWhite, 0, right);
	PutWindowTilemap(WIN_HINT);
	CopyWindowToVram(WIN_HINT, 3);
}

//Two lines of text inside the dialog box (the second one may use [BUFFER1] = character name)
static void PrintDialogLines(const u8* line1, const u8* line2, const u8* name)
{
	StringCopy(gStringVar1, name);

	StringExpandPlaceholders(gStringVar4, line1);
	PrintFit(WIN_DIALOG, gStringVar4, 4, 4, (WIN_DIALOG_W * 8) - 8, sTextDark);
	StringExpandPlaceholders(gStringVar4, line2);
	PrintFit(WIN_DIALOG, gStringVar4, 4, 20, (WIN_DIALOG_W * 8) - 8, sTextDark);
}

static void PrintInfoLine(const u8* label, const u8* value, s16 x, u8 y)
{
	AddTextPrinterParameterized3(WIN_DIALOG, FONT_SMALL, x, y, sTextDark, 0, label);
	AddTextPrinterParameterized3(WIN_DIALOG, FONT_SMALL, x + GetStringWidth(FONT_SMALL, label, 0), y, sTextDark, 0, value);
}

static void PrintInfo(const struct SlotInfo* slot)
{
	const struct MapHeader* header;

	if (slot->state == SLOT_NEW)
	{
		AddTextPrinterParameterized3(WIN_DIALOG, FONT_SMALL, 4, 8,  sTextDark, 0, TXT(gText_CswUi_InfoNoData1));
		AddTextPrinterParameterized3(WIN_DIALOG, FONT_SMALL, 4, 22, sTextDark, 0, TXT(gText_CswUi_InfoNoData2));
		return;
	}

	//Line 1: location
	header = Overworld_GetMapHeaderByGroupAndId(slot->mapGroup, slot->mapNum);
	GetMapName(gStringVar2, header->regionMapSectionId, 0);
	TrimToWidth(gStringVar2, FONT_SMALL, (WIN_DIALOG_W * 8) - 8 - GetStringWidth(FONT_SMALL, TXT(gText_CswUi_InfoLocation), 0));
	PrintInfoLine(TXT(gText_CswUi_InfoLocation), gStringVar2, 4, 3);

	//Line 2: money and badges
	ConvertIntToDecimalStringN(gStringVar2, slot->money, STR_CONV_MODE_LEFT_ALIGN, 7);
	PrintInfoLine(TXT(gText_CswUi_InfoMoney), gStringVar2, 4, 14);
	ConvertIntToDecimalStringN(gStringVar2, slot->badges, STR_CONV_MODE_LEFT_ALIGN, 1);
	PrintInfoLine(TXT(gText_CswUi_InfoBadges), gStringVar2, 92, 14);

	//Line 3: Pokedex and party
	ConvertIntToDecimalStringN(gStringVar2, slot->dexCaught, STR_CONV_MODE_LEFT_ALIGN, 4);
	PrintInfoLine(TXT(gText_CswUi_InfoDex), gStringVar2, 4, 25);
	ConvertIntToDecimalStringN(gStringVar2, slot->partyCount, STR_CONV_MODE_LEFT_ALIGN, 1);
	PrintInfoLine(TXT(gText_CswUi_InfoParty), gStringVar2, 92, 25);
}

//Message (or info) in the dialog box for the selected character
static void PrintDialog(struct CharSwapScreenState* state, u8 cursor, u8 showInfo)
{
	const struct SlotInfo* slot = &state->slots[cursor];

	FillWindowPixelBuffer(WIN_DIALOG, 0);

	if (showInfo)
		PrintInfo(slot);
	else switch (slot->state)
	{
		case SLOT_ACTIVE:
			PrintDialogLines(TXT(gText_CswUi_Active1), TXT(gText_CswUi_Active2), slot->name);
			break;
		case SLOT_NEW:
			PrintDialogLines(TXT(gText_CswUi_New1), TXT(gText_CswUi_New2), slot->name);
			break;
		default:
			PrintDialogLines(TXT(gText_CswUi_Ready1), TXT(gText_CswUi_Ready2), slot->name);
			break;
	}

	PutWindowTilemap(WIN_DIALOG);
	CopyWindowToVram(WIN_DIALOG, 3);
}

/* ------------------------------------------------------------------ */
/* Sprites                                                             */
/* ------------------------------------------------------------------ */

static void CreateSlotSprites(struct CharSwapScreenState* state)
{
	for (u8 slot = 0; slot < NUM_SLOTS; ++slot)
	{
		u8 spriteId = AddPseudoEventObject(state->slots[slot].graphicsId, SpriteCallbackDummy, sPanelCenterX[slot], SPRITE_CENTER_Y, 0);

		if (spriteId >= MAX_SPRITES)
		{
			state->spriteIds[slot] = MAX_SPRITES;
			continue;
		}

		gSprites[spriteId].oam.priority = 1;
		StartSpriteAnim(&gSprites[spriteId], OW_ANIM_WALK_SOUTH); //Walking in place, facing down
		state->spriteIds[slot] = spriteId;
	}
}

/* ------------------------------------------------------------------ */
/* Task: init                                                          */
/* ------------------------------------------------------------------ */

static void Task_CharSwapScreenInit(u8 taskId)
{
	struct Task* task = &gTasks[taskId];
	struct CharSwapScreenState* state;

	switch (task->tStep)
	{
	case 0:
		SetVBlankCallback(NULL);
		state = Calloc(sizeof(struct CharSwapScreenState));
		SetState(taskId, state);
		break;
	case 1:
		state = GetState(taskId);
		SetupBgsAndWindows(state);
		break;
	case 2:
		if (!LoadCharSwapScreenGfx(taskId))
			return;
		break;
	case 3:
		state = GetState(taskId);
		CopyToBgTilemapBuffer(1, bgCharSwapScreenMap, 0, 0);
		LoadSlots(state);
		task->tCursor = state->activeSlot ^ 1; //Starts on the character you would switch to
		task->tInfo = FALSE;
		break;
	case 4:
		state = GetState(taskId);
		UpdatePanelPalettes(task->tCursor, 0);
		CreateSlotSprites(state);
		break;
	case 5:
		state = GetState(taskId);
		PrintTitle();
		PrintNamesAndStatus(state);
		PrintDialog(state, task->tCursor, FALSE);
		PrintHint();
		break;
	case 6:
		CopyBgTilemapBufferToVram(0);
		CopyBgTilemapBufferToVram(1);
		break;
	case 7:
		BeginNormalPaletteFade(0xFFFFFFFF, 0, 16, 0, RGB_BLACK);
		break;
	case 8:
		SetVBlankCallback(VBlankCB_CharSwapScreen);
		break;
	default:
		if (gPaletteFade->active)
			return;
		task->tFrame = 0;
		task->tPhase = PHASE_CHOOSING;
		task->func = Task_CharSwapScreenMain;
		return;
	}
	task->tStep++;
}

/* ------------------------------------------------------------------ */
/* Task: main loop and exit                                            */
/* ------------------------------------------------------------------ */

static void StartExit(u8 taskId, u8 result)
{
	struct Task* task = &gTasks[taskId];

	task->tResult = result;
	task->tPhase = PHASE_FADE_OUT;
	BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
	task->func = Task_CharSwapScreenExit;
}

static void MoveCursor(u8 taskId, u8 newCursor)
{
	struct Task* task = &gTasks[taskId];
	struct CharSwapScreenState* state = GetState(taskId);

	if (task->tCursor == newCursor)
		return;

	task->tCursor = newCursor;
	PlaySE(SE_SELECT);
	UpdatePanelPalettes(newCursor, task->tFrame / 6);
	PrintDialog(state, newCursor, task->tInfo);
}

static void Task_CharSwapScreenMain(u8 taskId)
{
	struct Task* task = &gTasks[taskId];
	struct CharSwapScreenState* state = GetState(taskId);

	if (gPaletteFade->active)
		return;

	//Selection ring glow
	if (++task->tFrame >= 24)
		task->tFrame = 0;
	if (task->tFrame % 6 == 0)
		UpdatePanelPalettes(task->tCursor, task->tFrame / 6);

	if (JOY_NEW(DPAD_LEFT))
		MoveCursor(taskId, 0);
	else if (JOY_NEW(DPAD_RIGHT))
		MoveCursor(taskId, 1);
	else if (JOY_NEW(L_BUTTON))
	{
		PlaySE(SE_SELECT);
		task->tInfo ^= 1;
		PrintDialog(state, task->tCursor, task->tInfo);
	}
	else if (JOY_NEW(A_BUTTON))
	{
		if (task->tCursor == state->activeSlot)
			PlaySE(SE_WALL_HIT); //That character is already being played
		else
		{
			PlaySE(SE_SELECT);
			StartExit(taskId, TRUE);
		}
	}
	else if (JOY_NEW(B_BUTTON) || JOY_NEW(R_BUTTON))
	{
		PlaySE(SE_SELECT);
		StartExit(taskId, FALSE);
	}
}

static void Task_CharSwapScreenExit(u8 taskId)
{
	struct Task* task = &gTasks[taskId];
	struct CharSwapScreenState* state = GetState(taskId);

	if (gPaletteFade->active)
		return;

	gSpecialVar_LastResult = task->tResult;

	for (u8 slot = 0; slot < NUM_SLOTS; ++slot)
	{
		if (state->spriteIds[slot] < MAX_SPRITES)
			DestroySprite(&gSprites[state->spriteIds[slot]]);
	}

	FreeAllWindowBuffers();
	Free(state);
	DestroyTask(taskId);
	SetMainCallback2(CB2_ReturnToFieldContinueScript);
}

#endif //CHARACTER_SWAP_SYSTEM
