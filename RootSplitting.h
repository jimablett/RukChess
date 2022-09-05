// RootSplitting.h

#pragma once

#ifndef ROOT_SPLITTING_H
#define ROOT_SPLITTING_H

#include "Board.h"
#include "Def.h"
#include "Types.h"

#ifdef ROOT_SPLITTING
int RootSplitting_Search(BoardItem* Board, int Alpha, int Beta, int Depth, const int Ply, MoveItem* BestMoves, const BOOL InCheck);
#endif // ROOT_SPLITTING

#endif // !ROOT_SPLITTING_H