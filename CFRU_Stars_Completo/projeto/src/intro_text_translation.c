#include "defines.h"
#include "../include/event_data.h"
#include "intro_texts_table.h"

/*
 * TEXTOS DA INTRODUCAO POR IDIOMA (guia de controles do Pikachu + Professor Carvalho/Oak)
 *
 * Os textos originais continuam na ROM. Dois ganchos (assembly/hooks/intro_text_hooks.s)
 * chamam TranslateIntroText() com o ponteiro do texto que o jogo ia mostrar; se for um dos
 * textos da tabela, devolvemos a versao do idioma escolhido (flags 0x260/0x261/0x262).
 *
 * A tabela (src/intro_texts_table.h) e os textos (strings/intro_texts.string) sao gerados
 * pelo programa ferramentas/editores/editor_textos_intro.py.
 */

extern u8 LanguageSelect_GetSavedLanguage(void);

static u32 GetIntroLanguageIndex(void)
{
	u32 i;
	u8 saved;

	for (i = 0; i < INTRO_LANG_COUNT; ++i)
	{
		if (FlagGet(sIntroLangFlags[i]))
			return i;
	}

	//Sem flag (ex.: o Novo Jogo acabou de zerar): usa o idioma guardado no SaveBlock2
	saved = LanguageSelect_GetSavedLanguage();
	if (saved >= 1 && saved <= INTRO_LANG_COUNT)
		return saved - 1;

	return INTRO_LANG_DEFAULT;
}

//Chamada pelos ganchos: recebe o texto original e devolve o texto do idioma atual
const u8* TranslateIntroText(const u8* src)
{
	u32 addr = (u32) src;
	u32 i;

	if (addr < INTRO_TEXT_MIN || addr > INTRO_TEXT_MAX) //Quase sempre cai aqui (rapido)
		return src;

	for (i = 0; i < NELEMS(sIntroTexts); ++i)
	{
		if (sIntroTexts[i].original == addr)
			return sIntroTexts[i].text[GetIntroLanguageIndex()];
	}

	return src;
}
