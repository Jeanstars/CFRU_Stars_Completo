#pragma once

/*
character_swap_screen.h
	Character Swap System: data shared between character_swap.c and the character selection screen
	(src/character_swap_screen.c).
*/

#ifdef CHARACTER_SWAP_SYSTEM

//A few fields read (read-only) from the character that is stored in the PC boxes (the inactive one)
struct CharSwapPeek
{
	bool8 exists;                       //FALSE = the other character was never used (nothing is stored)
	u8 name[PLAYER_NAME_LENGTH + 1];    //Name chosen by the player
	u8 partyCount;
	u8 badgeFlags;                      //One bit per badge (the 8 badge flags are one byte of the flags)
	u16 walkSpriteVar;                  //VAR_PLAYER_WALKRUN of that character (0 = default sprite)
	u32 moneyRaw;                       //Encrypted like gSaveBlock1->money (use GetMoney)
	struct WarpData location;
	#ifdef CHAR_SWAP_SEPARATE_POKEDEX
	u8 dexCaughtFlags[sizeof(((struct SaveBlock1*) 0)->dexCaughtFlags)];
	#endif
};

bool8 CharSwap_IsOtherCharacterNew(void);
bool8 CharSwap_PeekOtherCharacter(struct CharSwapPeek* out);

//callasm CharSwap_OpenSwapScreen -> (follow with waitstate) LASTRESULT = 1 if the player chose to switch, 0 if cancelled
void CharSwap_OpenSwapScreen(void);

#endif
