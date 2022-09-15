// Evaluate.cpp

#include "stdafx.h"

#include "Evaluate.h"

#include "BitBoard.h"
#include "Board.h"
#include "Def.h"
#include "NNUE.h"
#include "NNUE2.h"
#include "Types.h"
#include "Utils.h"

#ifdef SIMPLIFIED_EVALUATION_FUNCTION

// Material

int PiecesScore[6] = { 100, 320, 330, 500, 900, 0 }; // PNBRQK

// Piece-square tables

int PawnSquareScore[64] = {
     0,  0,   0,   0,   0,   0,  0,  0,
    50, 50,  50,  50,  50,  50, 50, 50,
    10, 10,  20,  30,  30,  20, 10, 10,
     5,  5,  10,  25,  25,  10,  5,  5,
     0,  0,   0,  20,  20,   0,  0,  0,
     5, -5, -10,   0,   0, -10, -5,  5,
     5, 10,  10, -20, -20,  10, 10,  5,
     0,  0,   0,   0,   0,   0,  0,  0
};

int KnightSquareScore[64] = {
    -50, -40, -30, -30, -30, -30, -40, -50,
    -40, -20,   0,   0,   0,   0, -20, -40,
    -30,   0,  10,  15,  15,  10,   0, -30,
    -30,   5,  15,  20,  20,  15,   5, -30,
    -30,   0,  15,  20,  20,  15,   0, -30,
    -30,   5,  10,  15,  15,  10,   5, -30,
    -40, -20,   0,   5,   5,   0, -20, -40,
    -50, -40, -30, -30, -30, -30, -40, -50
};

int BishopSquareScore[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   5,   5,  10,  10,   5,   5, -10,
    -10,   0,  10,  10,  10,  10,   0, -10,
    -10,  10,  10,  10,  10,  10,  10, -10,
    -10,   5,   0,   0,   0,   0,   5, -10,
    -20, -10, -10, -10, -10, -10, -10, -20
};

int RookSquareScore[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0
};

int QueenSquareScore[64] = {
    -20, -10, -10, -5, -5, -10, -10, -20,
    -10,   0,   0,  0,  0,   0,   0, -10,
    -10,   0,   5,  5,  5,   5,   0, -10,
     -5,   0,   5,  5,  5,   5,   0,  -5,
      0,   0,   5,  5,  5,   5,   0,  -5,
    -10,   5,   5,  5,  5,   5,   0, -10,
    -10,   0,   5,  0,  0,   0,   0, -10,
    -20, -10, -10, -5, -5, -10, -10, -20
};

int KingSquareScoreOpening[64] = {
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -20, -30, -30, -40, -40, -30, -30, -20,
    -10, -20, -20, -20, -20, -20, -20, -10,
     20,  20,   0,   0,   0,   0,  20,  20,
     20,  30,  10,   0,   0,  10,  30,  20
};

int KingSquareScoreEnding[64] = {
    -50, -40, -30, -20, -20, -30, -40, -50,
    -30, -20, -10,   0,   0, -10, -20, -30,
    -30, -10,  20,  30,  30,  20, -10, -30,
    -30, -10,  30,  40,  40,  30, -10, -30,
    -30, -10,  30,  40,  40,  30, -10, -30,
    -30, -10,  20,  30,  30,  20, -10, -30,
    -30, -30,   0,   0,   0,   0, -30, -30,
    -50, -30, -30, -30, -30, -30, -30, -50
};

// ------------------------------------------------------------------

void InitEvaluation(void)
{
    // Integrate material with piece-square tables
    for (int Square = 0; Square < 64; ++Square) {
        PawnSquareScore[Square] += PiecesScore[PAWN];
        KnightSquareScore[Square] += PiecesScore[KNIGHT];
        BishopSquareScore[Square] += PiecesScore[BISHOP];
        RookSquareScore[Square] += PiecesScore[ROOK];
        QueenSquareScore[Square] += PiecesScore[QUEEN];

        KingSquareScoreOpening[Square] += PiecesScore[KING];
        KingSquareScoreEnding[Square] += PiecesScore[KING];
    }
}

BOOL IsEndGame(const BoardItem* Board)
{
    if (!Board->BB_Pieces[WHITE][QUEEN] && !Board->BB_Pieces[BLACK][QUEEN]) {
        return TRUE;
    }

    if (Board->BB_Pieces[WHITE][QUEEN] && !Board->BB_Pieces[WHITE][ROOK] && POPCNT(Board->BB_Pieces[WHITE][KNIGHT] | Board->BB_Pieces[WHITE][BISHOP]) <= 1) {
        return TRUE;
    }

    if (Board->BB_Pieces[BLACK][QUEEN] && !Board->BB_Pieces[BLACK][ROOK] && POPCNT(Board->BB_Pieces[BLACK][KNIGHT] | Board->BB_Pieces[BLACK][BISHOP]) <= 1) {
        return TRUE;
    }

    return FALSE;
}

int Evaluate(BoardItem* Board)
{
    int Score = 0;

    U64 Pieces;

    int Square;

#ifdef DEBUG_STATISTIC
    ++Board->EvaluateCount;
#endif // DEBUG_STATISTIC

    // White pieces

    Pieces = Board->BB_WhitePieces;

    while (Pieces) {
        Square = LSB(Pieces);

        switch (PIECE(Board->Pieces[Square])) {
        case PAWN:
            Score += PawnSquareScore[Square];
            break;

        case KNIGHT:
            Score += KnightSquareScore[Square];
            break;

        case BISHOP:
            Score += BishopSquareScore[Square];
            break;

        case ROOK:
            Score += RookSquareScore[Square];
            break;

        case QUEEN:
            Score += QueenSquareScore[Square];
            break;

        case KING:
            if (IsEndGame(Board)) { // End game
                Score += KingSquareScoreEnding[Square];
            }
            else { // Open/Middle game
                Score += KingSquareScoreOpening[Square];
            }

            break;
        }

        Pieces &= Pieces - 1;
    }

    // Black pieces

    Pieces = Board->BB_BlackPieces;

    while (Pieces) {
        Square = LSB(Pieces);

        switch (PIECE(Board->Pieces[Square])) {
        case PAWN:
            Score -= PawnSquareScore[Square ^ 56];
            break;

        case KNIGHT:
            Score -= KnightSquareScore[Square ^ 56];
            break;

        case BISHOP:
            Score -= BishopSquareScore[Square ^ 56];
            break;

        case ROOK:
            Score -= RookSquareScore[Square ^ 56];
            break;

        case QUEEN:
            Score -= QueenSquareScore[Square ^ 56];
            break;

        case KING:
            if (IsEndGame(Board)) { // End game
                Score -= KingSquareScoreEnding[Square ^ 56];
            }
            else { // Open/Middle game
                Score -= KingSquareScoreOpening[Square ^ 56];
            }

            break;
        }

        Pieces &= Pieces - 1;
    }

    // Perspective

    if (Board->CurrentColor == BLACK) {
        Score = -Score;
    }

    return Score;
}

#elif defined(TOGA_EVALUATION_FUNCTION)

// Game phase (4.1)

const int MaxGamePhase = 24; // 1 x 4N + 1 x 4B + 2 x 4R + 4 x 2Q

// Material (4.2)

SCORE PiecesScoreOpening[6] = { 70, 325, 325, 500, 975, 0 };    // PNBRQK
SCORE PiecesScoreEnding[6] = { 90, 325, 325, 500, 975, 0 };     // PNBRQK

