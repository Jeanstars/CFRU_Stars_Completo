// ARQUIVO GERADO por editor_textos_intro.py - NAO EDITE A MAO (edite pelo programa).
#pragma once

extern const u8 gIntroText_intro_pag1_PT[];
extern const u8 gIntroText_intro_pag1_EN[];
extern const u8 gIntroText_intro_pag1_ES[];
extern const u8 gIntroText_intro_pag2_PT[];
extern const u8 gIntroText_intro_pag2_EN[];
extern const u8 gIntroText_intro_pag2_ES[];
extern const u8 gIntroText_intro_pag3_PT[];
extern const u8 gIntroText_intro_pag3_EN[];
extern const u8 gIntroText_intro_pag3_ES[];
extern const u8 gIntroText_botoes_intro_PT[];
extern const u8 gIntroText_botoes_intro_EN[];
extern const u8 gIntroText_botoes_intro_ES[];
extern const u8 gIntroText_botao_direcional_PT[];
extern const u8 gIntroText_botao_direcional_EN[];
extern const u8 gIntroText_botao_direcional_ES[];
extern const u8 gIntroText_botao_a_PT[];
extern const u8 gIntroText_botao_a_EN[];
extern const u8 gIntroText_botao_a_ES[];
extern const u8 gIntroText_botao_b_PT[];
extern const u8 gIntroText_botao_b_EN[];
extern const u8 gIntroText_botao_b_ES[];
extern const u8 gIntroText_botao_start_PT[];
extern const u8 gIntroText_botao_start_EN[];
extern const u8 gIntroText_botao_start_ES[];
extern const u8 gIntroText_botao_select_PT[];
extern const u8 gIntroText_botao_select_EN[];
extern const u8 gIntroText_botao_select_ES[];
extern const u8 gIntroText_botao_lr_PT[];
extern const u8 gIntroText_botao_lr_EN[];
extern const u8 gIntroText_botao_lr_ES[];
extern const u8 gIntroText_oak_ola_PT[];
extern const u8 gIntroText_oak_ola_EN[];
extern const u8 gIntroText_oak_ola_ES[];
extern const u8 gIntroText_oak_este_mundo_PT[];
extern const u8 gIntroText_oak_este_mundo_EN[];
extern const u8 gIntroText_oak_este_mundo_ES[];
extern const u8 gIntroText_oak_habitado_PT[];
extern const u8 gIntroText_oak_habitado_EN[];
extern const u8 gIntroText_oak_habitado_ES[];
extern const u8 gIntroText_oak_pokemon_sao_PT[];
extern const u8 gIntroText_oak_pokemon_sao_EN[];
extern const u8 gIntroText_oak_pokemon_sao_ES[];
extern const u8 gIntroText_oak_fale_de_voce_PT[];
extern const u8 gIntroText_oak_fale_de_voce_EN[];
extern const u8 gIntroText_oak_fale_de_voce_ES[];
extern const u8 gIntroText_oak_nome_PT[];
extern const u8 gIntroText_oak_nome_EN[];
extern const u8 gIntroText_oak_nome_ES[];
extern const u8 gIntroText_oak_seu_nome_e_PT[];
extern const u8 gIntroText_oak_seu_nome_e_EN[];
extern const u8 gIntroText_oak_seu_nome_e_ES[];
extern const u8 gIntroText_oak_neto_PT[];
extern const u8 gIntroText_oak_neto_EN[];
extern const u8 gIntroText_oak_neto_ES[];
extern const u8 gIntroText_oak_nome_rival_PT[];
extern const u8 gIntroText_oak_nome_rival_EN[];
extern const u8 gIntroText_oak_nome_rival_ES[];
extern const u8 gIntroText_oak_rival_era_PT[];
extern const u8 gIntroText_oak_rival_era_EN[];
extern const u8 gIntroText_oak_rival_era_ES[];
extern const u8 gIntroText_oak_rival_certo_PT[];
extern const u8 gIntroText_oak_rival_certo_EN[];
extern const u8 gIntroText_oak_rival_certo_ES[];
extern const u8 gIntroText_oak_lenda_PT[];
extern const u8 gIntroText_oak_lenda_EN[];
extern const u8 gIntroText_oak_lenda_ES[];
extern const u8 gIntroText_oak_menino_menina_PT[];
extern const u8 gIntroText_oak_menino_menina_EN[];
extern const u8 gIntroText_oak_menino_menina_ES[];

