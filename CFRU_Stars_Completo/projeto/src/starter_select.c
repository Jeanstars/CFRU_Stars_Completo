/********\

CUSTOM FILE - STARTER SELECTION SCREEN  (v3)
Tela de selecao de Pokemon inicial (3 opcoes, com sprites).
- Fundo em 8bpp (ate 240 cores); pilares/plataformas coloridos pelo TIPO de cada Pokemon.
- Todos os Pokemon entregues por esta tela tem IVs perfeitos (31 em tudo) - veja STARTER_IV.

Chamada pelo script com:  special 0xF0  +  waitstate

ENTRADAS (setvar antes do special):
	Var8000 = especie da opcao 1 (esquerda)
	Var8001 = especie da opcao 2 (meio)
	Var8002 = especie da opcao 3 (direita)
	Var8003 = level dos 3 Pokemon       (0 = usa 5)
	Var8004 = item segurado             (0 = nenhum)
	Var8005 = shiny?                    (1 = os 3 sao shiny, 0 = normal)
	Var8006 = nao entregar?             (1 = so escolhe, nao entrega o mon)

SAIDAS (disponiveis no script depois do waitstate):
	LASTRESULT (0x800D) = indice escolhido (0, 1 ou 2)
	Var8006             = especie escolhida
	Var8007             = onde o mon foi parar (0 = party, 1 = PC, 2 = sem espaco/nao entregue)

\********/

#include "../include/global.h"
#include "../include/pokemon.h"
#include "../include/bg.h"
#include "../include/window.h"
#include "../include/sprite.h"
#include "../include/palette.h"
#include "../include/task.h"
#include "../include/scanline_effect.h"
#include "../include/main.h"
#include "../include/new/Vanilla_functions.h"
#include "../include/new/ram_locs.h"
#include "../include/event_data.h"
#include "../include/gpu_regs.h"
#include "../include/overworld.h"
#include "../include/malloc.h"
#include "../include/sound.h"
#include "../include/string_util.h"
#include "../include/menu.h"
#include "../include/text.h"
#include "../include/trainer_pokemon_sprites.h"
#include "../include/constants/species.h"
#include "../include/constants/rgb.h"
#include "../include/constants/pokemon.h"
#include "../include/new/build_pokemon.h"
#include "../include/new/dns.h"
#include "../include/new/util.h"
#include "../include/new/ability_util.h"
#include "../include/new/rom_locs.h"
#include "../include/data2.h"
#include "../include/pokemon_summary_screen.h"
#include "../include/text_window.h"

extern u8 GiveMonToPlayer(struct Pokemon* mon);
extern const u8 gTypeNames[][TYPE_NAME_LENGTH + 1];

#define NUM_STARTERS        3
#define DEFAULT_LEVEL       5
#define STARTER_IV          31      //31 = IVs perfeitos (32 = IVs aleatorios)

//Posicao dos sprites (centro, em pixels)
#define PIC_Y               52
static const s16 sPicX[NUM_STARTERS] = {50, 120, 190};

//Paleta do fundo (8bpp): 0-191 = arte fixa, 192-239 = 3 rampas de 16 cores (uma por pilar)
#define RAMP_PAL_BASE       192
#define BG_PAL_BYTES        (240 * 2)

#define FONT_SMALL          0

//Texto do nome dentro da tag (tag = 68px de largura, centrada no pilar; a bolinha ocupa a esquerda)
#define TAG_TEXT_Y            2      //posicao vertical do texto (centraliza o nome na altura da tag)
#define TAG_TEXT_CENTER_DX    4      //centro do texto em relacao ao centro do pilar
#define TAG_TEXT_LEFT_LIMIT   (-16)  //o nome nunca comeca antes disso (4px depois da bolinha)
#define TAG_TEXT_RIGHT_LIMIT  30     //o nome nunca termina depois disso (4px antes da borda direita)
#define TAG_TEXT_MAX_WIDTH    (TAG_TEXT_RIGHT_LIMIT - TAG_TEXT_LEFT_LIMIT) //nomes mais largos sao encurtados

