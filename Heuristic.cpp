// Heuristic.cpp

#include "stdafx.h"

#include "Heuristic.h"

#include "Board.h"
#include "Def.h"
#include "Utils.h"

#ifdef COMMON_HEURISTIC_TABLE
volatile int HeuristicTable[2][6][64]; // [Color][Piece][Square]
#endif // COMMON_HEURISTIC_TABLE

#ifdef COMMON_KILLER_MOVE_TABLE
volatile int KillerMoveTable[MAX_PLY + 1][2]; // [Max. ply + 1][Two killer moves]
#endif // COMMON_KILLER_MOVE_TABLE

#ifdef COMMON_COUNTER_MOVE_TABLE
volatile int CounterMoveTable[2][6][64]; // [Color][Piece][Square]
#endif // COMMON_COUNTER_MOVE_TABLE

#ifdef MOVES_SORT_HEURISTIC

void UpdateHeuristic(BoardItem* Board, const int Move, const int Bonus)
{
#ifdef COMMON_HEURISTIC_TABLE
    HeuristicTable[Board->CurrentColor][PIECE(Board->Pieces[MOVE_FROM(Move)])][MOVE_TO(Move)] += Bonus - HeuristicTable[Board->CurrentColor][PIECE(Board->Pieces[MOVE_FROM(Move)])][MOVE_TO(Move)] * ABS(Bonus) / MAX_HEURISTIC_SCORE;
#else
    Board->HeuristicTable[Board->CurrentColor][PIECE(Board->Pieces[MOVE_FROM(Move)])][MOVE_TO(Move)] += Bonus - Board->HeuristicTable[Board->CurrentColor][PIECE(Board->Pieces[MOVE_FROM(Move)])][MOVE_TO(Move)] * ABS(Bonus) / MAX_HEURISTIC_SCORE;
#endif // COMMON_HEURISTIC_TABLE
}

void ClearHeuristic(BoardItem* Board)
{
#ifdef COMMON_HEURISTIC_TABLE
    for (int Color = 0; Color < 2; ++Color) {
        for (int Piece = 0; Piece < 6; ++Piece) {
            for (int Square = 0; Square < 64; ++Square) {
                HeuristicTable[Color][Piece][Square] = 0;
            }
        }
    }
#else
    memset(Board->HeuristicTable, 0, sizeof(Board->HeuristicTable));
#endif // COMMON_HEURISTIC_TABLE
}

#endif // MOVES_SORT_HEURISTIC

#ifdef KILLER_MOVE

void UpdateKillerMove(BoardItem* Board, const int Move, const int Ply)
{
#ifdef KILLER_MOVE_2
    int TempMove;
#endif // KILLER_MOVE_2

#ifdef COMMON_KILLER_MOVE_TABLE
    if (KillerMoveTable[Ply][0] != Move) {
#ifdef KILLER_MOVE_2
        TempMove = KillerMoveTable[Ply][0];
#endif // KILLER_MOVE_2

        KillerMoveTable[Ply][0] = Move;

#ifdef KILLER_MOVE_2
        KillerMoveTable[Ply][1] = TempMove;
#endif // KILLER_MOVE_2
    }
#else
    if (Board->KillerMoveTable[Ply][0] != Move) {
#ifdef KILLER_MOVE_2
        TempMove = Board->KillerMoveTable[Ply][0];
#endif // KILLER_MOVE_2

        Board->KillerMoveTable[Ply][0] = Move;

#ifdef KILLER_MOVE_2
        Board->KillerMoveTable[Ply][1] = TempMove;
#endif // KILLER_MOVE_2
    }
#endif // COMMON_KILLER_MOVE_TABLE
}

void ClearKillerMove(BoardItem* Board)
{
#ifdef COMMON_KILLER_MOVE_TABLE
    for (int Ply = 0; Ply < MAX_PLY + 1; ++Ply) {
        KillerMoveTable[Ply][0] = 0;
        KillerMoveTable[Ply][1] = 0;
    }
#else
    memset(Board->KillerMoveTable, 0, sizeof(Board->KillerMoveTable));
#endif // COMMON_KILLER_MOVE_TABLE
}

#endif // KILLER_MOVE

#ifdef COUNTER_MOVE

void UpdateCounterMove(BoardItem* Board, const int Move)
{
    if (Board->HalfMoveNumber == 0) {
        return;
    }

    HistoryItem* Info = &Board->MoveTable[Board->HalfMoveNumber - 1]; // Prev. move info

    if (Info->Type == MOVE_NULL) {
        return;
    }

#ifdef COMMON_COUNTER_MOVE_TABLE
    CounterMoveTable[CHANGE_COLOR(Board->CurrentColor)][Info->PieceFrom][Info->To] = Move;
#else
    Board->CounterMoveTable[CHANGE_COLOR(Board->CurrentColor)][Info->PieceFrom][Info->To] = Move;
#endif // COMMON_COUNTER_MOVE_TABLE
}

void ClearCounterMove(BoardItem* Board)
{
#ifdef COMMON_HEURISTIC_TABLE
    for (int Color = 0; Color < 2; ++Color) {
        for (int Piece = 0; Piece < 6; ++Piece) {
            for (int Square = 0; Square < 64; ++Square) {
                CounterMoveTable[Color][Piece][Square] = 0;
            }
        }
    }
#else
    memset(Board->CounterMoveTable, 0, sizeof(Board->CounterMoveTable));
#endif // COMMON_HEURISTIC_TABLE
}

#endif // COUNTER_MOVE