SCORE BishopPairOpening = 50;
SCORE BishopPairEnding = 50;

// Piece-square tables (4.3)

SCORE PawnSquareScoreOpening[64] = {
    -15, -5, 0,  5,  5, 0, -5, -15,
    -15, -5, 0,  5,  5, 0, -5, -15,
    -15, -5, 0,  5,  5, 0, -5, -15,
    -15, -5, 0, 15, 15, 0, -5, -15,
    -15, -5, 0, 25, 25, 0, -5, -15,
    -15, -5, 0, 15, 15, 0, -5, -15,
    -15, -5, 0,  5,  5, 0, -5, -15,
    -15, -5, 0,  5,  5, 0, -5, -15
};

SCORE PawnSquareScoreEnding[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

SCORE KnightSquareScoreOpening[64] = {
    -135, -25, -15, -10, -10, -15, -25, -135,
     -20, -10,   0,   5,   5,   0, -10,  -20,
      -5,   5,  15,  20,  20,  15,   5,   -5,
      -5,   5,  15,  20,  20,  15,   5,   -5,
     -10,   0,  10,  15,  15,  10,   0,  -10,
     -20, -10,   0,   5,   5,   0, -10,  -20,
     -35, -25, -15, -10, -10, -15, -25,  -35,
     -50, -40, -30, -25, -25, -30, -40,  -50
};

SCORE KnightSquareScoreEnding[64] = {
    -40, -30, -20, -15, -15, -20, -30, -40,
    -30, -20, -10,  -5,  -5, -10, -20, -30,
    -20, -10,   0,   5,   5,   0, -10, -20,
    -15,  -5,   5,  10,  10,   5,  -5, -15,
    -15,  -5,   5,  10,  10,   5,  -5, -15,
    -20, -10,   0,   5,   5,   0, -10, -20,
    -30, -20, -10,  -5,  -5, -10, -20, -30,
    -40, -30, -20, -15, -15, -20, -30, -40
};

SCORE BishopSquareScoreOpening[64] = {
     -8,  -8,  -6,  -4,  -4,  -6,  -8,  -8,
     -8,   0,  -2,   0,   0,  -2,   0,  -8,
     -6,  -2,   4,   2,   2,   4,  -2,  -6,
     -4,   0,   2,   8,   8,   2,   0,  -4,
     -4,   0,   2,   8,   8,   2,   0,  -4,
     -6,  -2,   4,   2,   2,   4,  -2,  -6,
     -8,   0,  -2,   0,   0,  -2,   0,  -8,
    -18, -18, -16, -14, -14, -16, -18, -18
};

SCORE BishopSquareScoreEnding[64] = {
    -18, -12, -9, -6, -6, -9, -12, -18,
    -12,  -6, -3,  0,  0, -3,  -6, -12,
     -9,  -3,  0,  3,  3,  0,  -3,  -9,
     -6,   0,  3,  6,  6,  3,   0,  -6,
     -6,   0,  3,  6,  6,  3,   0,  -6,
     -9,  -3,  0,  3,  3,  0,  -3,  -9,
    -12,  -6, -3,  0,  0, -3,  -6, -12,
    -18, -12, -9, -6, -6, -9, -12, -18
};

SCORE RookSquareScoreOpening[64] = {
    -6, -3, 0, 3, 3, 0, -3, -6,
    -6, -3, 0, 3, 3, 0, -3, -6,
    -6, -3, 0, 3, 3, 0, -3, -6,
    -6, -3, 0, 3, 3, 0, -3, -6,
    -6, -3, 0, 3, 3, 0, -3, -6,
    -6, -3, 0, 3, 3, 0, -3, -6,
    -6, -3, 0, 3, 3, 0, -3, -6,
    -6, -3, 0, 3, 3, 0, -3, -6
};

SCORE RookSquareScoreEnding[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

SCORE QueenSquareScoreOpening[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
    -5, -5, -5, -5, -5, -5, -5, -5
};

SCORE QueenSquareScoreEnding[64] = {
    -24, -16, -12, -8, -8, -12, -16, -24,
    -16,  -8,  -4,  0,  0,  -4,  -8, -16,
    -12,  -4,   0,  4,  4,   0,  -4, -12,
     -8,   0,   4,  8,  8,   4,   0,  -8,
     -8,   0,   4,  8,  8,   4,   0,  -8,
    -12,  -4,   0,  4,  4,   0,  -4, -12,
    -16,  -8,  -4,  0,  0,  -4,  -8, -16,
    -24, -16, -12, -8, -8, -12, -16, -24
};

SCORE KingSquareScoreOpening[64] = {
    -40, -30, -50, -70, -70, -50, -30, -40,
    -30, -20, -40, -60, -60, -40, -20, -30,
    -20, -10, -30, -50, -50, -30, -10, -20,
    -10,   0, -20, -40, -40, -20,   0, -10,
      0,  10, -10, -30, -30, -10,  10,   0,
     10,  20,   0, -20, -20,   0,  20,  10,
     30,  40,  20,   0,   0,  20,  40,  30,
     40,  50,  30,  10,  10,  30,  50,  40
};

SCORE KingSquareScoreEnding[64] = {
    -72, -48, -36, -24, -24, -36, -48, -72,
    -48, -24, -12,   0,   0, -12, -24, -48,
    -36, -12,   0,  12,  12,   0, -12, -36,
    -24,   0,  12,  24,  24,  12,   0, -24,
    -24,   0,  12,  24,  24,  12,   0, -24,
    -36, -12,   0,  12,  12,   0, -12, -36,
    -48, -24, -12,   0,   0, -12, -24, -48,
    -72, -48, -36, -24, -24, -36, -48, -72
};

// Pawns (4.4)

SCORE PawnDoubledOpening = -10;
SCORE PawnDoubledEnding = -20;

SCORE PawnIsolatedOpening = -10;
SCORE PawnIsolatedEnding = -20;

SCORE PawnIsolatedOpenOpening = -20;
SCORE PawnIsolatedOpenEnding = -20;

SCORE PawnBackwardOpening = -8;
SCORE PawnBackwardEnding = -10;

SCORE PawnBackwardOpenOpening = -16;
SCORE PawnBackwardOpenEnding = -10;

SCORE PawnCandidateOpening[8] = { 0, 5, 5, 10, 20, 35, 55, 0 };
SCORE PawnCandidateEnding[8] = { 0, 10, 10, 20, 40, 70, 110, 0 };

// Tempo (4.5)

SCORE TempoOpening = 20;
SCORE TempoEnding = 10;

// Pattern (4.6)

SCORE TrappedBishopOpening = -100;
SCORE TrappedBishopEnding = -100;

SCORE BlockedBishopOpening = -50;
SCORE BlockedBishopEnding = -50;

SCORE BlockedRookOpening = -50;
SCORE BlockedRookEnding = -50;

// Piece (4.7)

// Mobility (4.7.1)

SCORE KnightMobilityMoveDecrease = 4;
SCORE BishopMobilityMoveDecrease = 6;
SCORE RookMobilityMoveDecrease = 7;

SCORE KnightMobility = 4;
SCORE BishopMobility = 5;

SCORE RookMobilityOpening = 2;
SCORE RookMobilityEnding = 4;

// Open file (4.7.2)

SCORE RookOnClosedFileOpening = -10;
SCORE RookOnClosedFileEnding = -10;

SCORE RookOnSemiOpenFileOpening = 0;
SCORE RookOnSemiOpenFileEnding = 0;

SCORE RookOnSemiOpenFileAdjacentToEnemyKingOpening = 10;
SCORE RookOnSemiOpenFileAdjacentToEnemyKingEnding = 0;

SCORE RookOnSemiOpenFileSameToEnemyKingOpening = 20;
SCORE RookOnSemiOpenFileSameToEnemyKingEnding = 0;

SCORE RookOnOpenFileOpening = 10;
SCORE RookOnOpenFileEnding = 10;

SCORE RookOnOpenFileAdjacentToEnemyKingOpening = 20;
SCORE RookOnOpenFileAdjacentToEnemyKingEnding = 10;

SCORE RookOnOpenFileSameToEnemyKingOpening = 30;
SCORE RookOnOpenFileSameToEnemyKingEnding = 10;

// Outpost (4.7.3)

SCORE KnightOutpost[64] = {
    0, 0, 0,  0,  0, 0, 0, 0,
    0, 0, 0,  0,  0, 0, 0, 0,
    0, 0, 4,  5,  5, 4, 0, 0,
    0, 2, 5, 10, 10, 5, 2, 0,
    0, 2, 5, 10, 10, 5, 2, 0,
    0, 0, 0,  0,  0, 0, 0, 0,
    0, 0, 0,  0,  0, 0, 0, 0,
    0, 0, 0,  0,  0, 0, 0, 0
};

// Seventh rank (4.7.4)

SCORE RookOnSeventhOpening = 20;
SCORE RookOnSeventhEnding = 40;

SCORE QueenOnSeventhOpening = 10;
SCORE QueenOnSeventhEnding = 20;

// King distance (4.7.5)

// King (4.8)

// Pawn shelter (4.8.1)

SCORE KingFriendlyPawnAdvanceBase = -36;
SCORE KingFriendlyPawnAdvanceDefault = -11;

// Pawn storm (4.8.2)

SCORE KingPawnStorm[8] = { 0, 0, 0, -10, -30, -60, 0, 0 };

// Piece attack (4.8.3)

SCORE KingZoneAttackedBase = -20;
SCORE KingZoneAttackedWeight[8] = { 0, 0, 50, 75, 88, 94, 97, 99 }; // %

// Passed pawns (4.9)

SCORE PawnPassedEndingBase1 = 20;
SCORE PawnPassedEndingBase2 = 120;

SCORE PawnPassedEndingHostileKingDistance = 20;
SCORE PawnPassedEndingFriendlyKingDistance = -5;

SCORE PawnPassedEndingConsideredFree = 60;

SCORE PawnPassedEndingUnstoppable = 800;

SCORE PawnPassedOpening[8] = { 0, 10, 10, 16, 28, 46, 70, 0 };
SCORE PawnPassedEndingWeight[8] = { 0, 0, 0, 10, 30, 60, 100, 0 }; // %

// ------------------------------------------------------------------

U64 PawnPassedMask[2][64];      // [Color][Square]
U64 PawnDoubledMask[2][64];     // [Color][Square]

U64 PawnFileMask[8];            // [File]
U64 PawnIsolatedMask[8];        // [File]

int Distance[64][64];           // [Square][Square]

void InitEvaluation(void)
{
    int File;
    int Rank;

    int Direction;

    U64 SquareMask;

    U64 PassedMask;
    U64 DoubledMask;

    U64 FileMask;
    U64 IsolatedMask;

    int FileDiff;
    int RankDiff;

    // Fill pawn passed and doubled masks
    for (int Color = 0; Color < 2; ++Color) {
        for (int Square = 0; Square < 64; ++Square) {
            File = FILE(Square);
            Rank = RANK(Square);

            Direction = (Color == WHITE) ? -1 : 1;

            PassedMask = 0ULL;
            DoubledMask = 0ULL;

            for (int File2 = File - 1; File2 <= File + 1; ++File2) {
                if (File2 >= 0 && File2 <= 7) {
                    for (int Rank2 = Rank + Direction; Rank2 >= 0 && Rank2 <= 7; Rank2 += Direction) {
                        SquareMask = BB_SQUARE(SQUARE(Rank2, File2));

                        PassedMask |= SquareMask;

                        if (File2 == File) {
                            DoubledMask |= SquareMask;
                        }
                    }
                }
            }

            PawnPassedMask[Color][Square] = PassedMask;
            PawnDoubledMask[Color][Square] = DoubledMask;
        }
    }

    // Fill pawn file and isolated masks
    for (File = 0; File <= 7; ++File) {
        FileMask = 0ULL;
        IsolatedMask = 0ULL;

        for (int File2 = File - 1; File2 <= File + 1; ++File2) {
            if (File2 >= 0 && File2 <= 7) {
                for (int Rank2 = 0; Rank2 <= 7; ++Rank2) {
                    SquareMask = BB_SQUARE(SQUARE(Rank2, File2));

                    if (File2 == File) {
                        FileMask |= SquareMask;
                    }
                    else {
                        IsolatedMask |= SquareMask;
                    }
                }
            }
        }

        PawnFileMask[File] = FileMask;
        PawnIsolatedMask[File] = IsolatedMask;
    }

    // Fill distance array
    for (int Square1 = 0; Square1 < 64; ++Square1) {
        for (int Square2 = 0; Square2 < 64; ++Square2) {
            FileDiff = ABS(FILE(Square1) - FILE(Square2));
            RankDiff = ABS(RANK(Square1) - RANK(Square2));

            Distance[Square1][Square2] = MAX(FileDiff, RankDiff);
        }
    }
}

SCORE EvaluateWhiteKingOpening(const int KingSquare, const U64 WhitePawns, const U64 BlackPawns)
{
    SCORE ScoreOpening = 0;
    SCORE KingScoreOpening = 0;

    int File = FILE(KingSquare);

    int KingPawnDistance;

    for (int File2 = File - 1; File2 <= File + 1; ++File2) {
        if (File2 >= 0 && File2 <= 7) {
            // Pawn shelter (4.8.1)

            if (PawnFileMask[File2] & WhitePawns) {
                KingPawnDistance = RANK(MSB(PawnFileMask[File2] & WhitePawns));

                if (File2 == File) {
                    KingScoreOpening += 2 * (KingFriendlyPawnAdvanceBase + KingPawnDistance * KingPawnDistance);
                }
                else {
                    KingScoreOpening += KingFriendlyPawnAdvanceBase + KingPawnDistance * KingPawnDistance;
                }
            }
            else { // No pawns
                if (File2 == File) {
                    KingScoreOpening += 2 * KingFriendlyPawnAdvanceBase;
                }
                else {
                    KingScoreOpening += KingFriendlyPawnAdvanceBase;
                }
            }

            // Pawn storm (4.8.2)

            if (PawnFileMask[File2] & BlackPawns) {
                ScoreOpening += KingPawnStorm[RANK(MSB(PawnFileMask[File2] & BlackPawns))];
            }
        }
    }

    if (KingScoreOpening == 0) {
        ScoreOpening += KingFriendlyPawnAdvanceDefault;
    }
    else {
        ScoreOpening += KingScoreOpening;
    }

    return ScoreOpening;
}

SCORE EvaluateBlackKingOpening(const int KingSquare, const U64 WhitePawns, const U64 BlackPawns)
{
    SCORE ScoreOpening = 0;
    SCORE KingScoreOpening = 0;

    int File = FILE(KingSquare);

    int KingPawnDistance;

    for (int File2 = File - 1; File2 <= File + 1; ++File2) {
        if (File2 >= 0 && File2 <= 7) {
            // Pawn shelter (4.8.1)

            if (PawnFileMask[File2] & BlackPawns) {
                KingPawnDistance = 7 - RANK(LSB(PawnFileMask[File2] & BlackPawns));

                if (File2 == File) {
                    KingScoreOpening += 2 * (KingFriendlyPawnAdvanceBase + KingPawnDistance * KingPawnDistance);
                }
                else {
                    KingScoreOpening += KingFriendlyPawnAdvanceBase + KingPawnDistance * KingPawnDistance;
                }
            }
            else { // No pawns
                if (File2 == File) {
                    KingScoreOpening += 2 * KingFriendlyPawnAdvanceBase;
                }
                else {
                    KingScoreOpening += KingFriendlyPawnAdvanceBase;
                }
            }

            // Pawn storm (4.8.2)

            if (PawnFileMask[File2] & WhitePawns) {
                ScoreOpening += KingPawnStorm[7 - RANK(LSB(PawnFileMask[File2] & WhitePawns))];
            }
        }
    }

    if (KingScoreOpening == 0) {
        ScoreOpening += KingFriendlyPawnAdvanceDefault;
    }
    else {
        ScoreOpening += KingScoreOpening;
    }

    return ScoreOpening;
}

SCORE Evaluate(BoardItem* Board)
{
    SCORE ScoreOpening = 0;
    SCORE ScoreEnding = 0;

    SCORE Score;

    U64 Pieces;

    U64 Attacks;
    U64 Attacks2;

    int Square;
    int SquareAhead;

    int File;
    int Rank;

    U64 WhitePieces = Board->BB_WhitePieces;
    U64 BlackPieces = Board->BB_BlackPieces;

    U64 AllPieces = (WhitePieces | BlackPieces);

    U64 WhitePawns = Board->BB_Pieces[WHITE][PAWN];
    U64 BlackPawns = Board->BB_Pieces[BLACK][PAWN];

    int WhiteKingSquare = LSB(Board->BB_Pieces[WHITE][KING]);
    int BlackKingSquare = LSB(Board->BB_Pieces[BLACK][KING]);

    U64 WhiteKingZone = KingAttacks(WhiteKingSquare);
    U64 BlackKingZone = KingAttacks(BlackKingSquare);

    SCORE PawnPassedScoreEnding;

    int WhiteKingZoneAttackedCount = 0;
    int BlackKingZoneAttackedCount = 0;

    int WhiteKingZoneAttackedValue = 0;
    int BlackKingZoneAttackedValue = 0;

    SCORE KingOpening;
    SCORE KingOpeningKingSide;
    SCORE KingOpeningQueenSide;

    int Phase;

#ifdef DEBUG_STATISTIC
    ++Board->EvaluateCount;
#endif // DEBUG_STATISTIC

    // Material (4.2): Bishop pair

    if (POPCNT(Board->BB_Pieces[WHITE][BISHOP]) >= 2) {
        ScoreOpening += BishopPairOpening;
        ScoreEnding += BishopPairEnding;
    }

    if (POPCNT(Board->BB_Pieces[BLACK][BISHOP]) >= 2) {
        ScoreOpening -= BishopPairOpening;
        ScoreEnding -= BishopPairEnding;
    }

    // Evaluate white pieces

    Pieces = WhitePieces;

    while (Pieces) {
        Square = LSB(Pieces);

        switch (PIECE(Board->Pieces[Square])) {
        case PAWN:
            // Material (4.2)

            ScoreOpening += PiecesScoreOpening[PAWN];
            ScoreEnding += PiecesScoreEnding[PAWN];

            // Piece-square tables (4.3)

            ScoreOpening += PawnSquareScoreOpening[Square];
            ScoreEnding += PawnSquareScoreEnding[Square];

            // Pawns (4.4)

            SquareAhead = Square - 8;

            File = FILE(Square);
            Rank = RANK(Square);

            if (PawnDoubledMask[WHITE][Square] & WhitePawns) { // Doubled
                ScoreOpening += PawnDoubledOpening;
                ScoreEnding += PawnDoubledEnding;
            }

            if (!(PawnIsolatedMask[File] & WhitePawns)) { // Isolated
                if (!(PawnDoubledMask[WHITE][Square] & (WhitePawns | BlackPawns))) { // Isolated open
                    ScoreOpening += PawnIsolatedOpenOpening;
                    ScoreEnding += PawnIsolatedOpenEnding;
                }
                else {
                    ScoreOpening += PawnIsolatedOpening;
                    ScoreEnding += PawnIsolatedEnding;
                }
            }
            else if (
                !(PawnPassedMask[BLACK][SquareAhead] & PawnIsolatedMask[File] & WhitePawns)
                && (
                    (BB_SQUARE(SquareAhead) & (WhitePawns | BlackPawns))
                    || (PawnAttacks(BB_SQUARE(SquareAhead), WHITE) & BlackPawns)
                )
            ) { // Backward
                if (!(PawnDoubledMask[WHITE][Square] & (WhitePawns | BlackPawns))) { // Backward open
                    ScoreOpening += PawnBackwardOpenOpening;
                    ScoreEnding += PawnBackwardOpenEnding;
                }
                else {
                    ScoreOpening += PawnBackwardOpening;
                    ScoreEnding += PawnBackwardEnding;
                }
            }

            // Passed pawns (4.9)
            // Pawns (4.4): Candidate

            if (!(PawnPassedMask[WHITE][Square] & BlackPawns)) { // Passed pawns
                PawnPassedScoreEnding = PawnPassedEndingBase2;

                // King distance

                PawnPassedScoreEnding += PawnPassedEndingHostileKingDistance * Distance[SquareAhead][BlackKingSquare];
                PawnPassedScoreEnding += PawnPassedEndingFriendlyKingDistance * Distance[SquareAhead][WhiteKingSquare];

                // Free

                if (!(BB_SQUARE(SquareAhead) & AllPieces) && !IsSquareAttacked(Board, SquareAhead, WHITE)) {
                    PawnPassedScoreEnding += PawnPassedEndingConsideredFree;
                }

                // Unstoppable

                if (!Board->BB_Pieces[BLACK][KNIGHT] && !Board->BB_Pieces[BLACK][BISHOP] && !Board->BB_Pieces[BLACK][ROOK] && !Board->BB_Pieces[BLACK][QUEEN]) {
                    if ((KingAttacks(Square) & Board->BB_Pieces[WHITE][KING]) && (KingAttacks(SQUARE(0, FILE(Square))) & Board->BB_Pieces[WHITE][KING])) {
                        PawnPassedScoreEnding += PawnPassedEndingUnstoppable;
                    }
                    else if (Board->CurrentColor == WHITE && !(PawnDoubledMask[WHITE][Square] & AllPieces)) {
                        for (int Square2 = SquareAhead; Square2 >= 0; Square2 -= 8) {
                            if (Distance[Square][Square2] >= Distance[BlackKingSquare][Square2]) {
                                break; // for
                            }

                            if (RANK(Square2) == 0) { // Last iteration
                                PawnPassedScoreEnding += PawnPassedEndingUnstoppable;
                            }
                        }
                    }
                }

                ScoreOpening += PawnPassedOpening[7 - Rank];
                ScoreEnding += PawnPassedEndingBase1 + PawnPassedScoreEnding * PawnPassedEndingWeight[7 - Rank] / 100;
            }
            else if (
                !(PawnDoubledMask[WHITE][Square] & (WhitePawns | BlackPawns))
                && (POPCNT(PawnPassedMask[WHITE][Square] & PawnIsolatedMask[File] & BlackPawns) <= POPCNT(PawnPassedMask[BLACK][SquareAhead] & PawnIsolatedMask[File] & WhitePawns))
                && (POPCNT(PawnAttacks(BB_SQUARE(Square), WHITE) & BlackPawns) <= POPCNT(PawnAttacks(BB_SQUARE(Square), BLACK) & WhitePawns))
            ) { // Candidate
                ScoreOpening += PawnCandidateOpening[7 - Rank];
                ScoreEnding += PawnCandidateEnding[7 - Rank];
            }

            break;

        case KNIGHT:
            // Material (4.2)

            ScoreOpening += PiecesScoreOpening[KNIGHT];
            ScoreEnding += PiecesScoreEnding[KNIGHT];

            // Piece-square tables (4.3)

            ScoreOpening += KnightSquareScoreOpening[Square];
            ScoreEnding += KnightSquareScoreEnding[Square];

            // Mobility (4.7.1)

            Attacks = KnightAttacks(Square);

            ScoreOpening += (POPCNT(Attacks & ~AllPieces) - KnightMobilityMoveDecrease) * KnightMobility;
            ScoreEnding += (POPCNT(Attacks & ~AllPieces) - KnightMobilityMoveDecrease) * KnightMobility;

            // Piece attack (4.8.3)

            if (Attacks & BlackKingZone) {
                BlackKingZoneAttackedCount += 1;
                BlackKingZoneAttackedValue += 1;
            }

            // Outpost (4.7.3)

            if (KnightOutpost[Square]) {
                Attacks2 = PawnAttacks(BB_SQUARE(Square), BLACK) & WhitePawns;

                if (Attacks2) {
                    ScoreOpening += KnightOutpost[Square] * POPCNT(Attacks2);
                    ScoreEnding += KnightOutpost[Square] * POPCNT(Attacks2);
                }
            }

            break;

        case BISHOP:
            // Material (4.2)

            ScoreOpening += PiecesScoreOpening[BISHOP];
            ScoreEnding += PiecesScoreEnding[BISHOP];

            // Piece-square tables (4.3)

            ScoreOpening += BishopSquareScoreOpening[Square];
            ScoreEnding += BishopSquareScoreEnding[Square];

            // Mobility (4.7.1)

            Attacks = BishopAttacks(Square, AllPieces);

            ScoreOpening += (POPCNT(Attacks & ~AllPieces) - BishopMobilityMoveDecrease) * BishopMobility;
            ScoreEnding += (POPCNT(Attacks & ~AllPieces) - BishopMobilityMoveDecrease) * BishopMobility;

            // Piece attack (4.8.3)

            if (Attacks & BlackKingZone) {
                BlackKingZoneAttackedCount += 1;
                BlackKingZoneAttackedValue += 1;
            }

            // Pattern (4.6)

            if (
                (Square == 1 && (BlackPawns & BB_C7)) // Bb8
                || (Square == 6 && (BlackPawns & BB_F7)) // Bg8
                || (Square == 8 && (BlackPawns & BB_B6)) // Ba7
                || (Square == 15 && (BlackPawns & BB_G6)) // Bh7
                || (Square == 16 && (BlackPawns & BB_B5)) // Ba6
                || (Square == 23 && (BlackPawns & BB_G5)) // Bh6
            ) {
                ScoreOpening += TrappedBishopOpening;
                ScoreEnding += TrappedBishopEnding;
            }

            if (
                (Square == 58 && (WhitePawns & BB_D2) && (AllPieces & BB_D3)) // Bc1
                || (Square == 61 && (WhitePawns & BB_E2) && (AllPieces & BB_E3)) // Bf1
            ) {
                ScoreOpening += BlockedBishopOpening;
                ScoreEnding += BlockedBishopEnding;
            }

            break;

        case ROOK:
            // Material (4.2)

            ScoreOpening += PiecesScoreOpening[ROOK];
            ScoreEnding += PiecesScoreEnding[ROOK];

            // Piece-square tables (4.3)

            ScoreOpening += RookSquareScoreOpening[Square];
            ScoreEnding += RookSquareScoreEnding[Square];

            // Mobility (4.7.1)

            Attacks = RookAttacks(Square, AllPieces);

            ScoreOpening += (POPCNT(Attacks & ~AllPieces) - RookMobilityMoveDecrease) * RookMobilityOpening;
            ScoreEnding += (POPCNT(Attacks & ~AllPieces) - RookMobilityMoveDecrease) * RookMobilityEnding;

            // Piece attack (4.8.3)

            if (Attacks & BlackKingZone) {
                BlackKingZoneAttackedCount += 1;
                BlackKingZoneAttackedValue += 2;
            }

            // Open file (4.7.2)

            File = FILE(Square);

            if (PawnFileMask[File] & WhitePawns) {
                if (PawnFileMask[File] & BlackPawns) { // Closed file
                    ScoreOpening += RookOnClosedFileOpening;
                    ScoreEnding += RookOnClosedFileEnding;
                }
            }
            else if (PawnFileMask[File] & BlackPawns) { // Semi open file
                if (File == FILE(BlackKingSquare)) { // Same to enemy king
                    ScoreOpening += RookOnSemiOpenFileSameToEnemyKingOpening;
                    ScoreEnding += RookOnSemiOpenFileSameToEnemyKingEnding;
                }
                else if ((File == FILE(BlackKingSquare) - 1) || (File == FILE(BlackKingSquare) + 1)) { // Adjacent to enemy king
                    ScoreOpening += RookOnSemiOpenFileAdjacentToEnemyKingOpening;
                    ScoreEnding += RookOnSemiOpenFileAdjacentToEnemyKingEnding;
                }
                else {
                    ScoreOpening += RookOnSemiOpenFileOpening;
                    ScoreEnding += RookOnSemiOpenFileEnding;
                }
            }
            else { // Open file
                if (File == FILE(BlackKingSquare)) { // Same to enemy king
                    ScoreOpening += RookOnOpenFileSameToEnemyKingOpening;
                    ScoreEnding += RookOnOpenFileSameToEnemyKingEnding;
                }
                else if ((File == FILE(BlackKingSquare) - 1) || (File == FILE(BlackKingSquare) + 1)) { // Adjacent to enemy king
                    ScoreOpening += RookOnOpenFileAdjacentToEnemyKingOpening;
                    ScoreEnding += RookOnOpenFileAdjacentToEnemyKingEnding;
                }
                else {
                    ScoreOpening += RookOnOpenFileOpening;
                    ScoreEnding += RookOnOpenFileEnding;
                }
            }

            // Seventh rank (4.7.4)

            if (
                (BB_SQUARE(Square) & BB_RANK_7)
                && (
                    (BlackPawns & BB_RANK_7)
                    || (Board->BB_Pieces[BLACK][KING] & BB_RANK_8)
                )
            ) {
                ScoreOpening += RookOnSeventhOpening;
                ScoreEnding += RookOnSeventhEnding;
            }

            // Pattern (4.6)

            if (
                ((BB_SQUARE(Square) & (BB_A1 | BB_A2 | BB_B1)) && (Board->BB_Pieces[WHITE][KING] & (BB_B1 | BB_C1)))
                || ((BB_SQUARE(Square) & (BB_H1 | BB_H2 | BB_G1)) && (Board->BB_Pieces[WHITE][KING] & (BB_F1 | BB_G1)))
            ) {
                ScoreOpening += BlockedRookOpening;
                ScoreEnding += BlockedRookEnding;
            }

            break;

        case QUEEN:
            // Material (4.2)

            ScoreOpening += PiecesScoreOpening[QUEEN];
            ScoreEnding += PiecesScoreEnding[QUEEN];

            // Piece-square tables (4.3)

            ScoreOpening += QueenSquareScoreOpening[Square];
            ScoreEnding += QueenSquareScoreEnding[Square];

            // Piece attack (4.8.3)

            Attacks = QueenAttacks(Square, AllPieces);

            if (Attacks & BlackKingZone) {
                BlackKingZoneAttackedCount += 1;
                BlackKingZoneAttackedValue += 4;
            }

            // Seventh rank (4.7.4)

            if (
                (BB_SQUARE(Square) & BB_RANK_7)
                && (
                    (BlackPawns & BB_RANK_7)
                    || (Board->BB_Pieces[BLACK][KING] & BB_RANK_8)
                )
            ) {
                ScoreOpening += QueenOnSeventhOpening;
                ScoreEnding += QueenOnSeventhEnding;
            }

            // King distance (4.7.5)

            ScoreOpening += 10 - ABS(RANK(Square) - RANK(BlackKingSquare)) - ABS(FILE(Square) - FILE(BlackKingSquare));
            ScoreEnding += 10 - ABS(RANK(Square) - RANK(BlackKingSquare)) - ABS(FILE(Square) - FILE(BlackKingSquare));

            break;

        case KING:
            // Material (4.2)

            ScoreOpening += PiecesScoreOpening[KING];
            ScoreEnding += PiecesScoreEnding[KING];

            // Piece-square tables (4.3)

            ScoreOpening += KingSquareScoreOpening[Square];
            ScoreEnding += KingSquareScoreEnding[Square];

            // King (4.8)

            // Pawn shelter (4.8.1)
            // Pawn storm (4.8.2)

            if (Board->BB_Pieces[BLACK][QUEEN] && (Board->BB_Pieces[BLACK][KNIGHT] || Board->BB_Pieces[BLACK][BISHOP] || Board->BB_Pieces[BLACK][ROOK])) {
                KingOpening = EvaluateWhiteKingOpening(Square, WhitePawns, BlackPawns);

                if (Board->CastleFlags & (CASTLE_WHITE_KING | CASTLE_WHITE_QUEEN)) { // White O-O/O-O-O
                    KingOpeningKingSide = EvaluateWhiteKingOpening(62, WhitePawns, BlackPawns); // g1
                    KingOpeningQueenSide = EvaluateWhiteKingOpening(58, WhitePawns, BlackPawns); // c1

                    ScoreOpening += (KingOpening + MAX(KingOpeningKingSide, KingOpeningQueenSide)) / 2;
                }
                else if (Board->CastleFlags & CASTLE_WHITE_KING) { // White O-O
                    KingOpeningKingSide = EvaluateWhiteKingOpening(62, WhitePawns, BlackPawns); // g1

                    ScoreOpening += (KingOpening + KingOpeningKingSide) / 2;
                }
                else if (Board->CastleFlags & CASTLE_WHITE_QUEEN) { // White O-O-O
                    KingOpeningQueenSide = EvaluateWhiteKingOpening(58, WhitePawns, BlackPawns); // c1

                    ScoreOpening += (KingOpening + KingOpeningQueenSide) / 2;
                }
                else {
                    ScoreOpening += KingOpening;
                }
            }

            break;
        } // switch

        Pieces &= Pieces - 1;
    } // while

    // Evaluate black pieces

    Pieces = BlackPieces;

    while (Pieces) {
        Square = LSB(Pieces);

        switch (PIECE(Board->Pieces[Square])) {
        case PAWN:
            // Material (4.2)

            ScoreOpening -= PiecesScoreOpening[PAWN];
            ScoreEnding -= PiecesScoreEnding[PAWN];

            // Piece-square tables (4.3)

            ScoreOpening -= PawnSquareScoreOpening[Square ^ 56];
            ScoreEnding -= PawnSquareScoreEnding[Square ^ 56];

            // Pawns (4.4)

            SquareAhead = Square + 8;

            File = FILE(Square);
            Rank = RANK(Square);

            if (PawnDoubledMask[BLACK][Square] & BlackPawns) { // Doubled
                ScoreOpening -= PawnDoubledOpening;
                ScoreEnding -= PawnDoubledEnding;
            }

            if (!(PawnIsolatedMask[File] & BlackPawns)) { // Isolated
                if (!(PawnDoubledMask[BLACK][Square] & (WhitePawns | BlackPawns))) { // Isolated open
                    ScoreOpening -= PawnIsolatedOpenOpening;
                    ScoreEnding -= PawnIsolatedOpenEnding;
                }
                else {
                    ScoreOpening -= PawnIsolatedOpening;
                    ScoreEnding -= PawnIsolatedEnding;
                }
            }
            else if (
                !(PawnPassedMask[WHITE][SquareAhead] & PawnIsolatedMask[File] & BlackPawns)
                && (
                    (BB_SQUARE(SquareAhead) & (WhitePawns | BlackPawns))
                    || (PawnAttacks(BB_SQUARE(SquareAhead), BLACK) & WhitePawns)
                )
            ) { // Backward
                if (!(PawnDoubledMask[BLACK][Square] & (WhitePawns | BlackPawns))) { // Backward open
                    ScoreOpening -= PawnBackwardOpenOpening;
                    ScoreEnding -= PawnBackwardOpenEnding;
                }
                else {
                    ScoreOpening -= PawnBackwardOpening;
                    ScoreEnding -= PawnBackwardEnding;
                }
            }

            // Passed pawns (4.9)
            // Pawns (4.4): Candidate

            if (!(PawnPassedMask[BLACK][Square] & WhitePawns)) { // Passed pawns
                PawnPassedScoreEnding = PawnPassedEndingBase2;

                // King distance

                PawnPassedScoreEnding += PawnPassedEndingHostileKingDistance * Distance[SquareAhead][WhiteKingSquare];
                PawnPassedScoreEnding += PawnPassedEndingFriendlyKingDistance * Distance[SquareAhead][BlackKingSquare];

                // Free

                if (!(BB_SQUARE(SquareAhead) & AllPieces) && !IsSquareAttacked(Board, SquareAhead, BLACK)) {
                    PawnPassedScoreEnding += PawnPassedEndingConsideredFree;
                }

                // Unstoppable

                if (!Board->BB_Pieces[WHITE][KNIGHT] && !Board->BB_Pieces[WHITE][BISHOP] && !Board->BB_Pieces[WHITE][ROOK] && !Board->BB_Pieces[WHITE][QUEEN]) {
                    if ((KingAttacks(Square) & Board->BB_Pieces[BLACK][KING]) && (KingAttacks(SQUARE(7, FILE(Square))) & Board->BB_Pieces[BLACK][KING])) {
                        PawnPassedScoreEnding += PawnPassedEndingUnstoppable;
                    }
                    else if (Board->CurrentColor == BLACK && !(PawnDoubledMask[BLACK][Square] & AllPieces)) {
                        for (int Square2 = SquareAhead; Square2 <= 63; Square2 += 8) {
                            if (Distance[Square][Square2] >= Distance[WhiteKingSquare][Square2]) {
                                break; // for
                            }

                            if (RANK(Square2) == 7) { // Last iteration
                                PawnPassedScoreEnding += PawnPassedEndingUnstoppable;
                            }
                        }
                    }
                }

                ScoreOpening -= PawnPassedOpening[Rank];
                ScoreEnding -= PawnPassedEndingBase1 + PawnPassedScoreEnding * PawnPassedEndingWeight[Rank] / 100;
            }
            else if (
                !(PawnDoubledMask[BLACK][Square] & (WhitePawns | BlackPawns))
                && (POPCNT(PawnPassedMask[BLACK][Square] & PawnIsolatedMask[File] & WhitePawns) <= POPCNT(PawnPassedMask[WHITE][SquareAhead] & PawnIsolatedMask[File] & BlackPawns))
                && (POPCNT(PawnAttacks(BB_SQUARE(Square), BLACK) & WhitePawns) <= POPCNT(PawnAttacks(BB_SQUARE(Square), WHITE) & BlackPawns))
            ) { // Candidate
                ScoreOpening -= PawnCandidateOpening[Rank];
                ScoreEnding -= PawnCandidateEnding[Rank];
            }

            break;

        case KNIGHT:
            // Material (4.2)

            ScoreOpening -= PiecesScoreOpening[KNIGHT];
            ScoreEnding -= PiecesScoreEnding[KNIGHT];

            // Piece-square tables (4.3)

            ScoreOpening -= KnightSquareScoreOpening[Square ^ 56];
            ScoreEnding -= KnightSquareScoreEnding[Square ^ 56];

            // Mobility (4.7.1)

            Attacks = KnightAttacks(Square);

            ScoreOpening -= (POPCNT(Attacks & ~AllPieces) - KnightMobilityMoveDecrease) * KnightMobility;
            ScoreEnding -= (POPCNT(Attacks & ~AllPieces) - KnightMobilityMoveDecrease) * KnightMobility;

            // Piece attack (4.8.3)

            if (Attacks & WhiteKingZone) {
                WhiteKingZoneAttackedCount += 1;
                WhiteKingZoneAttackedValue += 1;
            }

            // Outpost (4.7.3)

            if (KnightOutpost[Square ^ 56]) {
                Attacks2 = PawnAttacks(BB_SQUARE(Square), WHITE) & BlackPawns;

                if (Attacks2) {
                    ScoreOpening -= KnightOutpost[Square ^ 56] * POPCNT(Attacks2);
                    ScoreEnding -= KnightOutpost[Square ^ 56] * POPCNT(Attacks2);
                }
            }

            break;

        case BISHOP:
            // Material (4.2)

            ScoreOpening -= PiecesScoreOpening[BISHOP];
            ScoreEnding -= PiecesScoreEnding[BISHOP];

            // Piece-square tables (4.3)

            ScoreOpening -= BishopSquareScoreOpening[Square ^ 56];
            ScoreEnding -= BishopSquareScoreEnding[Square ^ 56];

            // Mobility (4.7.1)

            Attacks = BishopAttacks(Square, AllPieces);

            ScoreOpening -= (POPCNT(Attacks & ~AllPieces) - BishopMobilityMoveDecrease) * BishopMobility;
            ScoreEnding -= (POPCNT(Attacks & ~AllPieces) - BishopMobilityMoveDecrease) * BishopMobility;

            // Piece attack (4.8.3)

            if (Attacks & WhiteKingZone) {
                WhiteKingZoneAttackedCount += 1;
                WhiteKingZoneAttackedValue += 1;
            }

            // Pattern (4.6)

            if (
                (Square == 57 && (WhitePawns & BB_C2)) // Bb1
                || (Square == 62 && (WhitePawns & BB_F2)) // Bg1
                || (Square == 48 && (WhitePawns & BB_B3)) // Ba2
                || (Square == 55 && (WhitePawns & BB_G3)) // Bh2
                || (Square == 40 && (WhitePawns & BB_B4)) // Ba3
                || (Square == 47 && (WhitePawns & BB_G4)) // Bh3
            ) {
                ScoreOpening -= TrappedBishopOpening;
                ScoreEnding -= TrappedBishopEnding;
            }

            if (
                (Square == 2 && (BlackPawns & BB_D7) && (AllPieces & BB_D6)) // Bc8
                || (Square == 5 && (BlackPawns & BB_E7) && (AllPieces & BB_E6)) // Bf8
            ) {
                ScoreOpening -= BlockedBishopOpening;
                ScoreEnding -= BlockedBishopEnding;
            }

            break;

        case ROOK:
            // Material (4.2)

            ScoreOpening -= PiecesScoreOpening[ROOK];
            ScoreEnding -= PiecesScoreEnding[ROOK];

            // Piece-square tables (4.3)

            ScoreOpening -= RookSquareScoreOpening[Square ^ 56];
            ScoreEnding -= RookSquareScoreEnding[Square ^ 56];

            // Mobility (4.7.1)

            Attacks = RookAttacks(Square, AllPieces);

            ScoreOpening -= (POPCNT(Attacks & ~AllPieces) - RookMobilityMoveDecrease) * RookMobilityOpening;
            ScoreEnding -= (POPCNT(Attacks & ~AllPieces) - RookMobilityMoveDecrease) * RookMobilityEnding;

            // Piece attack (4.8.3)

            if (Attacks & WhiteKingZone) {
                WhiteKingZoneAttackedCount += 1;
                WhiteKingZoneAttackedValue += 2;
            }

            // Open file (4.7.2)

            File = FILE(Square);

            if (PawnFileMask[File] & BlackPawns) {
                if (PawnFileMask[File] & WhitePawns) { // Closed file
                    ScoreOpening -= RookOnClosedFileOpening;
                    ScoreEnding -= RookOnClosedFileEnding;
                }
            }
            else if (PawnFileMask[File] & WhitePawns) { // Semi open file
                if (File == FILE(WhiteKingSquare)) { // Same to enemy king
                    ScoreOpening -= RookOnSemiOpenFileSameToEnemyKingOpening;
                    ScoreEnding -= RookOnSemiOpenFileSameToEnemyKingEnding;
                }
                else if ((File == FILE(WhiteKingSquare) - 1) || (File == FILE(WhiteKingSquare) + 1)) { // Adjacent to enemy king
                    ScoreOpening -= RookOnSemiOpenFileAdjacentToEnemyKingOpening;
                    ScoreEnding -= RookOnSemiOpenFileAdjacentToEnemyKingEnding;
                }
                else {
                    ScoreOpening -= RookOnSemiOpenFileOpening;
                    ScoreEnding -= RookOnSemiOpenFileEnding;
                }
            }
            else { // Open file
                if (File == FILE(WhiteKingSquare)) { // Same to enemy king
                    ScoreOpening -= RookOnOpenFileSameToEnemyKingOpening;
                    ScoreEnding -= RookOnOpenFileSameToEnemyKingEnding;
                }
                else if ((File == FILE(WhiteKingSquare) - 1) || (File == FILE(WhiteKingSquare) + 1)) { // Adjacent to enemy king
                    ScoreOpening -= RookOnOpenFileAdjacentToEnemyKingOpening;
                    ScoreEnding -= RookOnOpenFileAdjacentToEnemyKingEnding;
                }
                else {
                    ScoreOpening -= RookOnOpenFileOpening;
                    ScoreEnding -= RookOnOpenFileEnding;
                }
            }

            // Seventh rank (4.7.4)

            if (
                (BB_SQUARE(Square) & BB_RANK_2)
                && (
                    (WhitePawns & BB_RANK_2)
                    || (Board->BB_Pieces[WHITE][KING] & BB_RANK_1)
                )
            ) {
                ScoreOpening -= RookOnSeventhOpening;
                ScoreEnding -= RookOnSeventhEnding;
            }

            // Pattern (4.6)

            if (
                ((BB_SQUARE(Square) & (BB_A8 | BB_A7 | BB_B8)) && (Board->BB_Pieces[BLACK][KING] & (BB_B8 | BB_C8)))
                || ((BB_SQUARE(Square) & (BB_H8 | BB_H7 | BB_G8)) && (Board->BB_Pieces[BLACK][KING] & (BB_F8 | BB_G8)))
            ) {
                ScoreOpening -= BlockedRookOpening;
                ScoreEnding -= BlockedRookEnding;
            }

            break;

        case QUEEN:
            // Material (4.2)

            ScoreOpening -= PiecesScoreOpening[QUEEN];
            ScoreEnding -= PiecesScoreEnding[QUEEN];

            // Piece-square tables (4.3)

            ScoreOpening -= QueenSquareScoreOpening[Square ^ 56];
            ScoreEnding -= QueenSquareScoreEnding[Square ^ 56];

            // Piece attack (4.8.3)

            Attacks = QueenAttacks(Square, AllPieces);

            if (Attacks & WhiteKingZone) {
                WhiteKingZoneAttackedCount += 1;
                WhiteKingZoneAttackedValue += 4;
            }

            // Seventh rank (4.7.4)

            if (
                (BB_SQUARE(Square) & BB_RANK_2)
                && (
                    (WhitePawns & BB_RANK_2)
                    || (Board->BB_Pieces[WHITE][KING] & BB_RANK_1)
                )
            ) {
                ScoreOpening -= QueenOnSeventhOpening;
                ScoreEnding -= QueenOnSeventhEnding;
            }

            // King distance (4.7.5)

            ScoreOpening -= 10 - ABS(RANK(Square) - RANK(WhiteKingSquare)) - ABS(FILE(Square) - FILE(WhiteKingSquare));
            ScoreEnding -= 10 - ABS(RANK(Square) - RANK(WhiteKingSquare)) - ABS(FILE(Square) - FILE(WhiteKingSquare));

            break;

        case KING:
            // Material (4.2)

            ScoreOpening -= PiecesScoreOpening[KING];
            ScoreEnding -= PiecesScoreEnding[KING];

            // Piece-square tables (4.3)

            ScoreOpening -= KingSquareScoreOpening[Square ^ 56];
            ScoreEnding -= KingSquareScoreEnding[Square ^ 56];

            // King (4.8)

            // Pawn shelter (4.8.1)
            // Pawn storm (4.8.2)

            if (Board->BB_Pieces[WHITE][QUEEN] && (Board->BB_Pieces[WHITE][KNIGHT] || Board->BB_Pieces[WHITE][BISHOP] || Board->BB_Pieces[WHITE][ROOK])) {
                KingOpening = EvaluateBlackKingOpening(Square, WhitePawns, BlackPawns);

                if (Board->CastleFlags & (CASTLE_BLACK_KING | CASTLE_BLACK_QUEEN)) { // Black O-O/O-O-O
                    KingOpeningKingSide = EvaluateBlackKingOpening(6, WhitePawns, BlackPawns); // g8
                    KingOpeningQueenSide = EvaluateBlackKingOpening(2, WhitePawns, BlackPawns); // c8

                    ScoreOpening -= (KingOpening + MAX(KingOpeningKingSide, KingOpeningQueenSide)) / 2;
                }
                else if (Board->CastleFlags & CASTLE_BLACK_KING) { // Black O-O
                    KingOpeningKingSide = EvaluateBlackKingOpening(6, WhitePawns, BlackPawns); // g8

                    ScoreOpening -= (KingOpening + KingOpeningKingSide) / 2;
                }
                else if (Board->CastleFlags & CASTLE_BLACK_QUEEN) { // Black O-O-O
                    KingOpeningQueenSide = EvaluateBlackKingOpening(2, WhitePawns, BlackPawns); // c8

                    ScoreOpening -= (KingOpening + KingOpeningQueenSide) / 2;
                }
                else {
                    ScoreOpening -= KingOpening;
                }
            }

            break;
        } // switch

        Pieces &= Pieces - 1;
    } // while

    // Piece attack (4.8.3)

    if (WhiteKingZoneAttackedCount) {
        if (Board->BB_Pieces[BLACK][QUEEN] && (Board->BB_Pieces[BLACK][KNIGHT] || Board->BB_Pieces[BLACK][BISHOP] || Board->BB_Pieces[BLACK][ROOK])) {
            ScoreOpening += KingZoneAttackedBase * WhiteKingZoneAttackedValue * KingZoneAttackedWeight[MIN(WhiteKingZoneAttackedCount, 7)] / 100;
        }
    }

    if (BlackKingZoneAttackedCount) {
        if (Board->BB_Pieces[WHITE][QUEEN] && (Board->BB_Pieces[WHITE][KNIGHT] || Board->BB_Pieces[WHITE][BISHOP] || Board->BB_Pieces[WHITE][ROOK])) {
            ScoreOpening -= KingZoneAttackedBase * BlackKingZoneAttackedValue * KingZoneAttackedWeight[MIN(BlackKingZoneAttackedCount, 7)] / 100;
        }
    }

    // Tempo (4.5)

    if (Board->CurrentColor == WHITE) {
        ScoreOpening += TempoOpening;
        ScoreEnding += TempoEnding;
    }
    else { // BLACK
        ScoreOpening -= TempoOpening;
        ScoreEnding -= TempoEnding;
    }

    // Game phase (4.1)

    Phase = MaxGamePhase;

    Phase -= POPCNT(Board->BB_Pieces[WHITE][KNIGHT] | Board->BB_Pieces[BLACK][KNIGHT]);
    Phase -= POPCNT(Board->BB_Pieces[WHITE][BISHOP] | Board->BB_Pieces[BLACK][BISHOP]);
    Phase -= 2 * POPCNT(Board->BB_Pieces[WHITE][ROOK] | Board->BB_Pieces[BLACK][ROOK]);
    Phase -= 4 * POPCNT(Board->BB_Pieces[WHITE][QUEEN] | Board->BB_Pieces[BLACK][QUEEN]);

    Phase = MIN(MAX(Phase, 0), MaxGamePhase);

    // Interpolate score

    Score = ((ScoreOpening * (MaxGamePhase - Phase)) + (ScoreEnding * Phase)) / MaxGamePhase;

    // Perspective

    if (Board->CurrentColor == BLACK) {
        Score = -Score;
    }

    return Score;
}

#elif defined(NNUE_EVALUATION_FUNCTION)

int Evaluate(BoardItem* Board)
{
#ifdef DEBUG_STATISTIC
    ++Board->EvaluateCount;
#endif // DEBUG_STATISTIC

    return NetworkEvaluate(Board);
}

#elif defined(NNUE_EVALUATION_FUNCTION_2)

int Evaluate(BoardItem* Board)
{
#ifdef DEBUG_STATISTIC
    ++Board->EvaluateCount;
#endif // DEBUG_STATISTIC

    return NetworkEvaluate(Board);
}

#endif // SIMPLIFIED_EVALUATION_FUNCTION/TOGA_EVALUATION_FUNCTION/NNUE_EVALUATION_FUNCTION/NNUE_EVALUATION_FUNCTION_2