// Sort.h

#pragma once

#ifndef SORT_H
#define SORT_H

#include "Board.h"
#include "Def.h"

#define SORT_PVS_MOVE_VALUE				(1 << 30)
#define SORT_HASH_MOVE_VALUE			(1 << 29)

#define SORT_PAWN_PROMOTE_MOVE_BONUS	(1 << 28)
#define SORT_CAPTURE_MOVE_BONUS			(1 << 27)

#define SORT_KILLER_MOVE_1_VALUE		(1 << 26)
#define SORT_KILLER_MOVE_2_VALUE		(SORT_KILLER_MOVE_1_VALUE - 1)

#define SORT_COUNTER_MOVE_VALUE			(SORT_KILLER_MOVE_2_VALUE - 1)

#if defined(PVS) || defined(QUIESCENCE_PVS)
void SetPvsMoveSortValue(BoardItem* Board, const int Ply, MoveItem* GenMoveList, const int GenMoveCount);
#endif // PVS || QUIESCENCE_PVS

#if defined(HASH_MOVE) || defined(QUIESCENCE_HASH_MOVE)
void SetHashMoveSortValue(MoveItem* GenMoveList, const int GenMoveCount, const int HashMove);
#endif // HASH_MOVE || QUIESCENCE_HASH_MOVE

#ifdef KILLER_MOVE

void SetKillerMove1SortValue(const BoardItem* Board, const int Ply, MoveItem* GenMoveList, const int GenMoveCount, const int HashMove);

#ifdef KILLER_MOVE_2
void SetKillerMove2SortValue(const BoardItem* Board, const int Ply, MoveItem* GenMoveList, const int GenMoveCount, const int HashMove);
#endif // KILLER_MOVE_2

#endif // KILLER_MOVE

#ifdef COUNTER_MOVE
void SetCounterMoveSortValue(const BoardItem* Board, const int Ply, MoveItem* GenMoveList, const int GenMoveCount, const int HashMove);
#endif // COUNTER_MOVE

#if defined(MOVES_SORT_SEE) || defined(MOVES_SORT_MVV_LVA) || defined(MOVES_SORT_HEURISTIC) || defined(MOVES_SORT_SQUARE_SCORE)
void PrepareNextMove(const int StartIndex, MoveItem* GenMoveList, const int GenMoveCount);
#endif // MOVES_SORT_SEE || MOVES_SORT_MVV_LVA || MOVES_SORT_HEURISTIC || MOVES_SORT_SQUARE_SCORE

#endif // !SORT_H