extern const u8 bgStarterSelectTiles[];
extern const u8 bgStarterSelectMap[];
extern const u8 bgStarterSelectPal[];

extern const u8 gText_StarterSelect_Msg1[];
extern const u8 gText_StarterSelect_Msg2[];
extern const u8 gText_StarterSelect_Hint[];
extern const u8 gText_StarterSelect_ConfirmMsg1[];
extern const u8 gText_StarterSelect_ConfirmMsg2[];
extern const u8 gText_StarterSelect_ConfirmKeys[];
extern const u8 gText_StarterSelect_Lv[];
extern const u8 gText_StarterSelect_Type[];
extern const u8 gText_StarterSelect_Ability[];
extern const u8 gText_StarterSelect_Nature[];
extern const u8 gText_StarterSelect_Ivs[];
extern const u8 gText_StarterSelect_IvsPerfect[];
extern const u8 gText_StarterSelect_Slash[];

enum
{
	WIN_NAMES,
	WIN_DIALOG,
	WIN_INFO,
};

enum
{
	PHASE_CHOOSING,
	PHASE_CONFIRM,
	PHASE_FADE_OUT,
};

//BG0 = texto (4bpp, charblock 0).  BG1 = arte (8bpp, charblock 1; mapa no screenblock 30).
static const struct BgTemplate sStarterSelectBgTemplates[] =
{
	{
		.bg = 0,
		.charBaseIndex = 0,
		.mapBaseIndex = 31,
		.screenSize = 0,
		.paletteMode = 0,
		.priority = 0,
		.baseTile = 1,
	},
	{
		.bg = 1,
		.charBaseIndex = 1,
		.mapBaseIndex = 30,
		.screenSize = 0,
		.paletteMode = 1,
		.priority = 2,
		.baseTile = 0,
	}
};

#define WIN_NAMES_W   30
#define WIN_NAMES_H   2
#define WIN_DIALOG_W  13
#define WIN_DIALOG_H  6
#define WIN_INFO_W    14
#define WIN_INFO_H    6

#define WIN_NAMES_BASE   0
#define WIN_DIALOG_BASE  (WIN_NAMES_BASE  + WIN_NAMES_W  * WIN_NAMES_H)
#define WIN_INFO_BASE    (WIN_DIALOG_BASE + WIN_DIALOG_W * WIN_DIALOG_H)

static const struct WindowTemplate sStarterSelectWindowTemplates[] =
{
	{ .bg = 0, .tilemapLeft = 0,  .tilemapTop = 12, .width = WIN_NAMES_W,  .height = WIN_NAMES_H,  .paletteNum = 15, .baseBlock = WIN_NAMES_BASE },
	{ .bg = 0, .tilemapLeft = 1,  .tilemapTop = 14, .width = WIN_DIALOG_W, .height = WIN_DIALOG_H, .paletteNum = 15, .baseBlock = WIN_DIALOG_BASE },
	{ .bg = 0, .tilemapLeft = 15, .tilemapTop = 14, .width = WIN_INFO_W,   .height = WIN_INFO_H,   .paletteNum = 15, .baseBlock = WIN_INFO_BASE },
	DUMMY_WIN_TEMPLATE
};

static const u8 sTextWhite[3]   = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_WHITE,      TEXT_COLOR_DARK_GREY};
static const u8 sTextDark[3]    = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_DARK_GREY,  TEXT_COLOR_LIGHT_GREY};
static const u8 sTextLight[3]   = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_LIGHT_GREY, TEXT_COLOR_DARK_GREY};
static const u8 sTextOrange[3]  = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_RED,        TEXT_COLOR_DARK_GREY};
static const u8 sTextGreen[3]   = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_GREEN,      TEXT_COLOR_DARK_GREY};

