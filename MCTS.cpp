// MCTS.cpp

#include "stdafx.h"

#include "MCTS.h"

#include "Board.h"
#include "Def.h"
#include "Evaluate.h"
#include "Game.h"
#include "Gen.h"
#include "Move.h"
#include "QuiescenceSearch.h"
#include "Search.h"
#include "Types.h"
#include "Utils.h"

#ifdef MCTS

//#define ROLLOUT_RANDOM
#define ROLLOUT_EVALUATE

#define MAX_ITERATIONS  100000 // ~200 Mbyte
//#define MAX_ITERATIONS  200000 // ~400 Mbyte
//#define MAX_ITERATIONS  500000 // ~1 Gbyte

#ifdef ROLLOUT_RANDOM

#define UCT_C           1.414 // sqrt(2.0)

#elif defined(ROLLOUT_EVALUATE)

#define UCT_C           100.0 // TODO

#endif // ROLLOUT_RANDOM || ROLLOUT_EVALUATE

#define SEARCH_DEPTH    2

typedef struct Node {
    struct Node* Parent;
    struct Node* Children[MAX_GEN_MOVES];

    int ChildCount;

    MoveItem Move;

    int Color;

    BOOL IsTerminal;
    BOOL IsFullyExpanded; // For terminal node always TRUE

    int GameResult; // Just for terminal node

    int NextMoveNumber;

    int N;
    I64 Q;
} NodeItem; // 2104 bytes (aligned 2104 bytes)

BOOL IsGameOver(BoardItem* Board, const int Ply, int* Result)
{
    if (Ply > 0) {
        if (IsInsufficientMaterial(Board)) {
            *Result = 0; // Draw

//            printf("GameOver: IsInsufficientMaterial\n");

            return TRUE;
        }

        if (Board->FiftyMove >= 100) {
            if (IsInCheck(Board, Board->CurrentColor)) {
                if (!HasLegalMoves(Board)) { // Checkmate
                    if (Board->CurrentColor == WHITE) {
#ifdef ROLLOUT_RANDOM
                        *Result = -1; // Black win
#elif defined(ROLLOUT_EVALUATE)
                        *Result = -INF + Ply; // Black win
#endif // ROLLOUT_RANDOM || ROLLOUT_EVALUATE
                    }
                    else { // BLACK
#ifdef ROLLOUT_RANDOM
                        *Result = 1; // White win
#elif defined(ROLLOUT_EVALUATE)
                        *Result = INF - Ply; // White win
#endif // ROLLOUT_RANDOM || ROLLOUT_EVALUATE
                    }

//                    printf("GameOver: FiftyMove -> Checkmate (%d)\n", *Result);

                    return TRUE;
                }
            }

            *Result = 0; // Draw

//            printf("GameOver: FiftyMove\n");

            return TRUE;
        }

        if (PositionRepeat1(Board)) {
            *Result = 0; // Draw

//            printf("GameOver: PositionRepeat1\n");

            return TRUE;
        }
    } // if

    if (!HasLegalMoves(Board)) { // No legal moves
        if (IsInCheck(Board, Board->CurrentColor)) { // Checkmate
            if (Board->CurrentColor == WHITE) {
#ifdef ROLLOUT_RANDOM
                *Result = -1; // Black win
#elif defined(ROLLOUT_EVALUATE)
                *Result = -INF + Ply; // Black win
#endif // ROLLOUT_RANDOM || ROLLOUT_EVALUATE
            }
            else { // BLACK
#ifdef ROLLOUT_RANDOM
                *Result = 1; // White win
#elif defined(ROLLOUT_EVALUATE)
                *Result = INF - Ply; // White win
#endif // ROLLOUT_RANDOM || ROLLOUT_EVALUATE
            }

//            printf("GameOver: Checkmate (%d)\n", *Result);
        }
        else { // Stalemate
            *Result = 0; // Draw

//            printf("GameOver: Stalemate\n");
        }

        return TRUE;
    }

    *Result = 0; // Draw (not used)

    return FALSE;
}

