#include "defines.h"
#include "../include/bg.h"
#include "../include/gpu_regs.h"
#include "../include/main.h"
#include "../include/malloc.h"
#include "../include/menu.h"
#include "../include/palette.h"
#include "../include/save.h"
#include "../include/script.h"
#include "../include/scanline_effect.h"
#include "../include/sound.h"
#include "../include/sprite.h"
#include "../include/task.h"
#include "../include/text.h"
#include "../include/window.h"
#include "../include/event_data.h"
#include "../include/international_string_util.h"
#include "../include/constants/songs.h"
#include "../include/new/dns.h"

/*
 * SELECAO DE IDIOMA (aparece depois da tela de titulo)
 *
 * Idiomas e flags (as mesmas dos seus scripts XSE):
 *     0x260 = BR  (Portugues)
 *     0x261 = USA (English)
 *     0x262 = ESP (Espanol)
 *
 * Fluxo:
 *  - Save novo/vazio ............ a tela aparece.
 *  - Save que ja tem uma das 3 flags ligada ... a tela e pulada (vale a flag).
 *  - Save antigo sem nenhuma flag ... a tela aparece uma vez.
 *
 * O "Novo Jogo" zera as flags (ClearSav1), entao o idioma tambem fica guardado
 * num byte livre do SaveBlock2 (filler_28[0x6F]), que o Novo Jogo nao apaga.
 * Logo depois do Novo Jogo as flags sao religadas (LanguageSelect_ReapplyAfterNewGame).
 *
 * ATENCAO: neste build o .bss fica na ROM, entao nao existem variaveis globais
 * gravaveis. Todo estado da tela fica nos dados da task (gTasks[].data[]).
 */

#define LANG_FLAG_BR  0x260
#define LANG_FLAG_USA 0x261
#define LANG_FLAG_ESP 0x262

#define LANG_NONE 0
#define LANG_BR   1
#define LANG_USA  2
#define LANG_ESP  3
#define LANG_COUNT 3

#define LANGUAGE_SAVE_BYTE (gSaveBlock2->filler_28[0x6F])

#ifndef NEW_GAME_START_LOCATION
	#warning "Selecao de idioma: sem NEW_GAME_START_LOCATION as flags nao sao religadas no Novo Jogo (veja LanguageSelect_ReapplyAfterNewGame)."
#endif

// Estado da task (tudo fica aqui: neste build nao ha variaveis globais gravaveis)
#define tState     data[0]
#define tCursor    data[1]
#define tTilemap0Lo data[2]
#define tTilemap0Hi data[3]
#define tSpriteId  data[4]
#define tTilemap1Lo data[5]
#define tTilemap1Hi data[6]
#define tTilemap2Lo data[7]
#define tTilemap2Hi data[8]
#define tFrame     data[9]

enum
{
	STATE_FADE_IN,
	STATE_INPUT,
	STATE_FADE_OUT,
};

extern const u8 gText_LangSelect_Title_PT[];
extern const u8 gText_LangSelect_Title_EN[];
extern const u8 gText_LangSelect_Title_ES[];
extern const u8 gText_LangSelect_Option_PT[];
extern const u8 gText_LangSelect_Option_EN[];
extern const u8 gText_LangSelect_Option_ES[];
extern const u8 gText_LangSelect_Arrow[];
extern const u8 gText_LangSelect_Hint[];

//Graficos gerados por ferramentas/tela_idioma/gerar_tela_idioma.py (src/language_select_gfx.c)
extern const u32 gLangBg1Tiles[];
extern const u32 gLangBg1TilesSize;
extern const u16 gLangBg1Map[];
extern const u32 gLangBg2Tiles[];
extern const u16 gLangPaletteBg[];
extern const u16 gLangCardPalSelected[];
extern const u32 gLangArrowTiles[];
extern const u16 gLangArrowPalette[];
extern const u16 gLangPulse[3][2];

static const u8 *const sOptionNames[LANG_COUNT] =
{
	gText_LangSelect_Option_PT,
	gText_LangSelect_Option_EN,
	gText_LangSelect_Option_ES,
};

static const u16 sLanguageFlags[LANG_COUNT] = {LANG_FLAG_BR, LANG_FLAG_USA, LANG_FLAG_ESP};