//Cor base de cada tipo (usada nos pilares/plataformas/tags)
static const u16 sTypeColors[NUMBER_OF_MON_TYPES] =
{
	[TYPE_NORMAL]   = RGB(22, 21, 17),
	[TYPE_FIGHTING] = RGB(24, 8, 6),
	[TYPE_FLYING]   = RGB(17, 18, 29),
	[TYPE_POISON]   = RGB(21, 9, 23),
	[TYPE_GROUND]   = RGB(26, 22, 11),
	[TYPE_ROCK]     = RGB(20, 18, 10),
	[TYPE_BUG]      = RGB(17, 23, 5),
	[TYPE_GHOST]    = RGB(14, 11, 21),
	[TYPE_STEEL]    = RGB(20, 21, 25),
	[TYPE_MYSTERY]  = RGB(14, 18, 16),
	[TYPE_FIRE]     = RGB(30, 13, 4),
	[TYPE_WATER]    = RGB(8, 15, 30),
	[TYPE_GRASS]    = RGB(9, 23, 8),
	[TYPE_ELECTRIC] = RGB(30, 26, 3),
	[TYPE_PSYCHIC]  = RGB(30, 9, 17),
	[TYPE_ICE]      = RGB(12, 25, 27),
	[TYPE_DRAGON]   = RGB(11, 7, 28),
	[TYPE_DARK]     = RGB(12, 9, 8),
	[TYPE_FAIRY]    = RGB(30, 17, 24),
	[TYPE_STELLAR]  = RGB(20, 12, 30),
};

static const u16 sTextOrangeColor = RGB(31, 18, 4);
static const u16 sTextGreenColor  = RGB(9, 27, 9);

//Movimento de "pulinho" do Pokemon selecionado
static const s8 sBobTable[16] = {0, -1, -2, -3, -4, -4, -3, -2, -1, 0, 0, 0, 0, 0, 0, 0};

struct StarterSelectState
{
	u16 tilemapBuffer[0x400];
	struct Pokemon mons[NUM_STARTERS];
	u8 spriteIds[NUM_STARTERS];
};

//Uso do task->data
#define tStep       data[0]
#define tGfxStep    data[1]
#define tCursor     data[2]
#define tLevel      data[3]
#define tNoGive     data[4]
#define tFrame      data[5]
#define tPhase      data[6]
#define tStatePtrLo data[14]
#define tStatePtrHi data[15]

static void CB2_StarterSelectInit(void);
static void CB2_StarterSelect(void);
static void VBlankCB_StarterSelect(void);
static void Task_StarterSelectFadeOutField(u8 taskId);
static void Task_StarterSelectInit(u8 taskId);
static void Task_StarterSelectMain(u8 taskId);
static void Task_StarterSelectExit(u8 taskId);

static struct StarterSelectState* GetState(u8 taskId)
{
	struct Task* task = &gTasks[taskId];
	u32 ptr = ((u32)(u16) task->tStatePtrHi << 16) | (u32)(u16) task->tStatePtrLo;
	return (struct StarterSelectState*) ptr;
}

static void SetState(u8 taskId, struct StarterSelectState* state)
{
	u32 ptr = (u32) state;
	gTasks[taskId].tStatePtrLo = (s16)(ptr & 0xFFFF);
	gTasks[taskId].tStatePtrHi = (s16)(ptr >> 16);
}

static u16 SanitizeSpecies(u16 species, u16 fallback)
{
	if (species == SPECIES_NONE || species >= NUM_SPECIES)
		return fallback;
	return species;
}
//////////////////////////////////////////////////////////////////////
// Entrada: special 0xF0
//////////////////////////////////////////////////////////////////////
void sp0F0_StarterSelect(void)
{
	gSpecialVar_LastResult = 0;
	BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
	CreateTask(Task_StarterSelectFadeOutField, 0);
}

static void Task_StarterSelectFadeOutField(u8 taskId)
{
	if (!gPaletteFade->active)
	{
		DestroyTask(taskId);
		SetMainCallback2(CB2_StarterSelectInit);
	}
}

//////////////////////////////////////////////////////////////////////
// Inicializacao
//////////////////////////////////////////////////////////////////////
static void CB2_StarterSelectInit(void)
{
	ResetSpriteData();
	ResetPaletteFade();
	FreeAllSpritePalettes();
	ResetTasks();
	ScanlineEffect_Stop();
	CreateTask(Task_StarterSelectInit, 0);
	SetMainCallback2(CB2_StarterSelect);
}

