// Sort.cpp

#include "stdafx.h"

#include "Sort.h"

#include "Board.h"
#include "Def.h"
#include "Heuristic.h"

#if defined(PVS) || defined(QUIESCENCE_PVS)
void SetPvsMoveSortValue(BoardItem* Board, const int Ply, MoveItem* GenMoveList, const int GenMoveCount)
{
    if (Board->FollowPV) {
        Board->FollowPV = FALSE;

        if (Board->BestMovesRoot[Ply].Move) {
            for (int Index = 0; Index < GenMoveCount; ++Index) {
                if (GenMoveList[Index].Move == Board->BestMovesRoot[Ply].Move) {
#ifdef DEBUG_PVS
                    printf("-- PVS: Move = %s%s\n", BoardName[MOVE_FROM(GenMoveList[Index].Move)], BoardName[MOVE_TO(GenMoveList[Index].Move)]);
#endif // DEBUG_PVS

                    GenMoveList[Index].SortValue = SORT_PVS_MOVE_VALUE;

                    Board->FollowPV = TRUE;

                    break;
                }
            }
        }
    }
}
#endif // PVS || QUIESCENCE_PVS

#if defined(HASH_MOVE) || defined(QUIESCENCE_HASH_MOVE)
void SetHashMoveSortValue(MoveItem* GenMoveList, const int GenMoveCount, const int HashMove)
{
    if (HashMove) {
        for (int Index = 0; Index < GenMoveCount; ++Index) {
            if (GenMoveList[Index].Move == HashMove) {
                GenMoveList[Index].SortValue = SORT_HASH_MOVE_VALUE;

                break;
            }
        }
    }
}
#endif // HASH_MOVE || QUIESCENCE_HASH_MOVE

#ifdef KILLER_MOVE

void SetKillerMove1SortValue(const BoardItem* Board, const int Ply, MoveItem* GenMoveList, const int GenMoveCount, const int HashMove)
{
    int KillerMove1 = Board->KillerMoveTable[Ply][0];

    if (KillerMove1 && KillerMove1 != HashMove) {
        for (int Index = 0; Index < GenMoveCount; ++Index) {
            if (GenMoveList[Index].Move == KillerMove1) {
                if (!(GenMoveList[Index].Type & (MOVE_CAPTURE | MOVE_PAWN_PROMOTE))) { // Not capture/promote move
                    GenMoveList[Index].SortValue = SORT_KILLER_MOVE_1_VALUE;
                }

                break;
            }
        }
    }
}

#ifdef KILLER_MOVE_2
void SetKillerMove2SortValue(const BoardItem* Board, const int Ply, MoveItem* GenMoveList, const int GenMoveCount, const int HashMove)
{
    int KillerMove2 = Board->KillerMoveTable[Ply][1];

    if (KillerMove2 && KillerMove2 != HashMove) {
        for (int Index = 0; Index < GenMoveCount; ++Index) {
            if (GenMoveList[Index].Move == KillerMove2) {
                if (!(GenMoveList[Index].Type & (MOVE_CAPTURE | MOVE_PAWN_PROMOTE))) { // Not capture/promote move
                    GenMoveList[Index].SortValue = SORT_KILLER_MOVE_2_VALUE;
                }

                break;
            }
        }
    }
}
#endif // KILLER_MOVE_2

#endif // KILLER_MOVE

#ifdef COUNTER_MOVE
void SetCounterMoveSortValue(const BoardItem* Board, const int Ply, MoveItem* GenMoveList, const int GenMoveCount, const int HashMove)
{
    if (Board->HalfMoveNumber == 0) {
        return;
    }

    const HistoryItem* Info = &Board->MoveTable[Board->HalfMoveNumber - 1]; // Prev. move info

    if (Info->Type == MOVE_NULL) {
        return;
    }

    int CounterMove = Board->CounterMoveTable[CHANGE_COLOR(Board->CurrentColor)][Info->PieceFrom][Info->To];

    int KillerMove1 = Board->KillerMoveTable[Ply][0];
    int KillerMove2 = Board->KillerMoveTable[Ply][1];

    if (CounterMove && CounterMove != HashMove && CounterMove != KillerMove1 && CounterMove != KillerMove2) {
        for (int Index = 0; Index < GenMoveCount; ++Index) {
            if (GenMoveList[Index].Move == CounterMove) {
                if (!(GenMoveList[Index].Type & (MOVE_CAPTURE | MOVE_PAWN_PROMOTE))) { // Not capture/promote move
                    GenMoveList[Index].SortValue = SORT_COUNTER_MOVE_VALUE;
                }

                break;
            }
        }
    }
}
#endif // COUNTER_MOVE

#if defined(MOVES_SORT_SEE) || defined(MOVES_SORT_MVV_LVA) || defined(MOVES_SORT_HEURISTIC)
void PrepareNextMove(const int StartIndex, MoveItem* GenMoveList, const int GenMoveCount)
{
    int BestMoveIndex = StartIndex;
    int BestMoveScore = GenMoveList[StartIndex].SortValue;

    MoveItem TempMoveItem;

    for (int Index = StartIndex + 1; Index < GenMoveCount; ++Index) {
        if (GenMoveList[Index].SortValue > BestMoveScore) {
            BestMoveIndex = Index;
            BestMoveScore = GenMoveList[Index].SortValue;
        }
    }

    if (StartIndex != BestMoveIndex) {
        TempMoveItem = GenMoveList[StartIndex];
        GenMoveList[StartIndex] = GenMoveList[BestMoveIndex];
        GenMoveList[BestMoveIndex] = TempMoveItem;
    }
}
#endif // MOVES_SORT_SEE || MOVES_SORT_MVV_LVA || MOVES_SORT_HEURISTIC