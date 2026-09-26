#pragma once

#include "../global.h"

/**
 * \file animated_trainer_pics.h
 * \brief 2-frame animation for the opponent trainer sprite in battle.
 *        The list of animated trainers is in src/animated_trainer_pics.c.
 */

struct AnimatedTrainerPic
{
	u16 trainerPicId;
	const u32* tiles;   //64x128 sprite: frame 1 on top, frame 2 below (LZ77 compressed)
	u8 frameDuration;   //Frames each image stays on screen (60 = 1 second)
	u8 playCount;       //0 = loop forever, N = play N times and stop on frame 1
};

//Exported Functions
void LoadTrainerFrontPicWithAnim(u16 trainerPicId, u8 battlerId);
void TryStartTrainerPicAnim(u8 spriteId, u16 trainerPicId);