// BG0 = textos, BG1 = cartoes/bandeiras/divisor, BG2 = degrade do fundo
static const struct BgTemplate sBgTemplates[] =
{
	{ .bg = 0, .charBaseIndex = 2, .mapBaseIndex = 30, .screenSize = 0, .paletteMode = 0, .priority = 0, .baseTile = 0 },
	{ .bg = 1, .charBaseIndex = 0, .mapBaseIndex = 29, .screenSize = 0, .paletteMode = 0, .priority = 1, .baseTile = 0 },
	{ .bg = 2, .charBaseIndex = 1, .mapBaseIndex = 28, .screenSize = 0, .paletteMode = 0, .priority = 2, .baseTile = 0 },
};

static const struct WindowTemplate sWindowTemplates[] =
{
	{
		.bg = 0,
		.tilemapLeft = 0,
		.tilemapTop = 0,
		.width = 30,
		.height = 20,
		.paletteNum = 0,
		.baseBlock = 1,
	},
	DUMMY_WIN_TEMPLATE
};

// Cores dos textos (indices da paleta 0 do fundo; veja TEXTO no gerador)
static const u8 sColorsTitle[3]      = {0, 1, 2};
static const u8 sColorsSelected[3]   = {0, 1, 2};
static const u8 sColorsUnselected[3] = {0, 3, 4};
static const u8 sColorsHint[3]       = {0, 5, 4};

#define WIN_MAIN 0
#define TITLE_Y 2
#define TITLE_SPACING 15
#define CARD_TOP_Y 56			//Topo do primeiro cartao (em pixels)
#define CARD_SPACING 32
#define OPTION_X 66
#define OPTION_TEXT_DY 4		//Texto dentro do cartao
#define HINT_Y 146

#define ARROW_TAG 0x4C47
#define ARROW_X 6				//Centro da setinha (balanca para a direita)
#define ARROW_CENTER_DY 11

static const struct OamData sArrowOam =
{
	.affineMode = ST_OAM_AFFINE_OFF,
	.objMode = ST_OAM_OBJ_NORMAL,
	.shape = SPRITE_SHAPE(8x16),
	.size = SPRITE_SIZE(8x16),
	.priority = 0,
};

static const struct SpriteSheet sArrowSheet = {(const u8*) gLangArrowTiles, 64, ARROW_TAG};
static const struct SpritePalette sArrowPalette = {gLangArrowPalette, ARROW_TAG};
static const struct SpriteTemplate sArrowTemplate =
{
	.tileTag = ARROW_TAG,
	.paletteTag = ARROW_TAG,
	.oam = &sArrowOam,
	.anims = gDummySpriteAnimTable,
	.images = NULL,
	.affineAnims = gDummySpriteAffineAnimTable,
	.callback = SpriteCallbackDummy,
};

static const u8 sArrowBob[8] = {0, 1, 2, 3, 4, 3, 2, 1};

/* ------------------------------------------------------------------ */
/* Idioma: aplicar / guardar                                          */
/* ------------------------------------------------------------------ */

static void SetLanguage(u8 language)
{
	u32 i;

	if (language == LANG_NONE || language > LANG_COUNT)
		return;

	for (i = 0; i < LANG_COUNT; ++i)
	{
		if (i == (u32) (language - 1))
			FlagSet(sLanguageFlags[i]);
		else
			FlagClear(sLanguageFlags[i]);
	}

	LANGUAGE_SAVE_BYTE = language;
}

static u8 GetLanguageFromFlags(void)
{
	u32 i;

	for (i = 0; i < LANG_COUNT; ++i)
	{
		if (FlagGet(sLanguageFlags[i]))
			return i + 1;
	}

	return LANG_NONE;
}

//Chamado no fim da tela de titulo (SetTitleScreenScene_Cry). TRUE = mostrar a tela de idioma.
bool8 LanguageSelect_NeedsScreen(void)
{
	u8 language = LANG_NONE;

	//Save vazio/invalido: nada de flags ou byte antigos
	if (gSaveFileStatus != SAVE_STATUS_EMPTY && gSaveFileStatus != SAVE_STATUS_INVALID)
	{
		language = GetLanguageFromFlags();	//As flags valem mais (seus scripts podem mudar)

		if (language == LANG_NONE && LANGUAGE_SAVE_BYTE >= LANG_BR && LANGUAGE_SAVE_BYTE <= LANG_ESP)
			language = LANGUAGE_SAVE_BYTE;
	}

	if (language != LANG_NONE)
	{
		SetLanguage(language);	//Deixa flags e byte iguais
		return FALSE;
	}

	return TRUE;
}

