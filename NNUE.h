// NNUE.h

#pragma once

#ifndef NNUE_H
#define NNUE_H

#include "Board.h"
#include "Def.h"
#include "Types.h"

#ifdef NNUE_EVALUATION_FUNCTION
void ReadNetwork(void);

int NetworkEvaluate(BoardItem* Board);
#endif // NNUE_EVALUATION_FUNCTION

#endif // !NNUE_H