// Evaluate.h

#pragma once

#ifndef EVALUATE_H
#define EVALUATE_H

#include "Board.h"
#include "Def.h"
#include "Types.h"

#ifdef SIMPLIFIED_EVALUATION_FUNCTION

extern int PiecesScore[6];

extern int PawnSquareScore[64];
extern int KnightSquareScore[64];
extern int BishopSquareScore[64];
extern int RookSquareScore[64];
extern int QueenSquareScore[64];

extern int KingSquareScoreOpening[64];
extern int KingSquareScoreEnding[64];

// ------------------------------------------------------------------

void InitEvaluation(void);

BOOL IsEndGame(const BoardItem* Board);

int Evaluate(BoardItem* Board);

#elif defined(TOGA_EVALUATION_FUNCTION)

// Game phase (4.1)

extern const int MaxGamePhase;

// Material (4.2)

extern SCORE PiecesScoreOpening[6];
extern SCORE PiecesScoreEnding[6];

extern SCORE BishopPairOpening;
extern SCORE BishopPairEnding;

// Piece-square tables (4.3)

extern SCORE PawnSquareScoreOpening[64];
extern SCORE PawnSquareScoreEnding[64];

extern SCORE KnightSquareScoreOpening[64];
extern SCORE KnightSquareScoreEnding[64];

extern SCORE BishopSquareScoreOpening[64];
extern SCORE BishopSquareScoreEnding[64];

extern SCORE RookSquareScoreOpening[64];
extern SCORE RookSquareScoreEnding[64];

extern SCORE QueenSquareScoreOpening[64];
extern SCORE QueenSquareScoreEnding[64];

extern SCORE KingSquareScoreOpening[64];
extern SCORE KingSquareScoreEnding[64];

// Pawns (4.4)

extern SCORE PawnDoubledOpening;
extern SCORE PawnDoubledEnding;

extern SCORE PawnIsolatedOpening;
extern SCORE PawnIsolatedEnding;

extern SCORE PawnIsolatedOpenOpening;
extern SCORE PawnIsolatedOpenEnding;

extern SCORE PawnBackwardOpening;
extern SCORE PawnBackwardEnding;

extern SCORE PawnBackwardOpenOpening;
extern SCORE PawnBackwardOpenEnding;

extern SCORE PawnCandidateOpening[8];
extern SCORE PawnCandidateEnding[8];

// Tempo (4.5)

extern SCORE TempoOpening;
extern SCORE TempoEnding;

// Pattern (4.6)

extern SCORE TrappedBishopOpening;
extern SCORE TrappedBishopEnding;

extern SCORE BlockedBishopOpening;
extern SCORE BlockedBishopEnding;

extern SCORE BlockedRookOpening;
extern SCORE BlockedRookEnding;

// Piece (4.7)

// Mobility (4.7.1)

extern SCORE KnightMobilityMoveDecrease;
extern SCORE BishopMobilityMoveDecrease;
extern SCORE RookMobilityMoveDecrease;

extern SCORE KnightMobility;
extern SCORE BishopMobility;

extern SCORE RookMobilityOpening;
extern SCORE RookMobilityEnding;

// Open file (4.7.2)

extern SCORE RookOnClosedFileOpening;
extern SCORE RookOnClosedFileEnding;

extern SCORE RookOnSemiOpenFileOpening;
extern SCORE RookOnSemiOpenFileEnding;

extern SCORE RookOnSemiOpenFileAdjacentToEnemyKingOpening;
extern SCORE RookOnSemiOpenFileAdjacentToEnemyKingEnding;

extern SCORE RookOnSemiOpenFileSameToEnemyKingOpening;
extern SCORE RookOnSemiOpenFileSameToEnemyKingEnding;

extern SCORE RookOnOpenFileOpening;
extern SCORE RookOnOpenFileEnding;

extern SCORE RookOnOpenFileAdjacentToEnemyKingOpening;
extern SCORE RookOnOpenFileAdjacentToEnemyKingEnding;

extern SCORE RookOnOpenFileSameToEnemyKingOpening;
extern SCORE RookOnOpenFileSameToEnemyKingEnding;

// Outpost (4.7.3)

extern SCORE KnightOutpost[64];

// Seventh rank (4.7.4)

extern SCORE RookOnSeventhOpening;
extern SCORE RookOnSeventhEnding;

extern SCORE QueenOnSeventhOpening;
extern SCORE QueenOnSeventhEnding;

// King distance (4.7.5)

// King (4.8)

// Pawn shelter (4.8.1)

extern SCORE KingFriendlyPawnAdvanceBase;
extern SCORE KingFriendlyPawnAdvanceDefault;

// Pawn storm (4.8.2)

extern SCORE KingPawnStorm[8];

// Piece attack (4.8.3)

extern SCORE KingZoneAttackedBase;
extern SCORE KingZoneAttackedWeight[8]; // %

// Passed pawns (4.9)

extern SCORE PawnPassedEndingBase1;
extern SCORE PawnPassedEndingBase2;

extern SCORE PawnPassedEndingHostileKingDistance;
extern SCORE PawnPassedEndingFriendlyKingDistance;

extern SCORE PawnPassedEndingConsideredFree;

extern SCORE PawnPassedEndingUnstoppable;

extern SCORE PawnPassedOpening[8];
extern SCORE PawnPassedEndingWeight[8]; // %

// ------------------------------------------------------------------

void InitEvaluation(void);

SCORE Evaluate(BoardItem* Board);

#elif defined(NNUE_EVALUATION_FUNCTION) || defined(NNUE_EVALUATION_FUNCTION_2)

int Evaluate(BoardItem* Board);

#endif // SIMPLIFIED_EVALUATION_FUNCTION || TOGA_EVALUATION_FUNCTION || NNUE_EVALUATION_FUNCTION || NNUE_EVALUATION_FUNCTION_2

#endif // !EVALUATE_H