NodeItem* CreateNodeMCTS(NodeItem* Parent, const MoveItem Move, BoardItem* Board, const int Ply)
{
    int GameResult;

    NodeItem* Node = (NodeItem*)calloc(1, sizeof(NodeItem));

    Node->Parent = Parent;

    Node->ChildCount = 0;

    Node->Move = Move;

    Node->Color = Board->CurrentColor;

    Node->IsTerminal = IsGameOver(Board, Ply, &GameResult);
    Node->IsFullyExpanded = (Node->IsTerminal ? TRUE : FALSE);

    Node->GameResult = (Node->IsTerminal ? GameResult : 0);

    Node->NextMoveNumber = 0;

    Node->N = 0;
    Node->Q = 0LL;

    ++Board->Nodes;

    return Node;
}

void FreeNodeMCTS(NodeItem* Node)
{
    NodeItem* ChildNode;

    for (int Index = 0; Index < Node->ChildCount; ++Index) {
        ChildNode = Node->Children[Index];

        FreeNodeMCTS(ChildNode);
    }

    free(Node);
}

BOOL IsRootNode(NodeItem* Node)
{
    return Node->Parent == nullptr;
}

NodeItem* BestChild(NodeItem* Node, const double C)
{
    NodeItem* ChildNode;

    double UCT;
    double MaxUCT = -DBL_MAX;

    int MaxIndexCount = 0;
    int MaxIndexList[MAX_GEN_MOVES];

    U64 RandomValue;
    int SelectedMaxIndex;

//    if (C == 0.0) {
//        printf("\n");
//    }

    for (int Index = 0; Index < Node->ChildCount; ++Index) {
        ChildNode = Node->Children[Index];

        UCT = ((double)ChildNode->Q / (double)ChildNode->N) + C * sqrt(log((double)Node->N) / (double)ChildNode->N);

//        if (C == 0.0) {
//            printf("Index = %3d Move = %s%s Q = %8lld N = %8d Q/N = %13f Exploration = %13f UCT = %13f\n", Index, BoardName[MOVE_FROM(ChildNode->Move.Move)], BoardName[MOVE_TO(ChildNode->Move.Move)], ChildNode->Q, ChildNode->N, (double)ChildNode->Q / (double)ChildNode->N, C * sqrt(log((double)Node->N) / (double)ChildNode->N), UCT);
//            printf("Index = %3d Move = %s%s Q = %8lld N = %8d UCT = %13f\n", Index, BoardName[MOVE_FROM(ChildNode->Move.Move)], BoardName[MOVE_TO(ChildNode->Move.Move)], ChildNode->Q, ChildNode->N, UCT);
//        }

        if (UCT > MaxUCT) {
            MaxUCT = UCT;

            MaxIndexCount = 0;
            MaxIndexList[MaxIndexCount++] = Index;
        }
        else if (UCT == MaxUCT) {
            MaxIndexList[MaxIndexCount++] = Index;
        }
    }

    if (MaxIndexCount > 1) {
        RandomValue = Rand64();
        SelectedMaxIndex = (int)(RandomValue & 0x7FFFFFFF) % MaxIndexCount;
    }
    else {
        SelectedMaxIndex = 0;
    }

    return Node->Children[MaxIndexList[SelectedMaxIndex]];
}

