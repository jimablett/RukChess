// Gen.h

#pragma once

#ifndef GEN_H
#define GEN_H

#include "Board.h"
#include "Def.h"

void GenerateAllMoves(const BoardItem* Board, MoveItem* MoveList, int* GenMoveCount);
void GenerateCaptureMoves(const BoardItem* Board, MoveItem* MoveList, int* GenMoveCount);

int GenerateAllLegalMoves(BoardItem* Board, MoveItem* LegalMoveList);

#endif // !GEN_H