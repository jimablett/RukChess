// NNUE2.h

#pragma once

#ifndef NNUE2_H
#define NNUE2_H

#include "Board.h"
#include "Def.h"
#include "Types.h"

#ifdef NNUE_EVALUATION_FUNCTION_2
void ReadNetwork(void);

int NetworkEvaluate(BoardItem* Board);
#endif // NNUE_EVALUATION_FUNCTION_2

#endif // !NNUE2_H