static void CB2_StarterSelect(void)
{
	RunTasks();
	AnimateSprites();
	BuildOamBuffer();
	UpdatePaletteFade();
}

static void VBlankCB_StarterSelect(void)
{
	LoadOam();
	ProcessSpriteCopyRequests();
	TransferPlttBuffer();
}

static void ResetGpuForStarterSelect(void)
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

static void StarterSelect_ResetBgPositions(void)
{
	u8 i;
	for (i = 0; i < 4; ++i)
	{
		ChangeBgX(i, 0, 0);
		ChangeBgY(i, 0, 0);
	}
}

static void SetupBgsAndWindows(struct StarterSelectState* state)
{
	ResetGpuForStarterSelect();
	ResetBgsAndClearDma3BusyFlags(0);
	InitBgsFromTemplates(0, sStarterSelectBgTemplates, 2);
	StarterSelect_ResetBgPositions();
	InitWindows(sStarterSelectWindowTemplates);
	DeactivateAllTextPrinters();
	SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_1D_MAP | DISPCNT_OBJ_ON);
	SetBgTilemapBuffer(1, state->tilemapBuffer);
	ShowBg(0);
	ShowBg(1);
	FillBgTilemapBufferRect_Palette0(0, 0, 0, 0, 30, 20);
	FillBgTilemapBufferRect_Palette0(1, 0, 0, 0, 30, 20);
}

static bool8 LoadStarterSelectGfx(u8 taskId)
{
	struct Task* task = &gTasks[taskId];

	switch (task->tGfxStep)
	{
	case 0:
		ResetTempTileDataBuffers();
		break;
	case 1:
		DecompressAndCopyTileDataToVram(1, bgStarterSelectTiles, 0, 0, 0);
		break;
	case 2:
		//Retorna TRUE enquanto a copia ainda esta ocupada, FALSE quando termina.
		if (FreeTempTileDataBuffersIfPossible() == 1)
			return FALSE; //ainda ocupado: espera
		break;
	case 3:
		LoadCompressedPalette(bgStarterSelectPal, 0, BG_PAL_BYTES); //240 cores (8bpp)
		LoadPalette(stdpal_get(0), 0xF0, 0x20);                     //paleta do texto (BG0)
		LoadPalette(&sTextOrangeColor, 0xF0 + TEXT_COLOR_RED, 2);   //vermelho -> laranja
		LoadPalette(&sTextGreenColor,  0xF0 + TEXT_COLOR_GREEN, 2); //verde mais vivo
		break;
	default:
		return TRUE;
	}
	task->tGfxStep++;
	return FALSE;
}

//////////////////////////////////////////////////////////////////////
// Cores dos pilares (rampas de 16 cores geradas a partir do tipo)
//////////////////////////////////////////////////////////////////////
#define RAMP_DARK   RGB(1, 2, 3)

//mistura a->b em t/16
static u16 Mix15(u16 a, u16 b, u8 t)
{
	u16 r = (a & 31)         + ((((b & 31)         - (a & 31))         * t) >> 4);
	u16 g = ((a >> 5) & 31)  + (((((b >> 5) & 31)  - ((a >> 5) & 31))  * t) >> 4);
	u16 bl = ((a >> 10) & 31) + (((((b >> 10) & 31) - ((a >> 10) & 31)) * t) >> 4);
	return RGB(r & 31, g & 31, bl & 31);
}

//Rampa: 0-11 = escuro->cor do tipo | 12,13 = brilhos | 14 = contorno escuro | 15 = canto de selecao
static void BuildRamp(u16* out, u16 typeColor, bool8 selected)
{
	u16 base = selected ? typeColor : Mix15(RAMP_DARK, typeColor, 11);
	u8 k;

	for (k = 0; k < 12; ++k)
		out[k] = Mix15(RAMP_DARK, base, (k * 16) / 11);
	out[12] = Mix15(base, RGB(31, 31, 31), 8);
	out[13] = Mix15(base, RGB(31, 31, 31), 13);
	out[14] = Mix15(base, RGB(0, 0, 0), 10);
	out[15] = selected ? RGB(31, 26, 5) : RGB(7, 11, 17); //dourado se selecionado, quase invisivel se nao
}