NodeItem* Expand(NodeItem* Node, BoardItem* Board, int* Ply)
{
    int GenMoveCount;
    MoveItem MoveList[MAX_GEN_MOVES];

    NodeItem* ChildNode;

    GenMoveCount = 0;
    GenerateAllMoves(Board, MoveList, &GenMoveCount);

    for (int MoveNumber = Node->NextMoveNumber; MoveNumber < GenMoveCount; ++MoveNumber) {
        Node->NextMoveNumber = MoveNumber + 1;

//        printf("Expand (make move): Ply = %d Move = %s%s\n", *Ply, BoardName[MOVE_FROM(MoveList[MoveNumber].Move)], BoardName[MOVE_TO(MoveList[MoveNumber].Move)]);

        MakeMove(Board, MoveList[MoveNumber]);

        ++(*Ply);

        if (!IsInCheck(Board, CHANGE_COLOR(Board->CurrentColor))) { // Legal move
            ChildNode = CreateNodeMCTS(Node, MoveList[MoveNumber], Board, *Ply);

            Node->Children[Node->ChildCount++] = ChildNode;

//            printf("Expand (new child): Move = %s%s\n", BoardName[MOVE_FROM(ChildNode->Move.Move)], BoardName[MOVE_TO(ChildNode->Move.Move)]);

            return ChildNode;
        }

        // Illegal move

        --(*Ply);

        UnmakeMove(Board);

//        printf("Expand (unmake illegal move): Ply = %d Move = %s%s\n", *Ply, BoardName[MOVE_FROM(MoveList[MoveNumber].Move)], BoardName[MOVE_TO(MoveList[MoveNumber].Move)]);
    }

    Node->IsFullyExpanded = TRUE;

    return nullptr;
}

NodeItem* TreePolicy(NodeItem* Node, BoardItem* Board, int* Ply)
{
    NodeItem* ChildNode;

    while (!Node->IsTerminal) {
        if (!Node->IsFullyExpanded) {
            ChildNode = Expand(Node, Board, Ply);

            if (ChildNode != nullptr) {
                return ChildNode;
            }
        }

        Node = BestChild(Node, UCT_C);

//        printf("TreePolicy (make move): Ply = %d Move = %s%s\n", *Ply, BoardName[MOVE_FROM(Node->Move.Move)], BoardName[MOVE_TO(Node->Move.Move)]);

        MakeMove(Board, Node->Move);

        ++(*Ply);
    }

    return Node;
}

#ifdef ROLLOUT_RANDOM

int RolloutRandom(NodeItem* Node, BoardItem* Board, int* Ply)
{
    int GameResult;

    int LegalMoveCount;
    MoveItem LegalMoveList[MAX_GEN_MOVES];

    U64 RandomValue;
    int SelectedMoveNumber;

    int Ply2 = 0;

    while (TRUE) {
        if (IsGameOver(Board, *Ply, &GameResult)) {
            break;
        }

        if (*Ply >= MAX_PLY) {
//            printf("RolloutRandom: MAX_PLY\n");

            GameResult = 0; // Draw (default)

            break;
        }

        LegalMoveCount = GenerateAllLegalMoves(Board, LegalMoveList);

        RandomValue = Rand64();
        SelectedMoveNumber = (int)(RandomValue & 0x7FFFFFFF) % LegalMoveCount;

//        printf("RolloutRandom (make move): Ply = %d Ply2 = %d Move = %s%s\n", *Ply, Ply2, BoardName[MOVE_FROM(LegalMoveList[SelectedMoveNumber].Move)], BoardName[MOVE_TO(LegalMoveList[SelectedMoveNumber].Move)]);

        MakeMove(Board, LegalMoveList[SelectedMoveNumber]);

        ++(*Ply);

        ++Ply2;
    }

    while (Ply2--) {
        --(*Ply);

        UnmakeMove(Board);

//        printf("RolloutRandom (unmake move): Ply = %d Ply2 = %d\n", *Ply, Ply2);
    }

    return GameResult;
}

#elif defined(ROLLOUT_EVALUATE)

