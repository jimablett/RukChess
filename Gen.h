// Gen.h

#pragma once

#ifndef GEN_H
#define GEN_H

#include "Board.h"
#include "Def.h"

void GenerateAllMoves(const BoardItem* Board, MoveItem* MoveList, int* GenMoveCount);
void GenerateCaptureMoves(const BoardItem* Board, MoveItem* MoveList, int* GenMoveCount);

#endif // !GEN_H