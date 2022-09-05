// Abdada.h

#pragma once

#ifndef ABDADA_H
#define ABDADA_H

#include "Board.h"
#include "Def.h"
#include "Types.h"

#ifdef ABDADA
int ABDADA_Search(BoardItem* Board, int Alpha, int Beta, int Depth, const int Ply, MoveItem* BestMoves, const BOOL IsPrincipal, const BOOL InCheck, const BOOL UsePruning, const int SkipMove);
#endif // ABDADA

#endif // !ABDADA_H