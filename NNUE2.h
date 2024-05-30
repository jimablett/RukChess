// NNUE2.h

#pragma once

#ifndef NNUE2_H
#define NNUE2_H

#include "Board.h"
#include "Def.h"

extern BOOL NnueFileLoaded;

BOOL LoadNetwork(const char* NnueFileName);

int NetworkEvaluate(BoardItem* Board);

#endif // !NNUE2_H