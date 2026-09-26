.align 2
.thumb

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

@ Character Swap System - script run when the Blue Flute is used.
@ Only does anything if CHARACTER_SWAP_SYSTEM is defined in src/config.h

@ Language flags: 0x261 = English, 0x262 = Spanish (Latin America), otherwise Portuguese (0x260)
.equ FLAG_LANG_ENGLISH, 0x261
.equ FLAG_LANG_SPANISH, 0x262

@ Shows "text", "text_EN" or "text_ES" depending on the language flags
.macro langmsgbox text:req, type:req
	checkflag FLAG_LANG_ENGLISH
	if equal _goto CharSwap_Lang_EN\@
	checkflag FLAG_LANG_SPANISH
	if equal _goto CharSwap_Lang_ES\@
	msgbox \text \type
	goto CharSwap_Lang_Done\@
CharSwap_Lang_EN\@:
	msgbox \text\()_EN \type
	goto CharSwap_Lang_Done\@
CharSwap_Lang_ES\@:
	msgbox \text\()_ES \type
CharSwap_Lang_Done\@:
.endm

.global SystemScript_CharacterSwap

SystemScript_CharacterSwap:
	lockall
	callasm CharSwap_CheckCanSwap
	compare LASTRESULT 0x1
	if equal _goto SystemScript_CharacterSwap_CantFollower
	compare LASTRESULT 0x2
	if equal _goto SystemScript_CharacterSwap_CantSurfing
	compare LASTRESULT 0x3
	if equal _goto SystemScript_CharacterSwap_CantNow
	langmsgbox gText_CharSwap_Ask MSG_YESNO
	compare LASTRESULT 0x0
	if equal _goto SystemScript_CharacterSwap_End
	closeonkeypress
	sound 0x6E @ SE_FLUTE
	checksound
	callasm CharSwap_ShouldAskName
	compare LASTRESULT 0x1
	if equal _call SystemScript_CharacterSwap_ChooseName
	callasm CharSwap_SwapAndWarp
	waitstate
	end

SystemScript_CharacterSwap_ChooseName:
	langmsgbox gText_CharSwap_ChooseName MSG_NORMAL
	fadescreen 0x1
	callasm CharSwap_OpenNamingScreen
	waitstate
	return

SystemScript_CharacterSwap_CantFollower:
	langmsgbox gText_CharSwap_CantFollower MSG_NORMAL
	goto SystemScript_CharacterSwap_End

SystemScript_CharacterSwap_CantSurfing:
	langmsgbox gText_CharSwap_CantSurfing MSG_NORMAL
	goto SystemScript_CharacterSwap_End

SystemScript_CharacterSwap_CantNow:
	langmsgbox gText_CharSwap_CantNow MSG_NORMAL

SystemScript_CharacterSwap_End:
	releaseall
	end
