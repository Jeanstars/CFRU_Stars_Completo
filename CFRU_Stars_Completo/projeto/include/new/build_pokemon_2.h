#pragma once

#include "../../src/defines.h"
#include "../../src/defines_battle.h"

/**
 * \file build_pokemon_2.h
 * \brief A file to be included only by "src/build_pokemon.c". It contains
 *		  several macros and constants for building Pokemon.
 */

#ifdef FLAG_MATCH_HIGHEST_TRAINER_LEVEL
#define IF_FLAG_MATCH_HIGHEST_TRAINER_LEVEL(structure)																										\
	if (side == B_SIDE_OPPONENT && FlagGet(FLAG_MATCH_HIGHEST_TRAINER_LEVEL))																					\
	{																																						\
		u8 matchHighestPlayerLevel = GetHighestMonLevel(gPlayerParty);																							\
		if (structure[i].lvl >= matchLevelOrigMax) /* This is (one of) the Trainer's strongest Pokemon */														\
			lvl = matchHighestPlayerLevel;																														\
		else /* Rest of the team sits 2-3 levels below the ace */																								\
		{																																						\
			u8 gap = 2 + (Random() & 1);																														\
			lvl = (matchHighestPlayerLevel > gap) ? (matchHighestPlayerLevel - gap) : 1;																			\
		}																																						\
	}
#else
#define IF_FLAG_MATCH_HIGHEST_TRAINER_LEVEL(structure)
#endif

#define MAKE_POKEMON(structure)																																\
{																																							\
	u16 speciesToCreate = TryReplaceNormalTrainerSpecies(structure[i].species, trainerId);																	\
	for (j = 0; gSpeciesNames[speciesToCreate][j] != EOS; ++j)																								\
		nameHash += gSpeciesNames[speciesToCreate][j];																										\
																																							\
	personalityValue += nameHash << 8;																														\
																																							\
	u8 lvl = structure[i].lvl;																																\
	if (FlagGet(FLAG_SCALE_TRAINER_LEVELS)																													\
	|| (gBattleTypeFlags & BATTLE_TYPE_TRAINER_TOWER))																										\
		lvl = GetHighestMonLevel(gPlayerParty);																												\
																																							\
	if (levelScaling && (side == B_SIDE_OPPONENT || !firstTrainer))																							\
	{																																						\
		if (IsBossTrainerClassForLevelScaling(trainerId))																									\
			ModifySpeciesAndLevelForBossBattle(&speciesToCreate, &lvl, maxPartyLevel, highestPlayerLevel, canEvolveMon);									\
		else																																				\
			ModifySpeciesAndLevelForGenericBattle(&speciesToCreate, &lvl, minPartyLevel, highestPlayerLevel, modifiedAveragePlayerLevel, trainer->partyFlags, trainer->partySize, canEvolveMon);	\
	}																																						\
																																							\
	/* FLAG_MATCH_HIGHEST_TRAINER_LEVEL: this Trainer's ace matches the player's strongest level, rest are 2-3 below it */								\
	IF_FLAG_MATCH_HIGHEST_TRAINER_LEVEL(structure)																											\
																																							\
	CreateMon(&party[i], speciesToCreate, lvl, baseIV, TRUE, personalityValue, otIdType, otid);																\
	TryFixMiniorForm(&party[i]);																															\
	party[i].metLevel = structure[i].lvl;																													\
}

#define SET_MOVES(structure)													\
{																				\
	for (j = 0; j < MAX_MON_MOVES; ++j) {										\
		party[i].moves[j] = structure[i].moves[j];								\
		party[i].pp[j] = GetTrainerMonMovePP(structure[i].moves[j], j);			\
	}																			\
																				\
	party[i].ppBonuses = GetTrainerMonMovePPBonus();							\
}

#define SET_IVS_SINGLE_VALUE(val)					\
{													\
    party[i].hpIV = val;							\
	party[i].attackIV = val;						\
	party[i].defenseIV = val;						\
	party[i].speedIV = val;							\
	party[i].spAttackIV = val;						\
	party[i].spDefenseIV = val;						\
}

#define SET_IVS(structure)							\
{													\
    party[i].hpIV = structure->hpIv;				\
	party[i].attackIV = structure->atkIv;			\
	party[i].defenseIV = structure->defIv;			\
	party[i].speedIV = structure->spdIv;			\
	party[i].spAttackIV = structure->spAtkIv;		\
	party[i].spDefenseIV = structure->spDefIv;		\
}

#define SET_EVS(structure)							\
{													\
    party[i].hpEv = structure->hpEv;				\
	party[i].atkEv = structure->atkEv;				\
	party[i].defEv = structure->defEv;				\
	party[i].spdEv = structure->spdEv;				\
	party[i].spAtkEv = structure->spAtkEv;			\
	party[i].spDefEv = structure->spDefEv;			\
}

struct TrainersWithEvs
{
    u8 nature;
    u8 ivs;
    u8 hpEv;
	u8 atkEv;
	u8 defEv;
	u8 spdEv;
	u8 spAtkEv;
	u8 spDefEv;
	u8 ball; //0FE = Class-Based Ball, 0xFF = Random Ball
	u8 ability; //0 = Hidden, 1 = Ability_1, 2 = Ability_2, 3 = Random Ability 1 & 2, 4 = Random Any Ability
	u8 teraType; // 0xFF = Random Tera Type based on Original Types, 0xFE = Random Tera Type
	bool8 shiny; // 0 = not shiny, 1 = shiny
};

enum
{
	Ability_Hidden,
	Ability_1,
	Ability_2,
	Ability_Random_1_2,
	Ability_RandomAll,
};

#define TRAINER_EV_CLASS_BALL 0xFE
#define TRAINER_EV_RANDOM_BALL 0xFF