static u16 GetSlotTypeColor(struct StarterSelectState* state, u8 slot)
{
	u16 species = GetMonData(&state->mons[slot], MON_DATA_SPECIES, NULL);
	u8 type = gBaseStats[species].type1;
	if (type >= NUMBER_OF_MON_TYPES)
		type = TYPE_NORMAL;
	return sTypeColors[type];
}

static void UpdatePillarPalettes(struct StarterSelectState* state, u8 selected)
{
	u16 ramp[16];
	u8 i;
	for (i = 0; i < NUM_STARTERS; ++i)
	{
		BuildRamp(ramp, GetSlotTypeColor(state, i), i == selected);
		LoadPalette(ramp, RAMP_PAL_BASE + (i * 16), 32);
	}
}

//////////////////////////////////////////////////////////////////////
// Texto
//////////////////////////////////////////////////////////////////////
//a-z -> A-Z (tabela de caracteres do jogo: A = 0xBB, a = 0xD5)
static void UpperCaseString(u8* str)
{
	for (; *str != EOS; ++str)
	{
		if (*str >= 0xD5 && *str <= 0xEE)
			*str -= (0xD5 - 0xBB);
	}
}

//Remove letras do final ate o texto caber em maxWidth pixels (garante que o nome nunca sai da tag)
static void FitStringToWidth(u8* str, u8 font, s16 maxWidth)
{
	u16 len = StringLength(str);
	while (len > 1 && GetStringWidth(font, str, 0) > maxWidth)
		str[--len] = EOS;
}

//Escreve o nome dentro da tag: centralizado, mas sempre dentro dos limites da tag
static void PrintNameInTag(const u8* name, s16 pillarX, const u8* color)
{
	s16 width = GetStringWidth(FONT_SMALL, name, 0);
	s16 x = pillarX + TAG_TEXT_CENTER_DX - (width / 2);

	if (x < pillarX + TAG_TEXT_LEFT_LIMIT)
		x = pillarX + TAG_TEXT_LEFT_LIMIT;
	if (x + width > pillarX + TAG_TEXT_RIGHT_LIMIT)
		x = pillarX + TAG_TEXT_RIGHT_LIMIT - width;
	AddTextPrinterParameterized3(WIN_NAMES, FONT_SMALL, x, TAG_TEXT_Y, color, 0, name);
}

static void PrintNames(struct StarterSelectState* state, u8 selected)
{
	u8 i;
	FillWindowPixelBuffer(WIN_NAMES, 0);
	for (i = 0; i < NUM_STARTERS; ++i)
	{
		u16 species = GetMonData(&state->mons[i], MON_DATA_SPECIES, NULL);
		GetSpeciesName(gStringVar4, species);
		UpperCaseString(gStringVar4);
		FitStringToWidth(gStringVar4, FONT_SMALL, TAG_TEXT_MAX_WIDTH);
		PrintNameInTag(gStringVar4, sPicX[i], (i == selected) ? sTextWhite : sTextLight);
	}
	PutWindowTilemap(WIN_NAMES);
	CopyWindowToVram(WIN_NAMES, 3);
}

static void PrintDialog(bool8 confirming)
{
	FillWindowPixelBuffer(WIN_DIALOG, 0);
	if (!confirming)
	{
		AddTextPrinterParameterized3(WIN_DIALOG, FONT_SMALL, 4, 2,  sTextDark, 0, gText_StarterSelect_Msg1);
		AddTextPrinterParameterized3(WIN_DIALOG, FONT_SMALL, 4, 13, sTextDark, 0, gText_StarterSelect_Msg2);
		AddTextPrinterParameterized3(WIN_DIALOG, FONT_SMALL, 4, 27, sTextDark, 0, gText_StarterSelect_Hint);
	}
	else
	{
		AddTextPrinterParameterized3(WIN_DIALOG, FONT_SMALL, 4, 2,  sTextDark, 0, gText_StarterSelect_ConfirmMsg1);
		AddTextPrinterParameterized3(WIN_DIALOG, FONT_SMALL, 4, 13, sTextDark, 0, gText_StarterSelect_ConfirmMsg2);
		AddTextPrinterParameterized3(WIN_DIALOG, FONT_SMALL, 4, 27, sTextDark, 0, gText_StarterSelect_ConfirmKeys);
	}
	PutWindowTilemap(WIN_DIALOG);
	CopyWindowToVram(WIN_DIALOG, 3);
}