//Chamado por WarpToPlayersRoom_Custom (fim do NewGameInitData, depois que as flags foram zeradas)
void LanguageSelect_ReapplyAfterNewGame(void)
{
	u8 language = LANGUAGE_SAVE_BYTE;

	if (language >= LANG_BR && language <= LANG_ESP)
		SetLanguage(language);
}

//Usado pelos textos da introducao e pela rota do Nuzlocke (apaga o save inteiro e reinicia)
u8 LanguageSelect_GetSavedLanguage(void)
{
	return LANGUAGE_SAVE_BYTE;
}

void LanguageSelect_SetSavedLanguage(u8 language)
{
	LANGUAGE_SAVE_BYTE = language;
}

/* ------------------------------------------------------------------ */
/* Tela                                                               */
/* ------------------------------------------------------------------ */

static u8 GetCenteredX(const u8* str, u8 fontId)
{
	s32 width = GetStringWidth(fontId, str, 0);

	if (width >= 240)
		return 0;

	return (240 - width) / 2;
}

static void PrintLanguageMenu(u8 cursor)
{
	u32 i;

	FillWindowPixelBuffer(WIN_MAIN, PIXEL_FILL(0));

	AddTextPrinterParameterized3(WIN_MAIN, 2, GetCenteredX(gText_LangSelect_Title_PT, 2), TITLE_Y,                      sColorsTitle, 0, gText_LangSelect_Title_PT);
	AddTextPrinterParameterized3(WIN_MAIN, 2, GetCenteredX(gText_LangSelect_Title_EN, 2), TITLE_Y + TITLE_SPACING,     sColorsTitle, 0, gText_LangSelect_Title_EN);
	AddTextPrinterParameterized3(WIN_MAIN, 2, GetCenteredX(gText_LangSelect_Title_ES, 2), TITLE_Y + 2 * TITLE_SPACING, sColorsTitle, 0, gText_LangSelect_Title_ES);

	for (i = 0; i < LANG_COUNT; ++i)
	{
		u8 y = CARD_TOP_Y + (i * CARD_SPACING) + OPTION_TEXT_DY;
		AddTextPrinterParameterized3(WIN_MAIN, 2, OPTION_X, y, (i == cursor) ? sColorsSelected : sColorsUnselected, 0, sOptionNames[i]);
	}

	AddTextPrinterParameterized3(WIN_MAIN, 0, GetCenteredX(gText_LangSelect_Hint, 0), HINT_Y, sColorsHint, 0, gText_LangSelect_Hint);

	PutWindowTilemap(WIN_MAIN);
	CopyWindowToVram(WIN_MAIN, COPYWIN_BOTH);
}

//Mistura duas cores RGB555: t = 0..32
static u16 BlendRgb5(u16 a, u16 b, u8 t)
{
	u16 r = (((a & 31) * (32 - t)) + ((b & 31) * t)) >> 5;
	u16 g = ((((a >> 5) & 31) * (32 - t)) + (((b >> 5) & 31) * t)) >> 5;
	u16 bl = ((((a >> 10) & 31) * (32 - t)) + (((b >> 10) & 31) * t)) >> 5;
	return r | (g << 5) | (bl << 10);
}

//Paleta de cada cartao: o selecionado fica azul-claro e a borda/miolo pulsam com o tempo
static void UpdateCardPalettes(u8 cursor, u16 frame, bool8 onlySelected)
{
	u32 k, i;

	for (k = 0; k < LANG_COUNT; ++k)
	{
		u16 pal[16];

		if (k != cursor)
		{
			if (onlySelected)
				continue;
			for (i = 0; i < 16; ++i)
				pal[i] = gLangPaletteBg[(1 + k) * 16 + i];
		}
		else
		{
			u8 phase = frame & 63;
			u8 t = (phase < 32 ? phase : 63 - phase);	//0..31 (vai e volta)

			for (i = 0; i < 16; ++i)
				pal[i] = gLangCardPalSelected[k * 16 + i];

			pal[1] = BlendRgb5(gLangPulse[0][0], gLangPulse[0][1], t);	//Borda
			pal[4] = BlendRgb5(gLangPulse[1][0], gLangPulse[1][1], t);	//Brilho de cima
			pal[6] = BlendRgb5(gLangPulse[2][0], gLangPulse[2][1], t);	//Miolo do indicador
		}

		LoadPalette(pal, (1 + k) * 16, 32);
	}
}

static void VBlankCB_LanguageSelect(void)
{
	LoadOam();
	ProcessSpriteCopyRequests();
	TransferPlttBuffer();
}

static void CB2_LanguageSelectMain(void)
{
	RunTasks();
	AnimateSprites();
	BuildOamBuffer();
	UpdatePaletteFade();
}

