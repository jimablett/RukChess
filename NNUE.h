// NNUE.h

#pragma once

#ifndef NNUE_H
#define NNUE_H

#include "Board.h"
#include "Def.h"

#ifdef NNUE_EVALUATION_FUNCTION
extern BOOL NnueFileLoaded;

BOOL LoadNetwork(const char* NnueFileName);

int NetworkEvaluate(BoardItem* Board);
#endif // NNUE_EVALUATION_FUNCTION

#endif // !NNUE_H