static void PrintInfoLine(const u8* label, const u8* value, u8 y, const u8* valueColor)
{
	s16 w = GetStringWidth(FONT_SMALL, label, 0);
	AddTextPrinterParameterized3(WIN_INFO, FONT_SMALL, 6, y, sTextWhite, 0, label);
	AddTextPrinterParameterized3(WIN_INFO, FONT_SMALL, 6 + w, y, valueColor, 0, value);
}

static bool8 HasPerfectIvs(struct Pokemon* mon)
{
	return GetMonData(mon, MON_DATA_HP_IV, NULL) == 31
		&& GetMonData(mon, MON_DATA_ATK_IV, NULL) == 31
		&& GetMonData(mon, MON_DATA_DEF_IV, NULL) == 31
		&& GetMonData(mon, MON_DATA_SPEED_IV, NULL) == 31
		&& GetMonData(mon, MON_DATA_SPATK_IV, NULL) == 31
		&& GetMonData(mon, MON_DATA_SPDEF_IV, NULL) == 31;
}

static void PrintInfo(struct StarterSelectState* state, u8 selected, u8 level)
{
	struct Pokemon* mon = &state->mons[selected];
	u16 species = GetMonData(mon, MON_DATA_SPECIES, NULL);
	u8 type1 = gBaseStats[species].type1;
	u8 type2 = gBaseStats[species].type2;

	FillWindowPixelBuffer(WIN_INFO, 0);

	//linha 1: LV. 5            IVS: PERFECT
	ConvertIntToDecimalStringN(gStringVar1, level, STR_CONV_MODE_LEFT_ALIGN, 3);
	PrintInfoLine(gText_StarterSelect_Lv, gStringVar1, 1, sTextOrange);
	if (HasPerfectIvs(mon))
	{
		s16 w = GetStringWidth(FONT_SMALL, gText_StarterSelect_Ivs, 0) + GetStringWidth(FONT_SMALL, gText_StarterSelect_IvsPerfect, 0);
		s16 x = (WIN_INFO_W * 8) - 6 - w;
		AddTextPrinterParameterized3(WIN_INFO, FONT_SMALL, x, 1, sTextWhite, 0, gText_StarterSelect_Ivs);
		AddTextPrinterParameterized3(WIN_INFO, FONT_SMALL, x + GetStringWidth(FONT_SMALL, gText_StarterSelect_Ivs, 0), 1, sTextGreen, 0, gText_StarterSelect_IvsPerfect);
	}

	//linha 2: TYPE: FIRE  (ou FIRE/FLYING)
	if (type1 >= NUMBER_OF_MON_TYPES) type1 = TYPE_NORMAL;
	StringCopy(gStringVar2, gTypeNames[type1]);
	UpperCaseString(gStringVar2);
	if (type2 != type1 && type2 < NUMBER_OF_MON_TYPES)
	{
		StringAppend(gStringVar2, gText_StarterSelect_Slash);
		StringCopy(gStringVar3, gTypeNames[type2]);
		UpperCaseString(gStringVar3);
		StringAppend(gStringVar2, gStringVar3);
	}
	PrintInfoLine(gText_StarterSelect_Type, gStringVar2, 10, sTextOrange);

	//linha 3: ABILITY: BLAZE
	StringCopy(gStringVar2, GetAbilityName(GetMonAbility(mon), species));
	UpperCaseString(gStringVar2);
	PrintInfoLine(gText_StarterSelect_Ability, gStringVar2, 19, sTextOrange);

	//linha 4: NATURE: ADAMANT
	StringCopy(gStringVar2, gNatureNamePointers[GetNature(mon)]);
	UpperCaseString(gStringVar2);
	PrintInfoLine(gText_StarterSelect_Nature, gStringVar2, 28, sTextOrange);

	PutWindowTilemap(WIN_INFO);
	CopyWindowToVram(WIN_INFO, 3);
}