static void* GetTaskPtr(s16 lo, s16 hi)
{
	return (void*) ((u32) ((u16) lo) | ((u32) ((u16) hi) << 16));
}

static void FinishLanguageSelect(u8 taskId)
{
	struct Task* task = &gTasks[taskId];
	void* tm0 = GetTaskPtr(task->tTilemap0Lo, task->tTilemap0Hi);
	void* tm1 = GetTaskPtr(task->tTilemap1Lo, task->tTilemap1Hi);
	void* tm2 = GetTaskPtr(task->tTilemap2Lo, task->tTilemap2Hi);

	SetLanguage(task->tCursor + 1);

	DestroySprite(&gSprites[task->tSpriteId]);
	FreeSpriteTilesByTag(ARROW_TAG);
	FreeSpritePaletteByTag(ARROW_TAG);

	FreeAllWindowBuffers();
	if (tm0 != NULL) Free(tm0);
	if (tm1 != NULL) Free(tm1);
	if (tm2 != NULL) Free(tm2);
	DestroyTask(taskId);

	SetVBlankCallback(NULL);
	ResetSpriteData();
	FreeAllSpritePalettes();
	SetMainCallback2(CB2_InitMainMenu);	//Segue para o menu principal (Continuar / Novo Jogo)
}

//Faz a setinha deslizar ate o cartao selecionado e balancar
static void UpdateArrow(struct Task* task)
{
	struct Sprite* arrow = &gSprites[task->tSpriteId];
	s16 alvo = CARD_TOP_Y + (task->tCursor * CARD_SPACING) + ARROW_CENTER_DY;
	s16 dif = alvo - arrow->pos1.y;

	if (dif > 6) dif = 6;
	else if (dif < -6) dif = -6;
	arrow->pos1.y += dif;
	arrow->pos1.x = ARROW_X + sArrowBob[(task->tFrame >> 2) & 7];
}

static void Task_LanguageSelect(u8 taskId)
{
	struct Task* task = &gTasks[taskId];

	++task->tFrame;
	UpdateArrow(task);

	switch (task->tState)
	{
		case STATE_FADE_IN:
			if (!gPaletteFade->active)
				task->tState = STATE_INPUT;
			break;

		case STATE_INPUT:
			if (JOY_NEW(DPAD_UP) || JOY_NEW(DPAD_DOWN))
			{
				if (JOY_NEW(DPAD_UP))
					task->tCursor = (task->tCursor == 0) ? LANG_COUNT - 1 : task->tCursor - 1;
				else
					task->tCursor = (task->tCursor >= LANG_COUNT - 1) ? 0 : task->tCursor + 1;
				PlaySE(SE_SELECT);
				PrintLanguageMenu(task->tCursor);
				UpdateCardPalettes(task->tCursor, task->tFrame, FALSE);
			}
			else if (JOY_NEW(A_BUTTON))
			{
				PlaySE(SE_SELECT);
				BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
				task->tState = STATE_FADE_OUT;
			}
			else if ((task->tFrame & 1) == 0)
				UpdateCardPalettes(task->tCursor, task->tFrame, TRUE);	//So o pulso do cartao selecionado
			break;

		case STATE_FADE_OUT:
			if (!gPaletteFade->active)
				FinishLanguageSelect(taskId);
			break;
	}
}

