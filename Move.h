// Move.h

#pragma once

#ifndef MOVE_H
#define MOVE_H

#include "Board.h"
#include "Def.h"

void MakeMove(BoardItem* Board, const MoveItem Move);
void UnmakeMove(BoardItem* Board);

#ifdef NULL_MOVE_PRUNING
void MakeNullMove(BoardItem* Board);
void UnmakeNullMove(BoardItem* Board);
#endif // NULL_MOVE_PRUNING

#endif // !MOVE_H