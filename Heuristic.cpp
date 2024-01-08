// Heuristic.cpp

#include "stdafx.h"

#include "Heuristic.h"

#include "Board.h"
#include "Def.h"
#include "Utils.h"

#ifdef COMMON_HEURISTIC_TABLE

volatile int HeuristicTable[2][6][64]; // [Color][Piece][Square]

#ifdef COMMON_COUNTER_MOVE_HISTORY_TABLE
volatile int CounterMoveHistoryTable[6][64][6 * 64]; // [Piece][Square][Piece * Square]
#endif // COMMON_COUNTER_MOVE_HISTORY_TABLE

#endif // COMMON_HEURISTIC_TABLE

#ifdef COMMON_KILLER_MOVE_TABLE
volatile int KillerMoveTable[MAX_PLY + 1][2]; // [Max. ply + 1][Two killer moves]
#endif // COMMON_KILLER_MOVE_TABLE

#ifdef COMMON_COUNTER_MOVE_TABLE
volatile int CounterMoveTable[2][6][64]; // [Color][Piece][Square]
#endif // COMMON_COUNTER_MOVE_TABLE

#ifdef MOVES_SORT_HEURISTIC

void UpdateHeuristic(BoardItem* Board, int** CMH_Pointer, const int Move, const int Bonus)
{
#ifdef COMMON_HEURISTIC_TABLE
    HeuristicTable[Board->CurrentColor][PIECE(Board->Pieces[MOVE_FROM(Move)])][MOVE_TO(Move)] += Bonus - HeuristicTable[Board->CurrentColor][PIECE(Board->Pieces[MOVE_FROM(Move)])][MOVE_TO(Move)] * ABS(Bonus) / MAX_HEURISTIC_SCORE;
#else
    Board->HeuristicTable[Board->CurrentColor][PIECE(Board->Pieces[MOVE_FROM(Move)])][MOVE_TO(Move)] += Bonus - Board->HeuristicTable[Board->CurrentColor][PIECE(Board->Pieces[MOVE_FROM(Move)])][MOVE_TO(Move)] * ABS(Bonus) / MAX_HEURISTIC_SCORE;
#endif // COMMON_HEURISTIC_TABLE

#ifdef COUNTER_MOVE_HISTORY
    if (CMH_Pointer) {
        if (CMH_Pointer[0]) {
            CMH_Pointer[0][(PIECE(Board->Pieces[MOVE_FROM(Move)]) << 6) + MOVE_TO(Move)] += Bonus - CMH_Pointer[0][(PIECE(Board->Pieces[MOVE_FROM(Move)]) << 6) + MOVE_TO(Move)] * ABS(Bonus) / MAX_HEURISTIC_SCORE;
        }

        if (CMH_Pointer[1]) {
            CMH_Pointer[1][(PIECE(Board->Pieces[MOVE_FROM(Move)]) << 6) + MOVE_TO(Move)] += Bonus - CMH_Pointer[1][(PIECE(Board->Pieces[MOVE_FROM(Move)]) << 6) + MOVE_TO(Move)] * ABS(Bonus) / MAX_HEURISTIC_SCORE;
        }
    }
#endif // COUNTER_MOVE_HISTORY
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

#ifdef COUNTER_MOVE_HISTORY

#ifdef COMMON_COUNTER_MOVE_HISTORY_TABLE
    for (int Piece = 0; Piece < 6; ++Piece) {
        for (int Square = 0; Square < 64; ++Square) {
            for (int PieceSquare = 0; PieceSquare < 6 * 64; ++PieceSquare) {
                CounterMoveHistoryTable[Piece][Square][PieceSquare] = 0;
            }
        }
    }
#else
    memset(Board->CounterMoveHistoryTable, 0, sizeof(Board->CounterMoveHistoryTable));
#endif // COMMON_COUNTER_MOVE_HISTORY_TABLE

#endif // COUNTER_MOVE_HISTORY
}

#ifdef COUNTER_MOVE_HISTORY
void SetCounterMoveHistoryPointer(BoardItem* Board, int** CMH_Pointer)
{
    HistoryItem* Info;

    if (Board->HalfMoveNumber == 0) {
        CMH_Pointer[0] = CMH_Pointer[1] = NULL;

        return;
    }

    Info = &Board->MoveTable[Board->HalfMoveNumber - 1]; // Prev. move info

    if (Info->Type == MOVE_NULL) {
        CMH_Pointer[0] = CMH_Pointer[1] = NULL;

        return;
    }

#ifdef COMMON_COUNTER_MOVE_HISTORY_TABLE
    CMH_Pointer[0] = CounterMoveHistoryTable[Info->PieceFrom][Info->To];
#else
    CMH_Pointer[0] = Board->CounterMoveHistoryTable[Info->PieceFrom][Info->To];
#endif // COMMON_COUNTER_MOVE_HISTORY_TABLE

    if (Board->HalfMoveNumber == 1) {
        CMH_Pointer[1] = NULL;

        return;
    }

    Info = &Board->MoveTable[Board->HalfMoveNumber - 2]; // Prev. prev. move info

    if (Info->Type == MOVE_NULL) {
        CMH_Pointer[1] = NULL;

        return;
    }

#ifdef COMMON_COUNTER_MOVE_HISTORY_TABLE
    CMH_Pointer[1] = CounterMoveHistoryTable[Info->PieceFrom][Info->To];
#else
    CMH_Pointer[1] = Board->CounterMoveHistoryTable[Info->PieceFrom][Info->To];
#endif // COMMON_COUNTER_MOVE_HISTORY_TABLE
}
#endif // COUNTER_MOVE_HISTORY

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