//Substitui o CB2_InitMainMenu logo depois da tela de titulo (quando LanguageSelect_NeedsScreen() == TRUE)
void CB2_LanguageSelect(void)
{
	u8 taskId;
	u8 spriteId;
	u16* tm0;
	u16* tm1;
	u16* tm2;
	u32 x, y;

	//Limpa tudo que a tela de titulo deixou
	SetVBlankCallback(NULL);
	SetHBlankCallback(NULL);
	SetGpuReg(REG_OFFSET_DISPCNT, 0);
	SetGpuReg(REG_OFFSET_BG0CNT, 0);
	SetGpuReg(REG_OFFSET_BG1CNT, 0);
	SetGpuReg(REG_OFFSET_BG2CNT, 0);
	SetGpuReg(REG_OFFSET_BG3CNT, 0);
	SetGpuReg(REG_OFFSET_BG0HOFS, 0);
	SetGpuReg(REG_OFFSET_BG0VOFS, 0);
	SetGpuReg(REG_OFFSET_BG1HOFS, 0);
	SetGpuReg(REG_OFFSET_BG1VOFS, 0);
	SetGpuReg(REG_OFFSET_BG2HOFS, 0);
	SetGpuReg(REG_OFFSET_BG2VOFS, 0);
	SetGpuReg(REG_OFFSET_BG3HOFS, 0);
	SetGpuReg(REG_OFFSET_BG3VOFS, 0);
	SetGpuReg(REG_OFFSET_WIN0H, 0);
	SetGpuReg(REG_OFFSET_WIN1H, 0);
	SetGpuReg(REG_OFFSET_WIN0V, 0);
	SetGpuReg(REG_OFFSET_WIN1V, 0);
	SetGpuReg(REG_OFFSET_WININ, 0);
	SetGpuReg(REG_OFFSET_WINOUT, 0);
	SetGpuReg(REG_OFFSET_MOSAIC, 0);
	SetGpuReg(REG_OFFSET_BLDCNT, 0);
	SetGpuReg(REG_OFFSET_BLDALPHA, 0);
	SetGpuReg(REG_OFFSET_BLDY, 0);

	DmaFill16(3, 0, (void*) VRAM, VRAM_SIZE);
	DmaFill32(3, 0, (void*) OAM, OAM_SIZE);
	DmaFill16(3, 0, (void*) PLTT, PLTT_SIZE);

	ScanlineEffect_Stop();
	ResetTasks();
	ResetSpriteData();
	FreeAllSpritePalettes();
	ResetPaletteFade();

	//Fundos e janela de texto
	ResetBgsAndClearDma3BusyFlags(0);
	InitBgsFromTemplates(0, sBgTemplates, NELEMS(sBgTemplates));

	tm0 = AllocZeroed(BG_SCREEN_SIZE);
	tm1 = AllocZeroed(BG_SCREEN_SIZE);
	tm2 = AllocZeroed(BG_SCREEN_SIZE);
	SetBgTilemapBuffer(0, tm0);
	SetBgTilemapBuffer(1, tm1);
	SetBgTilemapBuffer(2, tm2);

	InitWindows(sWindowTemplates);
	DeactivateAllTextPrinters();

	//Graficos: BG1 (cartoes, bandeiras, divisor) e BG2 (degrade)
	LoadBgTiles(1, gLangBg1Tiles, gLangBg1TilesSize, 0);
	LoadBgTiles(2, gLangBg2Tiles, 64, 0);
	for (y = 0; y < 20; ++y)
	{
		for (x = 0; x < 30; ++x)
		{
			tm1[y * 32 + x] = gLangBg1Map[y * 30 + x];
			tm2[y * 32 + x] = (y & 1) | ((5 + (y >> 1)) << 12);	//Degrade: 2 linhas de tiles por paleta, com xadrez entre os tons
		}
	}
	CopyBgTilemapBufferToVram(1);
	CopyBgTilemapBufferToVram(2);

	//Paletas (fundo: 16 paletas; objetos: setinha)
	LoadPalette(gLangPaletteBg, 0, 512);
	UpdateCardPalettes(0, 0, FALSE);
	LoadSpriteSheet(&sArrowSheet);
	LoadSpritePalette(&sArrowPalette);
	spriteId = CreateSprite(&sArrowTemplate, ARROW_X, CARD_TOP_Y + ARROW_CENTER_DY, 0);

	taskId = CreateTask(Task_LanguageSelect, 0);
	gTasks[taskId].tState = STATE_FADE_IN;
	gTasks[taskId].tCursor = 0;
	gTasks[taskId].tFrame = 0;
	gTasks[taskId].tSpriteId = spriteId;
	gTasks[taskId].tTilemap0Lo = (u32) tm0 & 0xFFFF;
	gTasks[taskId].tTilemap0Hi = ((u32) tm0 >> 16) & 0xFFFF;
	gTasks[taskId].tTilemap1Lo = (u32) tm1 & 0xFFFF;
	gTasks[taskId].tTilemap1Hi = ((u32) tm1 >> 16) & 0xFFFF;
	gTasks[taskId].tTilemap2Lo = (u32) tm2 & 0xFFFF;
	gTasks[taskId].tTilemap2Hi = ((u32) tm2 >> 16) & 0xFFFF;

	PrintLanguageMenu(0);

	ShowBg(0);
	ShowBg(1);
	ShowBg(2);
	SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_1D_MAP | DISPCNT_OBJ_ON | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_BG2_ON);

	SetVBlankCallback(VBlankCB_LanguageSelect);
	BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_WHITE);	//Vem do branco do fim da tela de titulo
	SetMainCallback2(CB2_LanguageSelectMain);
}
