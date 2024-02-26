// Heuristic.h

#pragma once

#ifndef HEURISTIC_H
#define HEURISTIC_H

#include "Board.h"
#include "Def.h"

#define MAX_HEURISTIC_SCORE     (1 << 25)

#define BONUS(Depth)            ((Depth) * (Depth))

#ifdef COMMON_HEURISTIC_TABLE

extern volatile int HeuristicTable[2][6][64]; // [Color][Piece][Square]

#ifdef COMMON_COUNTER_MOVE_HISTORY_TABLE
extern volatile int CounterMoveHistoryTable[6][64][6 * 64]; // [Piece][Square][Piece * Square]
#endif // COMMON_COUNTER_MOVE_HISTORY_TABLE

#endif // COMMON_HEURISTIC_TABLE

#ifdef COMMON_KILLER_MOVE_TABLE
extern volatile int KillerMoveTable[MAX_PLY + 1][2]; // [Max. ply + 1][Two killer moves]
#endif // COMMON_KILLER_MOVE_TABLE

#ifdef COMMON_COUNTER_MOVE_TABLE
extern volatile int CounterMoveTable[2][6][64]; // [Color][Piece][Square]
#endif // COMMON_COUNTER_MOVE_TABLE

#ifdef MOVES_SORT_HEURISTIC

void UpdateHeuristic(BoardItem* Board, int** CMH_Pointer, const int Move, const int Bonus);
void ClearHeuristic(BoardItem* Board);

#ifdef COUNTER_MOVE_HISTORY
void SetCounterMoveHistoryPointer(BoardItem* Board, int** CMH_Pointer, const int Ply);
#endif // COUNTER_MOVE_HISTORY

#endif // MOVES_SORT_HEURISTIC

#ifdef KILLER_MOVE
void UpdateKillerMove(BoardItem* Board, const int Move, const int Ply);
void ClearKillerMove(BoardItem* Board);
#endif // KILLER_MOVE

#ifdef COUNTER_MOVE
void UpdateCounterMove(BoardItem* Board, const int Move, const int Ply);
void ClearCounterMove(BoardItem* Board);
#endif // COUNTER_MOVE

#endif // !HEURISTIC_H