//////////////////////////////////////////////////////////////////////
// Pokemon
//////////////////////////////////////////////////////////////////////
static void CreateStarterMons(struct StarterSelectState* state, u8 level)
{
	static const u16 fallbackSpecies[NUM_STARTERS] = {SPECIES_BULBASAUR, SPECIES_CHARMANDER, SPECIES_SQUIRTLE};
	u16 species[NUM_STARTERS];
	u16 item = Var8004;
	u8 i;

	species[0] = SanitizeSpecies(Var8000, fallbackSpecies[0]);
	species[1] = SanitizeSpecies(Var8001, fallbackSpecies[1]);
	species[2] = SanitizeSpecies(Var8002, fallbackSpecies[2]);

	for (i = 0; i < NUM_STARTERS; ++i)
	{
		CreateMon(&state->mons[i], species[i], level, STARTER_IV, FALSE, 0, OT_ID_PLAYER_ID, 0);
		if (Var8005 == 1)
			ForceMonShiny(&state->mons[i]);
		if (item != 0)
			SetMonData(&state->mons[i], MON_DATA_HELD_ITEM, &item);
		HealMon(&state->mons[i]);
	}
}

static void CreateStarterSprites(struct StarterSelectState* state)
{
	u8 i;
	for (i = 0; i < NUM_STARTERS; ++i)
	{
		struct Pokemon* mon = &state->mons[i];
		u16 species = GetMonData(mon, MON_DATA_SPECIES, NULL);
		u32 otId = GetMonData(mon, MON_DATA_OT_ID, NULL);
		u32 personality = GetMonData(mon, MON_DATA_PERSONALITY, NULL);
		const struct CompressedSpritePalette* palette = GetMonSpritePalStructFromOtIdPersonality(species, otId, personality);
		u16 spriteId = CreateMonPicSprite_HandleDeoxys(species, otId, personality, 1, sPicX[i], PIC_Y, i, palette->tag);

		if (spriteId == 0xFFFF)
		{
			state->spriteIds[i] = MAX_SPRITES;
			continue;
		}

		gSprites[spriteId].callback = SpriteCallbackDummy;
		gSprites[spriteId].oam.priority = 1;
		state->spriteIds[i] = spriteId;
	}
}

static void Task_StarterSelectInit(u8 taskId)
{
	struct Task* task = &gTasks[taskId];
	struct StarterSelectState* state;

	switch (task->tStep)
	{
	case 0:
		SetVBlankCallback(NULL);
		state = Calloc(sizeof(struct StarterSelectState));
		SetState(taskId, state);
		break;
	case 1:
		state = GetState(taskId);
		SetupBgsAndWindows(state);
		break;
	case 2:
		if (!LoadStarterSelectGfx(taskId))
			return;
		break;
	case 3:
		state = GetState(taskId);
		CopyToBgTilemapBuffer(1, bgStarterSelectMap, 0, 0);
		task->tLevel = (Var8003 == 0 || Var8003 > 100) ? DEFAULT_LEVEL : Var8003;
		task->tNoGive = (Var8006 == 1);
		task->tCursor = 0;
		CreateStarterMons(state, task->tLevel);
		break;
	case 4:
		state = GetState(taskId);
		UpdatePillarPalettes(state, task->tCursor);
		CreateStarterSprites(state);
		break;
	case 5:
		state = GetState(taskId);
		PrintNames(state, task->tCursor);
		PrintDialog(FALSE);
		PrintInfo(state, task->tCursor, task->tLevel);
		break;
	case 6:
		CopyBgTilemapBufferToVram(0);
		CopyBgTilemapBufferToVram(1);
		break;
	case 7:
		BeginNormalPaletteFade(0xFFFFFFFF, 0, 16, 0, RGB_BLACK);
		break;
	case 8:
		state = GetState(taskId);
		SetVBlankCallback(VBlankCB_StarterSelect);
		PlayCry7(GetMonData(&state->mons[task->tCursor], MON_DATA_SPECIES, NULL), 0);
		break;
	default:
		if (gPaletteFade->active)
			return;
		task->tFrame = 0;
		task->tPhase = PHASE_CHOOSING;
		task->func = Task_StarterSelectMain;
		return;
	}
	task->tStep++;
}