#define INTRO_LANG_COUNT 3
#define INTRO_LANG_DEFAULT 0
#define INTRO_TEXT_MIN 0x81C582D
#define INTRO_TEXT_MAX 0x81C5EF4

struct IntroTextEntry
{
	u32 original;
	const u8* text[INTRO_LANG_COUNT];
};

//Ordem dos idiomas = ordem das flags abaixo
static const u16 sIntroLangFlags[INTRO_LANG_COUNT] = {0x260, 0x261, 0x262};

static const struct IntroTextEntry sIntroTexts[] =
{
	{0x81C5A04, {gIntroText_intro_pag1_PT, gIntroText_intro_pag1_EN, gIntroText_intro_pag1_ES}},
	{0x81C5AEB, {gIntroText_intro_pag2_PT, gIntroText_intro_pag2_EN, gIntroText_intro_pag2_ES}},
	{0x81C5BB9, {gIntroText_intro_pag3_PT, gIntroText_intro_pag3_EN, gIntroText_intro_pag3_ES}},
	{0x81C582D, {gIntroText_botoes_intro_PT, gIntroText_botoes_intro_EN, gIntroText_botoes_intro_ES}},
	{0x81C5875, {gIntroText_botao_direcional_PT, gIntroText_botao_direcional_EN, gIntroText_botao_direcional_ES}},
	{0x81C58BA, {gIntroText_botao_a_PT, gIntroText_botao_a_EN, gIntroText_botao_a_ES}},
	{0x81C58F9, {gIntroText_botao_b_PT, gIntroText_botao_b_EN, gIntroText_botao_b_ES}},
	{0x81C592B, {gIntroText_botao_start_PT, gIntroText_botao_start_EN, gIntroText_botao_start_ES}},
	{0x81C594F, {gIntroText_botao_select_PT, gIntroText_botao_select_EN, gIntroText_botao_select_ES}},
	{0x81C5981, {gIntroText_botao_lr_PT, gIntroText_botao_lr_EN, gIntroText_botao_lr_ES}},
	{0x81C5C78, {gIntroText_oak_ola_PT, gIntroText_oak_ola_EN, gIntroText_oak_ola_ES}},
	{0x81C5D06, {gIntroText_oak_este_mundo_PT, gIntroText_oak_este_mundo_EN, gIntroText_oak_este_mundo_ES}},
	{0x81C5D12, {gIntroText_oak_habitado_PT, gIntroText_oak_habitado_EN, gIntroText_oak_habitado_ES}},
	{0x81C5D4B, {gIntroText_oak_pokemon_sao_PT, gIntroText_oak_pokemon_sao_EN, gIntroText_oak_pokemon_sao_ES}},
	{0x81C5DBD, {gIntroText_oak_fale_de_voce_PT, gIntroText_oak_fale_de_voce_EN, gIntroText_oak_fale_de_voce_ES}},
	{0x81C5DEA, {gIntroText_oak_nome_PT, gIntroText_oak_nome_EN, gIntroText_oak_nome_ES}},
	{0x81C5E13, {gIntroText_oak_seu_nome_e_PT, gIntroText_oak_seu_nome_e_EN, gIntroText_oak_seu_nome_e_ES}},
	{0x81C5E2E, {gIntroText_oak_neto_PT, gIntroText_oak_neto_EN, gIntroText_oak_neto_ES}},
	{0x81C5E91, {gIntroText_oak_nome_rival_PT, gIntroText_oak_nome_rival_EN, gIntroText_oak_nome_rival_ES}},
	{0x81C5EB5, {gIntroText_oak_rival_era_PT, gIntroText_oak_rival_era_EN, gIntroText_oak_rival_era_ES}},
	{0x81C5EC5, {gIntroText_oak_rival_certo_PT, gIntroText_oak_rival_certo_EN, gIntroText_oak_rival_certo_ES}},
	{0x81C5EF4, {gIntroText_oak_lenda_PT, gIntroText_oak_lenda_EN, gIntroText_oak_lenda_ES}},
	{0x81C59D5, {gIntroText_oak_menino_menina_PT, gIntroText_oak_menino_menina_EN, gIntroText_oak_menino_menina_ES}},
};
