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

//#define MAX_ITERATIONS  100000 // ~200 Mbyte
#define MAX_ITERATIONS  200000 // ~400 Mbyte
//#define MAX_ITERATIONS  500000 // ~1 Gbyte

//#define SEARCH_DEPTH    2

typedef struct Node {
    struct Node* Parent;
    struct Node* Children[MAX_GEN_MOVES];

    int ChildCount;

    MoveItem Move;

    int Color;

    BOOL IsTerminal;
    BOOL IsFullyExpanded;

    int NextMoveNumber;

    int Q;
    int N;
} NodeItem; // 2096 bytes (aligned 2096 bytes)

BOOL HasLegalMoves(BoardItem* Board)
{
    int GenMoveCount;
    MoveItem MoveList[MAX_GEN_MOVES];

    GenMoveCount = 0;
    GenerateAllMoves(Board, MoveList, &GenMoveCount);

    for (int MoveNumber = 0; MoveNumber < GenMoveCount; ++MoveNumber) {
        MakeMove(Board, MoveList[MoveNumber]);

        if (IsInCheck(Board, CHANGE_COLOR(Board->CurrentColor))) { // Illegal move
            UnmakeMove(Board);

            continue; // Next move
        }

        // Legal move

        UnmakeMove(Board);

        return TRUE;
    }

    // No legal move

    return FALSE;
}

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
                    *Result = -INF + Ply;

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

    if (!HasLegalMoves(Board)) { // No legal move (checkmate or stalemate)
        if (IsInCheck(Board, Board->CurrentColor)) { // Checkmate
            *Result = -INF + Ply;

//            printf("GameOver: Checkmate (%d)\n", *Result);
        }
        else { // Stalemate
            *Result = 0; // Draw

//            printf("GameOver: Stalemate\n");
        }

        return TRUE;
    }

    *Result = 0; // Draw (default)

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

    Node->NextMoveNumber = 0;

    Node->Q = 0;
    Node->N = 0;

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

    for (int Index = 0; Index < Node->ChildCount; ++Index) {
        ChildNode = Node->Children[Index];

        UCT = ((double)ChildNode->Q / (double)ChildNode->N / (double)INF) + C * sqrt(2.0 * log((double)Node->N) / (double)ChildNode->N);

//        if (C == 0.0) {
//            printf("Index = %d Move = %s%s UCT = %f Q = %d N = %d\n", Index, BoardName[MOVE_FROM(ChildNode->Move.Move)], BoardName[MOVE_TO(ChildNode->Move.Move)], UCT, ChildNode->Q, ChildNode->N);
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

        if (!IsInCheck(Board, CHANGE_COLOR(Board->CurrentColor))) {
            ChildNode = CreateNodeMCTS(Node, MoveList[MoveNumber], Board, *Ply);

            Node->Children[Node->ChildCount++] = ChildNode;

//            printf("Expand (new child): Move = %s%s\n", BoardName[MOVE_FROM(ChildNode->Move.Move)], BoardName[MOVE_TO(ChildNode->Move.Move)]);

            return ChildNode;
        }

        --(*Ply);

        UnmakeMove(Board);

//        printf("Expand (unmake move): Ply = %d Move = %s%s\n", *Ply, BoardName[MOVE_FROM(MoveList[MoveNumber].Move)], BoardName[MOVE_TO(MoveList[MoveNumber].Move)]);
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

        Node = BestChild(Node, 1.0);

//        printf("TreePolicy (make move): Ply = %d Move = %s%s\n", *Ply, BoardName[MOVE_FROM(Node->Move.Move)], BoardName[MOVE_TO(Node->Move.Move)]);

        MakeMove(Board, Node->Move);

        ++(*Ply);
    }

    return Node;
}

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

int RolloutEvaluate(NodeItem* Node, BoardItem* Board, int* Ply)
{
    int Score;

//    MoveItem TempBestMoves[MAX_PLY];

//    TempBestMoves[0] = { 0, 0, 0 };

    Score = Evaluate(Board);
//    Score = QuiescenceSearch(Board, -INF, INF, 0, *Ply, TempBestMoves, TRUE, IsInCheck(Board, Board->CurrentColor));
//    Score = Search(Board, -INF, INF, SEARCH_DEPTH, *Ply, TempBestMoves, TRUE, IsInCheck(Board, Board->CurrentColor), FALSE, 0);

//    printf("RolloutEvaluate: Score = %d\n", Score);

    return Score;
}

void Backpropagate(NodeItem* Node, BoardItem* Board, int* Ply, int Result)
{
    while (!IsRootNode(Node)) {
        Result = -Result;

        Node->Q += Result;
        Node->N += 1;

//        printf("Backpropagate: Result = %d Q = %d N = %d\n", (Node->Parent->Color == WHITE ? Result : -Result), Node->Q, Node->N);

        --(*Ply);

        UnmakeMove(Board);

//        printf("Backpropagate (unmake move): Ply = %d Move = %s%s\n", *Ply, BoardName[MOVE_FROM(Node->Move.Move)], BoardName[MOVE_TO(Node->Move.Move)]);

        Node = Node->Parent;
    }

    // Root node

    Node->N += 1;
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

        *BestScore = GameResult;

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

//        Result = RolloutRandom(Node, Board, &Ply);
        Result = RolloutEvaluate(Node, Board, &Ply);

        Backpropagate(Node, Board, &Ply, Result);

        ++Iterations;
    }

//    printf("\n");

//    printf("Nodes = %d Iterations = %d\n", Board->Nodes, Iterations);

    Node = RootNode;

    for (int Ply2 = 0; Ply2 < MAX_PLY - 1; ++Ply2) {
        Node = BestChild(Node, 0.0);

        BestMoves[Ply2] = Node->Move;

        if (Node->IsTerminal || !Node->IsFullyExpanded) {
            BestMoves[Ply2 + 1] = { 0, 0, 0 };

            break;
        }
    }

    MakeMove(Board, BestMoves[0]);

    if (IsGameOver(Board, 1, &GameResult)) {
        *BestScore = GameResult;
    }
    else {
        *BestScore = -Evaluate(Board);
    }

    UnmakeMove(Board);

    FreeNodeMCTS(RootNode);
}

#endif // MCTS