int RolloutEvaluate(NodeItem* Node, BoardItem* Board, int* Ply)
{
    int Score;

//    MoveItem TempBestMoves[MAX_PLY];

    if (Node->IsTerminal) {
//        printf("RolloutEvaluate: GameResult = %d\n", Node->GameResult);

        return Node->GameResult;
    }

//    TempBestMoves[0] = { 0, 0, 0 };

    Score = Evaluate(Board);
//    Score = QuiescenceSearch(Board, -INF, INF, 0, *Ply, TempBestMoves, TRUE, IsInCheck(Board, Board->CurrentColor));
//    Score = Search(Board, -INF, INF, SEARCH_DEPTH, *Ply, TempBestMoves, TRUE, IsInCheck(Board, Board->CurrentColor), FALSE, 0);

    if (Board->CurrentColor == BLACK) {
        Score = -Score;
    }

//    printf("RolloutEvaluate: Score = %d\n", Score);

    return Score;
}

#endif // ROLLOUT_RANDOM || ROLLOUT_EVALUATE

void Backpropagate(NodeItem* Node, BoardItem* Board, int* Ply, const int Result)
{
    while (!IsRootNode(Node)) {
        ++Node->N;

        Node->Q += (Node->Parent->Color == WHITE ? (I64)Result : -(I64)Result);

//        printf("Backpropagate: Result = %d Q = %lld N = %d\n", Result, Node->Q, Node->N);

        --(*Ply);

        UnmakeMove(Board);

//        printf("Backpropagate (unmake move): Ply = %d Move = %s%s\n", *Ply, BoardName[MOVE_FROM(Node->Move.Move)], BoardName[MOVE_TO(Node->Move.Move)]);

        Node = Node->Parent;
    }

    // Root node

    ++Node->N;
}

void MonteCarloTreeSearch(BoardItem* Board, MoveItem* BestMoves, int* BestScore)
{
    int GameResult;

    NodeItem* RootNode;
    NodeItem* Node;

    int Iterations = 0;

    int Ply = 0;

    int Result;

//    printf("NodeItem = %zd\n", sizeof(NodeItem));

    if (IsGameOver(Board, 0, &GameResult)) {
        BestMoves[0] = { 0, 0, 0 };
        BestMoves[1] = { 0, 0, 0 };

        if (GameResult == 0) { // Draw
            *BestScore = 0;
        }
        else { // Checkmate
            *BestScore = -INF;
        }

        return;
    }

    RootNode = CreateNodeMCTS(nullptr, { 0, 0, 0 }, Board, 0);

    while (TRUE) {
        if (Iterations >= MAX_ITERATIONS) {
            StopSearch = TRUE;

            break;
        }

        if (
            (Iterations & 1023) == 0
            && Clock() >= TimeStop - 300ULL
        ) {
            StopSearch = TRUE;

            break;
        }

        if (StopSearch) {
            break;
        }

//        printf("\n");

        Node = TreePolicy(RootNode, Board, &Ply);

#ifdef ROLLOUT_RANDOM
        Result = RolloutRandom(Node, Board, &Ply);
#elif defined(ROLLOUT_EVALUATE)
        Result = RolloutEvaluate(Node, Board, &Ply);
#endif // ROLLOUT_RANDOM || ROLLOUT_EVALUATE

        Backpropagate(Node, Board, &Ply, Result);

        ++Iterations;
    }

//    printf("\n");

//    printf("Iterations = %d\n", Iterations);

    Node = RootNode;

    for (int Ply2 = 0; Ply2 < MAX_PLY; ++Ply2) {
        Node = BestChild(Node, 0.0);

        BestMoves[Ply2] = Node->Move;

        if (Node->IsTerminal || !Node->IsFullyExpanded) {
            if (Ply2 < MAX_PLY - 1) {
                BestMoves[Ply2 + 1] = { 0, 0, 0 };
            }

            break;
        }
    }

    MakeMove(Board, BestMoves[0]);

    if (IsGameOver(Board, 1, &GameResult)) {
        if (GameResult == 0) { // Draw
            *BestScore = 0;
        }
        else { // Checkmate
            *BestScore = -INF + 1;
        }
    }
    else {
        *BestScore = 0; // Default
    }

    UnmakeMove(Board);

    FreeNodeMCTS(RootNode);
}

#endif // MCTS