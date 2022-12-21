// PV_Splitting.cpp

#include "stdafx.h"

#include "PV_Splitting.h"

#include "Board.h"
#include "Def.h"
#include "Evaluate.h"
#include "Game.h"
#include "Gen.h"
#include "Hash.h"
#include "Heuristic.h"
#include "Move.h"
#include "QuiescenceSearch.h"
#include "Search.h"
#include "SEE.h"
#include "Sort.h"
#include "Types.h"
#include "Utils.h"

#ifdef PV_SPLITTING
int PVSplitting_Search(BoardItem* Board, int Alpha, int Beta, int Depth, const int Ply, MoveItem* BestMoves, const BOOL InCheck, const int SkipMove)
{
    int GenMoveCount;
    MoveItem MoveList[MAX_GEN_MOVES];

    int QuietMoveCount = 0;
    int QuietMoveList[MAX_GEN_MOVES]; // Move only

    volatile int LegalMoveCount = 0;

    MoveItem BestMove = { 0, 0, 0 };

    MoveItem TempBestMoves[MAX_PLY];

#if defined(HASH_SCORE) || defined(HASH_MOVE)
    int HashScore = -INF;
    int HashStaticScore = -INF;
    int HashMove = 0;
    int HashDepth = -MAX_PLY;
    int HashFlag = 0;
#endif // HASH_SCORE || HASH_MOVE

    int Score;
    int BestScore;
    int StaticScore;

    BOOL GiveCheck;

    int Extension;

    int NewDepth;

#if defined(MOVES_SORT_MVV_LVA) && defined(BAD_CAPTURE_LAST)
    int SEE_Value;
#endif // MOVES_SORT_MVV_LVA && BAD_CAPTURE_LAST

#if defined(NEGA_SCOUT) && defined(LATE_MOVE_REDUCTION)
    int LateMoveReduction;
#endif // NEGA_SCOUT && LATE_MOVE_REDUCTION

#if defined(SINGULAR_EXTENSION) && defined(HASH_SCORE) && defined(HASH_MOVE)
    int SingularBeta;
#endif // SINGULAR_EXTENSION && HASH_SCORE && HASH_MOVE

    int MoveNumber0;

    BOOL Cutoff = FALSE;

    BoardItem BoardCopy;
    BoardItem* ThreadBoard;

#ifdef QUIESCENCE
    if (Depth <= 0) {
        return QuiescenceSearch(Board, Alpha, Beta, 0, Ply, BestMoves, TRUE, InCheck);
    }
#endif // QUIESCENCE

    if (
        Ply > 0
        && (Board->Nodes & 65535) == 0
        && CompletedDepth >= MIN_SEARCH_DEPTH
        && Clock() >= TimeStop
    ) {
        StopSearch = TRUE;

        return 0;
    }

    if (StopSearch) {
        return 0;
    }

#ifdef MATE_DISTANCE_PRUNING
    Alpha = MAX(Alpha, -INF + Ply);
    Beta = MIN(Beta, INF - Ply - 1);

    if (Alpha >= Beta) {
        return Alpha;
    }
#endif // MATE_DISTANCE_PRUNING

    if (Ply > 0) {
        if (IsInsufficientMaterial(Board)) {
            return 0;
        }

        if (Board->FiftyMove >= 100) {
            if (InCheck) {
                GenMoveCount = 0;
                GenerateAllMoves(Board, MoveList, &GenMoveCount);

                for (int MoveNumber = 0; MoveNumber < GenMoveCount; ++MoveNumber) {
                    MakeMove(Board, MoveList[MoveNumber]);

                    if (IsInCheck(Board, CHANGE_COLOR(Board->CurrentColor))) {
                        UnmakeMove(Board);

                        continue; // Next move
                    }

                    // Legal move

                    UnmakeMove(Board);

                    return 0;
                }

                // No legal move

                return -INF + Ply;
            }

            return 0;
        }

        if (PositionRepeat1(Board)) {
            return 0;
        }
    }

    if (Ply >= MAX_PLY) {
        return (int)Evaluate(Board);
    }

#ifndef QUIESCENCE
    if (Depth <= 0) {
        return (int)Evaluate(Board);
    }
#endif // !QUIESCENCE

    if (Board->SelDepth < Ply + 1) {
        Board->SelDepth = Ply + 1;
    }

#if defined(HASH_SCORE) || defined(HASH_MOVE)
    if (!SkipMove) {
        LoadHash(Board->Hash, &HashDepth, Ply, &HashScore, &HashStaticScore, &HashMove, &HashFlag);

#ifdef DEBUG_STATISTIC
        if (HashFlag) {
            ++Board->HashCount;
        }
#endif // DEBUG_STATISTIC
    }
#endif // HASH_SCORE || HASH_MOVE

    if (InCheck) {
        StaticScore = -INF + Ply;
    }
    else {
#ifdef HASH_SCORE
        if (HashFlag) {
            StaticScore = HashStaticScore;
        }
        else {
#endif // HASH_SCORE
            StaticScore = (int)Evaluate(Board);

#ifdef HASH_SCORE
            if (!SkipMove) {
                SaveHash(Board->Hash, -MAX_PLY, 0, 0, StaticScore, 0, HASH_STATIC_SCORE);
            }
        }
#endif // HASH_SCORE
    }

#if defined(HASH_MOVE) && defined(IID)
    if (!SkipMove && !HashMove && Depth > 4) { // Hakkapeliitta
#ifdef DEBUG_IID
        printf("-- IID: Depth = %d\n", Depth);
#endif // DEBUG_IID

        // Search with full window for reduced depth
        TempBestMoves[0] = { 0, 0, 0 };

        Search(Board, Alpha, Beta, Depth - 2, Ply, TempBestMoves, TRUE, InCheck, FALSE, 0);

        if (StopSearch) {
            return 0;
        }

        LoadHash(Board->Hash, &HashDepth, Ply, &HashScore, &HashStaticScore, &HashMove, &HashFlag);

#ifdef DEBUG_IID
        if (HashMove) {
            printf("-- IID: Depth = %d Move = %s%s\n", Depth, BoardName[MOVE_FROM(HashMove)], BoardName[MOVE_TO(HashMove)]);
        }
#endif // DEBUG_IID
    }
#endif // HASH_MOVE && IID

#if defined(HASH_MOVE) && defined(IIR)
    if (!SkipMove && !HashMove && Depth > 4) {
        --Depth;
    }
#endif // HASH_MOVE && IIR

    BestScore = -INF + Ply;

    GenMoveCount = 0;
    GenerateAllMoves(Board, MoveList, &GenMoveCount);

#ifdef PVS
    SetPvsMoveSortValue(Board, Ply, MoveList, GenMoveCount);
#endif // PVS

#ifdef HASH_MOVE
    SetHashMoveSortValue(MoveList, GenMoveCount, HashMove);
#endif // HASH_MOVE

#ifdef KILLER_MOVE
    SetKillerMove1SortValue(Board, Ply, MoveList, GenMoveCount, HashMove);

#ifdef KILLER_MOVE_2
    SetKillerMove2SortValue(Board, Ply, MoveList, GenMoveCount, HashMove);
#endif // KILLER_MOVE_2

#endif // KILLER_MOVE

#ifdef COUNTER_MOVE
    SetCounterMoveSortValue(Board, Ply, MoveList, GenMoveCount, HashMove);
#endif // COUNTER_MOVE

#ifdef KILLER_MOVE

#ifdef COMMON_KILLER_MOVE_TABLE
    KillerMoveTable[Ply + 1][0] = 0;

#ifdef KILLER_MOVE_2
    KillerMoveTable[Ply + 1][1] = 0;
#endif // KILLER_MOVE_2

#else
    Board->KillerMoveTable[Ply + 1][0] = 0;

#ifdef KILLER_MOVE_2
    Board->KillerMoveTable[Ply + 1][1] = 0;
#endif // KILLER_MOVE_2

#endif // COMMON_KILLER_MOVE_TABLE

#endif // KILLER_MOVE

    // First move

    for (MoveNumber0 = 0; MoveNumber0 < GenMoveCount; ++MoveNumber0) {
#if defined(MOVES_SORT_MVV_LVA) && defined(BAD_CAPTURE_LAST)
NextMove0:
#endif // MOVES_SORT_MVV_LVA && BAD_CAPTURE_LAST

#if defined(MOVES_SORT_SEE) || defined(MOVES_SORT_MVV_LVA) || defined(MOVES_SORT_HEURISTIC) || defined(MOVES_SORT_SQUARE_SCORE)
        PrepareNextMove(MoveNumber0, MoveList, GenMoveCount);
#endif // MOVES_SORT_SEE || MOVES_SORT_MVV_LVA || MOVES_SORT_HEURISTIC || MOVES_SORT_SQUARE_SCORE

        if (MoveList[MoveNumber0].Move == SkipMove) {
            continue; // Next move
        }

#if defined(MOVES_SORT_MVV_LVA) && defined(BAD_CAPTURE_LAST)
        if (
            (MoveList[MoveNumber0].Type & MOVE_CAPTURE)
            && !(MoveList[MoveNumber0].Type & MOVE_PAWN_PROMOTE)
            && MoveList[MoveNumber0].SortValue >= 0
#ifdef PVS
            && MoveList[MoveNumber0].SortValue != SORT_PVS_MOVE_VALUE
#endif // PVS
#ifdef HASH_MOVE
            && MoveList[MoveNumber0].Move != HashMove
#endif // HASH_MOVE
        ) {
            SEE_Value = CaptureSEE(Board, MOVE_FROM(MoveList[MoveNumber0].Move), MOVE_TO(MoveList[MoveNumber0].Move), MOVE_PROMOTE_PIECE(MoveList[MoveNumber0].Move), MoveList[MoveNumber0].Type);

            if (SEE_Value < 0) { // Bad capture move
                MoveList[MoveNumber0].SortValue = SEE_Value - SORT_CAPTURE_MOVE_BONUS; // Search move later

                goto NextMove0;
            }
        }
#endif // MOVES_SORT_MVV_LVA && BAD_CAPTURE_LAST

        MakeMove(Board, MoveList[MoveNumber0]);

#if defined(HASH_PREFETCH) && (defined(HASH_SCORE) || defined(HASH_MOVE))
        Prefetch(Board->Hash);
#endif // HASH_PREFETCH && (HASH_SCORE || HASH_MOVE)

        if (IsInCheck(Board, CHANGE_COLOR(Board->CurrentColor))) {
            UnmakeMove(Board);

            continue; // Next move
        }

        ++LegalMoveCount;

        ++Board->Nodes;
/*
        if (Ply == 0 && PrintMode == PRINT_MODE_UCI && (Clock() - TimeStart) >= 3000ULL) {
            printf("info depth %d currmovenumber %d currmove %s%s", Depth, MoveNumber0 + 1, BoardName[MOVE_FROM(MoveList[MoveNumber0].Move)], BoardName[MOVE_TO(MoveList[MoveNumber0].Move)]);

            if (MoveList[MoveNumber0].Type & MOVE_PAWN_PROMOTE) {
                printf("%c", PiecesCharBlack[MOVE_PROMOTE_PIECE(MoveList[MoveNumber0].Move)]);
            }

            printf(" nodes %llu\n", Board->Nodes);
        }
*/
        GiveCheck = IsInCheck(Board, Board->CurrentColor);

        Extension = 0;

#ifdef CHECK_EXTENSION
        if (!Extension && GiveCheck) {
            Extension = 1;
        }
#endif // CHECK_EXTENSION

#if defined(SINGULAR_EXTENSION) && defined(HASH_SCORE) && defined(HASH_MOVE)
        if (
            !Extension
            && Ply > 0
            && Depth >= 8
            && MoveList[MoveNumber0].Move == HashMove
            && HashFlag == HASH_BETA
            && HashDepth >= Depth - 3
            && (HashScore > -INF + MAX_PLY && HashScore < INF - MAX_PLY)
        ) { // Xiphos
            UnmakeMove(Board);

            SingularBeta = HashScore - Depth;

            // Zero window search for reduced depth
            TempBestMoves[0] = { 0, 0, 0 };

            Score = Search(Board, SingularBeta - 1, SingularBeta, Depth / 2, Ply, TempBestMoves, FALSE, InCheck, FALSE, MoveList[MoveNumber0].Move);

#ifdef DEBUG_SINGULAR_EXTENSION
            PrintBoard(Board);

            printf("-- SE: Ply = %d Depth = %d SkipMove = %s%s HashScore = %d SingularBeta = %d Score = %d\n", Ply, Depth, BoardName[MOVE_FROM(MoveList[MoveNumber0].Move)], BoardName[MOVE_TO(MoveList[MoveNumber0].Move)], HashScore, SingularBeta, Score);
#endif // DEBUG_SINGULAR_EXTENSION

            if (Score < SingularBeta) {
#ifdef DEBUG_SINGULAR_EXTENSION
                printf("-- SE: Extension = 1\n");
#endif // DEBUG_SINGULAR_EXTENSION

                Extension = 1;
            }

            MakeMove(Board, MoveList[MoveNumber0]);
        }
#endif // SINGULAR_EXTENSION && HASH_SCORE && HASH_MOVE

        NewDepth = Depth - 1 + Extension;

        // Search with full window
        TempBestMoves[0] = { 0, 0, 0 };

        Score = -PVSplitting_Search(Board, -Beta, -Alpha, NewDepth, Ply + 1, TempBestMoves, GiveCheck, 0);

        UnmakeMove(Board);

        if (StopSearch) {
            return 0;
        }

        if (Score > BestScore) {
            BestScore = Score;

            if (BestScore > Alpha) {
                BestMove = MoveList[MoveNumber0];

                if (Ply == 0) { // Root node
                    if (BestMove.Move == BestMoves[0].Move) { // Move not changed
                        if (TimeStep > 0) {
                            --TimeStep;
                        }
                    }
                    else { // Move changed
                        TimeStep = MAX_TIME_STEPS - 1;
                    }
                }

                SaveBestMoves(BestMoves, BestMove, TempBestMoves);

                if (BestScore < Beta) {
                    Alpha = BestScore;
                }
#ifdef ALPHA_BETA_PRUNING
                else { // BestScore >= Beta
#ifdef DEBUG_STATISTIC
                    ++Board->CutoffCount;
#endif // DEBUG_STATISTIC

#if defined(MOVES_SORT_HEURISTIC) || defined(KILLER_MOVE) || defined(COUNTER_MOVE)
                    if (!(BestMove.Type & (MOVE_CAPTURE | MOVE_PAWN_PROMOTE))) { // Not capture/promote move
#ifdef MOVES_SORT_HEURISTIC
                        UpdateHeuristic(Board, BestMove.Move, BONUS(Depth));

                        for (int QuietMoveNumber = 0; QuietMoveNumber < QuietMoveCount; ++QuietMoveNumber) {
                            UpdateHeuristic(Board, QuietMoveList[QuietMoveNumber], -BONUS(Depth));
                        }
#endif // MOVES_SORT_HEURISTIC

#ifdef KILLER_MOVE
                        UpdateKillerMove(Board, BestMove.Move, Ply);
#endif // KILLER_MOVE

#ifdef COUNTER_MOVE
                        UpdateCounterMove(Board, BestMove.Move);
#endif // COUNTER_MOVE
                    }
#endif // MOVES_SORT_HEURISTIC || KILLER_MOVE || COUNTER_MOVE

                    goto PV_Done;
                }
#endif // ALPHA_BETA_PRUNING
            } // if
        } // if

        if (!(MoveList[MoveNumber0].Type & (MOVE_CAPTURE | MOVE_PAWN_PROMOTE))) { // Not capture/promote move
            QuietMoveList[QuietMoveCount++] = MoveList[MoveNumber0].Move;
        }

        ++MoveNumber0;

        break;
    } // for

    // Other moves (sorting)

#if defined(MOVES_SORT_SEE) || defined(MOVES_SORT_MVV_LVA) || defined(MOVES_SORT_HEURISTIC) || defined(MOVES_SORT_SQUARE_SCORE)
    for (int MoveNumber = MoveNumber0; MoveNumber < GenMoveCount; ++MoveNumber) {
#if defined(MOVES_SORT_MVV_LVA) && defined(BAD_CAPTURE_LAST)
NextMove:
#endif // MOVES_SORT_MVV_LVA && BAD_CAPTURE_LAST

        PrepareNextMove(MoveNumber, MoveList, GenMoveCount);

        if (MoveList[MoveNumber].Move == SkipMove) {
            continue; // Next move
        }

#if defined(MOVES_SORT_MVV_LVA) && defined(BAD_CAPTURE_LAST)
        if (
            (MoveList[MoveNumber].Type & MOVE_CAPTURE)
            && !(MoveList[MoveNumber].Type & MOVE_PAWN_PROMOTE)
            && MoveList[MoveNumber].SortValue >= 0
//#ifdef PVS
//            && MoveList[MoveNumber].SortValue != SORT_PVS_MOVE_VALUE
//#endif // PVS
//#ifdef HASH_MOVE
//            && MoveList[MoveNumber].Move != HashMove
//#endif // HASH_MOVE
        ) {
            SEE_Value = CaptureSEE(Board, MOVE_FROM(MoveList[MoveNumber].Move), MOVE_TO(MoveList[MoveNumber].Move), MOVE_PROMOTE_PIECE(MoveList[MoveNumber].Move), MoveList[MoveNumber].Type);

            if (SEE_Value < 0) { // Bad capture move
                MoveList[MoveNumber].SortValue = SEE_Value - SORT_CAPTURE_MOVE_BONUS; // Search move later

                goto NextMove;
            }
        }
#endif // MOVES_SORT_MVV_LVA && BAD_CAPTURE_LAST
    }
#endif // MOVES_SORT_SEE || MOVES_SORT_MVV_LVA || MOVES_SORT_HEURISTIC || MOVES_SORT_SQUARE_SCORE

    // Other moves (processing)

#pragma omp parallel for private(BoardCopy, ThreadBoard, GiveCheck, Extension, NewDepth, TempBestMoves, Score) schedule(dynamic, 1)
    for (int MoveNumber = MoveNumber0; MoveNumber < GenMoveCount; ++MoveNumber) {
        if (StopSearch || Cutoff || MoveList[MoveNumber].Move == SkipMove) {
            continue; // Next move
        }

        BoardCopy = *Board;
        ThreadBoard = &BoardCopy;

        ThreadBoard->Nodes = 0ULL;

#ifdef DEBUG_STATISTIC
        ThreadBoard->HashCount = 0ULL;
        ThreadBoard->EvaluateCount = 0ULL;
        ThreadBoard->CutoffCount = 0ULL;
        ThreadBoard->QuiescenceCount = 0ULL;
#endif // DEBUG_STATISTIC

        ThreadBoard->SelDepth = 0;

        MakeMove(ThreadBoard, MoveList[MoveNumber]);

#if defined(HASH_PREFETCH) && (defined(HASH_SCORE) || defined(HASH_MOVE))
        Prefetch(ThreadBoard->Hash);
#endif // HASH_PREFETCH && (HASH_SCORE || HASH_MOVE)

        if (IsInCheck(ThreadBoard, CHANGE_COLOR(ThreadBoard->CurrentColor))) {
            UnmakeMove(ThreadBoard);

            continue; // Next move
        }

#pragma omp critical
        {
            ++LegalMoveCount;
        }

        ++ThreadBoard->Nodes;
/*
        if (Ply == 0 && PrintMode == PRINT_MODE_UCI && (Clock() - TimeStart) >= 3000ULL) {
#pragma omp critical
            {
                printf("info depth %d currmovenumber %d currmove %s%s", Depth, MoveNumber + 1, BoardName[MOVE_FROM(MoveList[MoveNumber].Move)], BoardName[MOVE_TO(MoveList[MoveNumber].Move)]);

                if (MoveList[MoveNumber].Type & MOVE_PAWN_PROMOTE) {
                    printf("%c", PiecesCharBlack[MOVE_PROMOTE_PIECE(MoveList[MoveNumber].Move)]);
                }

                printf(" nodes %llu\n", Board->Nodes + ThreadBoard->Nodes);
            }
        }
*/
        GiveCheck = IsInCheck(ThreadBoard, ThreadBoard->CurrentColor);

        Extension = 0;

#ifdef CHECK_EXTENSION
        if (!Extension && GiveCheck) {
            Extension = 1;
        }
#endif // CHECK_EXTENSION
/*
#if defined(SINGULAR_EXTENSION) && defined(HASH_SCORE) && defined(HASH_MOVE)
        if (
            !Extension
            && Ply > 0
            && Depth >= 8
            && MoveList[MoveNumber].Move == HashMove
            && HashFlag == HASH_BETA
            && HashDepth >= Depth - 3
            && (HashScore > -INF + MAX_PLY && HashScore < INF - MAX_PLY)
        ) { // Xiphos
            UnmakeMove(ThreadBoard);

            SingularBeta = HashScore - Depth;

            // Zero window search for reduced depth
            TempBestMoves[0] = { 0, 0, 0 };

            Score = Search(ThreadBoard, SingularBeta - 1, SingularBeta, Depth / 2, Ply, TempBestMoves, FALSE, InCheck, FALSE, MoveList[MoveNumber].Move);

#ifdef DEBUG_SINGULAR_EXTENSION
            PrintBoard(ThreadBoard);

            printf("-- SE: Ply = %d Depth = %d SkipMove = %s%s HashScore = %d SingularBeta = %d Score = %d\n", Ply, Depth, BoardName[MOVE_FROM(MoveList[MoveNumber].Move)], BoardName[MOVE_TO(MoveList[MoveNumber].Move)], HashScore, SingularBeta, Score);
#endif // DEBUG_SINGULAR_EXTENSION

            if (Score < SingularBeta) {
#ifdef DEBUG_SINGULAR_EXTENSION
                printf("-- SE: Extension = 1\n");
#endif // DEBUG_SINGULAR_EXTENSION

                Extension = 1;
            }

            MakeMove(ThreadBoard, MoveList[MoveNumber]);
        }
#endif // SINGULAR_EXTENSION && HASH_SCORE && HASH_MOVE
*/
        NewDepth = Depth - 1 + Extension;

#ifdef LATE_MOVE_REDUCTION
        if (
            !Extension
            && !InCheck
            && !GiveCheck
            && !(MoveList[MoveNumber].Type & (MOVE_CAPTURE | MOVE_PAWN_PROMOTE)) // Not capture/promote move
//#ifdef PVS
//            && MoveList[MoveNumber].SortValue != SORT_PVS_MOVE_VALUE
//#endif // PVS
//#ifdef HASH_MOVE
//            && MoveList[MoveNumber].Move != HashMove
//#endif // HASH_MOVE
            && Depth >= 5
        ) {
            LateMoveReduction = LateMoveReductionTable[MIN(Depth, 63)][MIN(MoveNumber, 63)]; // Hakkapeliitta

            --LateMoveReduction;
        }
        else {
            LateMoveReduction = 0;
        }

        // Zero window search for reduced depth
        TempBestMoves[0] = { 0, 0, 0 };

        Score = -Search(ThreadBoard, -Alpha - 1, -Alpha, NewDepth - LateMoveReduction, Ply + 1, TempBestMoves, FALSE, GiveCheck, TRUE, 0);

        if (LateMoveReduction > 0 && Score > Alpha) {
            // Zero window search
            TempBestMoves[0] = { 0, 0, 0 };

            Score = -Search(ThreadBoard, -Alpha - 1, -Alpha, NewDepth, Ply + 1, TempBestMoves, FALSE, GiveCheck, TRUE, 0);
        }
#else
        // Zero window search
        TempBestMoves[0] = { 0, 0, 0 };

        Score = -Search(ThreadBoard, -Alpha - 1, -Alpha, NewDepth, Ply + 1, TempBestMoves, FALSE, GiveCheck, TRUE, 0);
#endif // LATE_MOVE_REDUCTION

        if (Score > Alpha && (Ply == 0 || Score < Beta)) {
            // Search with full window
            TempBestMoves[0] = { 0, 0, 0 };

            Score = -Search(ThreadBoard, -Beta, -Alpha, NewDepth, Ply + 1, TempBestMoves, TRUE, GiveCheck, TRUE, 0);
        }

        UnmakeMove(ThreadBoard);

#pragma omp critical
        {
            Board->Nodes += ThreadBoard->Nodes;

#ifdef DEBUG_STATISTIC
            Board->HashCount += ThreadBoard->HashCount;
            Board->EvaluateCount += ThreadBoard->EvaluateCount;
            Board->CutoffCount += ThreadBoard->CutoffCount;
            Board->QuiescenceCount += ThreadBoard->QuiescenceCount;
#endif // DEBUG_STATISTIC

            Board->SelDepth = MAX(Board->SelDepth, ThreadBoard->SelDepth);
        }

        if (StopSearch) {
            continue; // Next move
        }

#pragma omp critical
        if (Score > BestScore) {
            BestScore = Score;

            if (BestScore > Alpha) {
                BestMove = MoveList[MoveNumber];

                if (Ply == 0) { // Root node
                    if (BestMove.Move == BestMoves[0].Move) { // Move not changed
                        if (TimeStep > 0) {
                            --TimeStep;
                        }
                    }
                    else { // Move changed
                        TimeStep = MAX_TIME_STEPS - 1;
                    }
                }

                SaveBestMoves(BestMoves, BestMove, TempBestMoves);

                if (BestScore < Beta) {
                    Alpha = BestScore;
                }
#ifdef ALPHA_BETA_PRUNING
                else { // BestScore >= Beta
#ifdef DEBUG_STATISTIC
                    ++Board->CutoffCount;
#endif // DEBUG_STATISTIC

#if defined(MOVES_SORT_HEURISTIC) || defined(KILLER_MOVE) || defined(COUNTER_MOVE)
                    if (!(BestMove.Type & (MOVE_CAPTURE | MOVE_PAWN_PROMOTE))) { // Not capture/promote move
#ifdef MOVES_SORT_HEURISTIC
                        UpdateHeuristic(Board, BestMove.Move, BONUS(Depth));

                        for (int QuietMoveNumber = 0; QuietMoveNumber < QuietMoveCount; ++QuietMoveNumber) {
                            UpdateHeuristic(Board, QuietMoveList[QuietMoveNumber], -BONUS(Depth));
                        }
#endif // MOVES_SORT_HEURISTIC

#ifdef KILLER_MOVE
                        UpdateKillerMove(Board, BestMove.Move, Ply);
#endif // KILLER_MOVE

#ifdef COUNTER_MOVE
                        UpdateCounterMove(Board, BestMove.Move);
#endif // COUNTER_MOVE
                    }
#endif // MOVES_SORT_HEURISTIC || KILLER_MOVE || COUNTER_MOVE

                    Cutoff = TRUE;
                }
#endif // ALPHA_BETA_PRUNING
            } // if
        } // if

        if (!(MoveList[MoveNumber].Type & (MOVE_CAPTURE | MOVE_PAWN_PROMOTE))) { // Not capture/promote move
            QuietMoveList[QuietMoveCount++] = MoveList[MoveNumber].Move;
        }
    } // for

    if (StopSearch) {
        return 0;
    }

PV_Done:

    if (LegalMoveCount == 0) { // No legal move (checkmate or stalemate)
        if (SkipMove) {
            BestScore = Alpha;
        }
        else if (InCheck) { // Checkmate
            BestScore = -INF + Ply;
        }
        else { // Stalemate
            BestScore = 0;
        }
    }

#if defined(HASH_SCORE) || defined(HASH_MOVE)
    if (!SkipMove) {
        if (BestScore >= Beta) {
            HashFlag = HASH_BETA;
        }
        else if (BestMove.Move) {
            HashFlag = HASH_EXACT;
        }
        else {
            HashFlag = HASH_ALPHA;
        }

        SaveHash(Board->Hash, Depth, Ply, BestScore, StaticScore, BestMove.Move, HashFlag);
    }
#endif // HASH_SCORE || HASH_MOVE

    return BestScore;
}
#endif // PV_SPLITTING