// PV_Splitting.h

#pragma once

#ifndef PV_SPLITTING_H
#define PV_SPLITTING_H

#include "Board.h"
#include "Def.h"
#include "Types.h"

#ifdef PV_SPLITTING
int PVSplitting_Search(BoardItem* Board, int Alpha, int Beta, int Depth, const int Ply, MoveItem* BestMoves, const BOOL InCheck, const int SkipMove);
#endif // PV_SPLITTING

#endif // !PV_SPLITTING_H