//////////////////////////////////////////////////////////////////////
// Loop principal
//////////////////////////////////////////////////////////////////////
static void UpdateBobbing(struct StarterSelectState* state, struct Task* task)
{
	u8 i;
	task->tFrame++;
	for (i = 0; i < NUM_STARTERS; ++i)
	{
		if (state->spriteIds[i] >= MAX_SPRITES)
			continue;

		if (i == task->tCursor)
			gSprites[state->spriteIds[i]].pos2.y = sBobTable[(task->tFrame >> 2) & 0xF];
		else
			gSprites[state->spriteIds[i]].pos2.y = 0;
	}
}

static void MoveCursor(u8 taskId, s8 delta)
{
	struct Task* task = &gTasks[taskId];
	struct StarterSelectState* state = GetState(taskId);
	s8 newCursor = task->tCursor + delta;

	if (newCursor < 0)
		newCursor = NUM_STARTERS - 1;
	else if (newCursor >= NUM_STARTERS)
		newCursor = 0;

	task->tCursor = newCursor;
	task->tFrame = 0;
	PlaySE(5);
	UpdatePillarPalettes(state, newCursor);
	PrintNames(state, newCursor);
	PrintInfo(state, newCursor, task->tLevel);
	PlayCry7(GetMonData(&state->mons[newCursor], MON_DATA_SPECIES, NULL), 0);
}

static void Task_StarterSelectMain(u8 taskId)
{
	struct Task* task = &gTasks[taskId];
	struct StarterSelectState* state = GetState(taskId);

	UpdateBobbing(state, task);

	if (gPaletteFade->active)
		return;

	switch (task->tPhase)
	{
	case PHASE_CHOOSING:
		if (JOY_NEW(DPAD_LEFT))
			MoveCursor(taskId, -1);
		else if (JOY_NEW(DPAD_RIGHT))
			MoveCursor(taskId, 1);
		else if (JOY_NEW(A_BUTTON))
		{
			PlaySE(5);
			PrintDialog(TRUE);
			task->tPhase = PHASE_CONFIRM;
		}
		//B nao faz nada de proposito: o jogador PRECISA escolher um inicial.
		break;

	case PHASE_CONFIRM:
		if (JOY_NEW(A_BUTTON))
		{
			PlaySE(5);
			BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
			task->tPhase = PHASE_FADE_OUT;
			task->func = Task_StarterSelectExit;
		}
		else if (JOY_NEW(B_BUTTON))
		{
			PlaySE(5);
			PrintDialog(FALSE);
			task->tPhase = PHASE_CHOOSING;
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////
// Saida: entrega o Pokemon e volta para o script
//////////////////////////////////////////////////////////////////////
static void Task_StarterSelectExit(u8 taskId)
{
	struct Task* task = &gTasks[taskId];
	struct StarterSelectState* state = GetState(taskId);
	u8 i;

	if (gPaletteFade->active)
		return;

	gSpecialVar_LastResult = task->tCursor;
	Var8006 = GetMonData(&state->mons[task->tCursor], MON_DATA_SPECIES, NULL);

	if (task->tNoGive)
		Var8007 = 2;
	else
	{
		Var8007 = GiveMonToPlayer(&state->mons[task->tCursor]);
		//Igual ao comando givepokemon: se o mon foi para a party (0) ou para o PC (1), registra na Pokedex
		if (Var8007 == 0 || Var8007 == 1)
			SetMonPokedexFlags(&state->mons[task->tCursor]);
	}

	for (i = 0; i < NUM_STARTERS; ++i)
	{
		if (state->spriteIds[i] < MAX_SPRITES)
			FreeAndDestroyMonPicSprite(state->spriteIds[i]);
	}

	FreeAllWindowBuffers();
	Free(state);
	DestroyTask(taskId);
	SetMainCallback2(CB2_ReturnToFieldContinueScript);
}
