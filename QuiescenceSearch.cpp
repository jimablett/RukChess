// QuiescenceSearch.cpp

#include "stdafx.h"

#include "QuiescenceSearch.h"

#include "Board.h"
#include "Def.h"
#include "Evaluate.h"
#include "Game.h"
#include "Gen.h"
#include "Hash.h"
#include "Move.h"
#include "SEE.h"
#include "Sort.h"
#include "Types.h"
#include "Utils.h"

#ifdef QUIESCENCE
int QuiescenceSearch(BoardItem* Board, int Alpha, int Beta, const int Depth, const int Ply, MoveItem* BestMoves, const BOOL IsPrincipal, const BOOL InCheck)
{
    int GenMoveCount;
    MoveItem MoveList[MAX_GEN_MOVES];

    int LegalMoveCount = 0;

    MoveItem BestMove = { 0, 0, 0 };

    MoveItem TempBestMoves[MAX_PLY];

#if defined(QUIESCENCE_HASH_SCORE) || defined(QUIESCENCE_HASH_MOVE)
    int HashScore = -INF;
    int HashStaticScore = -INF;
    int HashMove = 0;
    int HashDepth = -MAX_PLY;
    int HashFlag = 0;

    int QuiescenceHashDepth;

    int OldAlpha = Alpha;
#endif // QUIESCENCE_HASH_SCORE || QUIESCENCE_HASH_MOVE

    int Score;
    int BestScore;
    int StaticScore;

    BOOL GiveCheck;

#ifdef DEBUG_STATISTIC
    ++Board->QuiescenceCount;
#endif // DEBUG_STATISTIC

    if (
        omp_get_thread_num() == 0 // Master thread
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

#ifdef QUIESCENCE_MATE_DISTANCE_PRUNING
    Alpha = MAX(Alpha, -INF + Ply);
    Beta = MIN(Beta, INF - Ply - 1);

    if (Alpha >= Beta) {
        return Alpha;
    }
#endif // QUIESCENCE_MATE_DISTANCE_PRUNING

    if (IsInsufficientMaterial(Board)) {
        return 0;
    }

    if (Board->FiftyMove >= 100) {
        if (InCheck) {
            if (!HasLegalMoves(Board)) { // Checkmate
                return -INF + Ply;
            }
        }

        return 0;
    }

    if (PositionRepeat1(Board)) {
        return 0;
    }

    if (Ply >= MAX_PLY) {
        return (int)Evaluate(Board);
    }

    if (Board->HalfMoveNumber >= MAX_GAME_MOVES) {
        return (int)Evaluate(Board);
    }

#if defined(QUIESCENCE_HASH_SCORE) || defined(QUIESCENCE_HASH_MOVE)
    LoadHash(Board->Hash, &HashDepth, Ply, &HashScore, &HashStaticScore, &HashMove, &HashFlag);

    if (InCheck || Depth >= 0) {
        QuiescenceHashDepth = 0;
    }
    else { // !InCheck && Depth < 0
        QuiescenceHashDepth = -1;
    }

    if (HashFlag) {
#ifdef DEBUG_STATISTIC
        ++Board->HashCount;
#endif // DEBUG_STATISTIC

#ifdef QUIESCENCE_HASH_SCORE
        if (!IsPrincipal && HashDepth >= QuiescenceHashDepth) {
            if (
                (HashFlag == HASH_BETA && HashScore >= Beta)
                || (HashFlag == HASH_ALPHA && HashScore <= Alpha)
                || (HashFlag == HASH_EXACT)
            ) {
                return HashScore;
            }
        }
#endif // QUIESCENCE_HASH_SCORE
    }
#endif // QUIESCENCE_HASH_SCORE || QUIESCENCE_HASH_MOVE

#ifdef QUIESCENCE_CHECK_EXTENSION
    if (InCheck) {
        BestScore = StaticScore = -INF + Ply;

        GenMoveCount = 0;
        GenerateAllMoves(Board, MoveList, &GenMoveCount);
    }
    else {
#endif // QUIESCENCE_CHECK_EXTENSION
#ifdef QUIESCENCE_HASH_SCORE
        if (HashFlag) {
            BestScore = StaticScore = HashStaticScore;

            if (
                (HashFlag == HASH_BETA && HashScore > BestScore)
                || (HashFlag == HASH_ALPHA && HashScore < BestScore)
                || (HashFlag == HASH_EXACT)
            ) {
                BestScore = HashScore;
            }
        }
        else {
#endif // QUIESCENCE_HASH_SCORE
            BestScore = StaticScore = (int)Evaluate(Board);
#ifdef QUIESCENCE_HASH_SCORE
        }
#endif // QUIESCENCE_HASH_SCORE

        if (BestScore >= Beta) {
#ifdef QUIESCENCE_HASH_SCORE
            if (!HashFlag) {
                SaveHash(Board->Hash, -MAX_PLY, 0, 0, StaticScore, 0, HASH_STATIC_SCORE);
            }
#endif // QUIESCENCE_HASH_SCORE

            return BestScore;
        }

        if (IsPrincipal && BestScore > Alpha) {
            Alpha = BestScore;
        }

#ifdef QUIESCENCE_CHECK_EXTENSION_EXTENDED
        if (Depth == 0) {
            GenMoveCount = 0;
            GenerateAllMoves(Board, MoveList, &GenMoveCount);
        }
        else {
#endif // QUIESCENCE_CHECK_EXTENSION_EXTENDED
            GenMoveCount = 0;
            GenerateCaptureMoves(Board, MoveList, &GenMoveCount);
#ifdef QUIESCENCE_CHECK_EXTENSION_EXTENDED
        }
#endif // QUIESCENCE_CHECK_EXTENSION_EXTENDED
#ifdef QUIESCENCE_CHECK_EXTENSION
    }
#endif // QUIESCENCE_CHECK_EXTENSION

#ifdef QUIESCENCE_PVS
    SetPvsMoveSortValue(Board, Ply, MoveList, GenMoveCount);
#endif // QUIESCENCE_PVS

#ifdef QUIESCENCE_HASH_MOVE
    SetHashMoveSortValue(MoveList, GenMoveCount, HashMove);
#endif // QUIESCENCE_HASH_MOVE

    for (int MoveNumber = 0; MoveNumber < GenMoveCount; ++MoveNumber) {
#if defined(MOVES_SORT_SEE) || defined(MOVES_SORT_MVV_LVA) || defined(MOVES_SORT_HEURISTIC) || defined(MOVES_SORT_SQUARE_SCORE)
        PrepareNextMove(MoveNumber, MoveList, GenMoveCount);
#endif // MOVES_SORT_SEE || MOVES_SORT_MVV_LVA || MOVES_SORT_HEURISTIC || MOVES_SORT_SQUARE_SCORE

#ifdef QUIESCENCE_SEE_MOVE_PRUNING
        if (
            !InCheck
#ifdef QUIESCENCE_PVS
            && MoveList[MoveNumber].SortValue != SORT_PVS_MOVE_VALUE
#endif // QUIESCENCE_PVS
#ifdef QUIESCENCE_HASH_MOVE
            && MoveList[MoveNumber].Move != HashMove
#endif // QUIESCENCE_HASH_MOVE
        ) {
#ifdef MOVES_SORT_SEE
            if (
                (MoveList[MoveNumber].Type & MOVE_CAPTURE)
                && !(MoveList[MoveNumber].Type & MOVE_PAWN_PROMOTE)
            ) {
                if (MoveList[MoveNumber].SortValue < -SORT_CAPTURE_MOVE_BONUS) { // Bad capture move
                    break;
                }
            }
            else {
                if (CaptureSEE(Board, MOVE_FROM(MoveList[MoveNumber].Move), MOVE_TO(MoveList[MoveNumber].Move), MOVE_PROMOTE_PIECE(MoveList[MoveNumber].Move), MoveList[MoveNumber].Type) < 0) { // Bad quiet move
                    continue; // Next move
                }
            }
#elif defined(MOVES_SORT_MVV_LVA)
            if (CaptureSEE(Board, MOVE_FROM(MoveList[MoveNumber].Move), MOVE_TO(MoveList[MoveNumber].Move), MOVE_PROMOTE_PIECE(MoveList[MoveNumber].Move), MoveList[MoveNumber].Type) < 0) { // Bad capture/quiet move
                continue; // Next move
            }
#endif // MOVES_SORT_SEE || MOVES_SORT_MVV_LVA
        }
#endif // QUIESCENCE_SEE_MOVE_PRUNING

        MakeMove(Board, MoveList[MoveNumber]);

#if defined(HASH_PREFETCH) && (defined(QUIESCENCE_HASH_SCORE) || defined(QUIESCENCE_HASH_MOVE))
        Prefetch(Board->Hash);
#endif // HASH_PREFETCH && (QUIESCENCE_HASH_SCORE || QUIESCENCE_HASH_MOVE)

        if (IsInCheck(Board, CHANGE_COLOR(Board->CurrentColor))) { // Illegal move
            UnmakeMove(Board);

            continue; // Next move
        }

        ++LegalMoveCount;

        ++Board->Nodes;

        GiveCheck = IsInCheck(Board, Board->CurrentColor);

#ifdef QUIESCENCE_CHECK_EXTENSION_EXTENDED
        if (
            !InCheck
            && !GiveCheck
            && !(MoveList[MoveNumber].Type & (MOVE_CAPTURE | MOVE_PAWN_PROMOTE)) // Not capture/promote move
        ) {
            UnmakeMove(Board);

            continue; // Next move
        }
#endif // QUIESCENCE_CHECK_EXTENSION_EXTENDED

        TempBestMoves[0] = { 0, 0, 0 };

        Score = -QuiescenceSearch(Board, -Beta, -Alpha, Depth - 1, Ply + 1, TempBestMoves, IsPrincipal, GiveCheck);

        UnmakeMove(Board);

        if (StopSearch) {
            return 0;
        }

        if (Score > BestScore) {
            BestScore = Score;

            if (BestScore > Alpha) {
                BestMove = MoveList[MoveNumber];

                if (IsPrincipal) {
                    SaveBestMoves(BestMoves, BestMove, TempBestMoves);
                }

                if (IsPrincipal && BestScore < Beta) {
                    Alpha = BestScore;
                }
#ifdef ALPHA_BETA_PRUNING
                else { // !IsPrincipal || BestScore >= Beta
#ifdef DEBUG_STATISTIC
                    ++Board->CutoffCount;
#endif // DEBUG_STATISTIC

#if defined(QUIESCENCE_HASH_SCORE) || defined(QUIESCENCE_HASH_MOVE)
                    SaveHash(Board->Hash, QuiescenceHashDepth, Ply, BestScore, StaticScore, BestMove.Move, HASH_BETA);
#endif // QUIESCENCE_HASH_SCORE || QUIESCENCE_HASH_MOVE

                    return BestScore;
                }
#endif // ALPHA_BETA_PRUNING
            }
        }
    } // for

#ifdef QUIESCENCE_CHECK_EXTENSION
    if (InCheck && LegalMoveCount == 0) { // Checkmate
        return -INF + Ply;
    }
#endif // QUIESCENCE_CHECK_EXTENSION

#if defined(QUIESCENCE_HASH_SCORE) || defined(QUIESCENCE_HASH_MOVE)
    if (IsPrincipal && BestScore > OldAlpha) {
        HashFlag = HASH_EXACT;
    }
    else { // !IsPrincipal || BestScore <= OldAlpha
        HashFlag = HASH_ALPHA;
    }

    SaveHash(Board->Hash, QuiescenceHashDepth, Ply, BestScore, StaticScore, BestMove.Move, HashFlag);
#endif // QUIESCENCE_HASH_SCORE || QUIESCENCE_HASH_MOVE

    return BestScore;
}
#endif // QUIESCENCE