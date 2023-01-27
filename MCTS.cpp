// MCTS.cpp

#include "stdafx.h"

#include "MCTS.h"

#include "Board.h"
#include "Def.h"
#include "Evaluate.h"
#include "Game.h"
#include "Gen.h"
#include "Move.h"
//#include "QuiescenceSearch.h"
#include "Search.h"
#include "Types.h"
#include "Utils.h"

#ifdef MCTS

#define MAX_ITERATIONS  100000 // ~200 Mbyte
//#define MAX_ITERATIONS  200000 // ~400 Mbyte
//#define MAX_ITERATIONS  500000 // ~1 Gbyte

#define SEARCH_DEPTH    2

typedef struct Node {
    struct Node* Parent;
    struct Node* Children[MAX_GEN_MOVES];

    int ChildCount;

    MoveItem Move;

    int Color;

    BOOL IsTerminal;
    BOOL IsFullyExpanded;

    int NextMoveNumber;

    double Q;
    double N;
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
                        *Result = -1; // Black win
                    }
                    else { // BLACK
                        *Result = 1; // White win
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
                *Result = -1; // Black win
            }
            else { // BLACK
                *Result = 1; // White win
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

    Node->NextMoveNumber = 0;

    Node->Q = 0.0;
    Node->N = 0.0;

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

NodeItem* BestChild(NodeItem* Node, const double C, const int Ply)
{
    NodeItem* ChildNode;

    double UCT;
    double MaxUCT = -DBL_MAX;

    int MaxIndexCount = 0;
    int MaxIndexList[MAX_GEN_MOVES];

    U64 RandomValue;
    int SelectedMaxIndex;

    if (C == 0.0) {
        printf("\n");
    }

    for (int Index = 0; Index < Node->ChildCount; ++Index) {
        ChildNode = Node->Children[Index];

        UCT = (ChildNode->Q / ChildNode->N) + C * sqrt(2.0 * log(Node->N) / ChildNode->N);
//        UCT = (ChildNode->Q / ChildNode->N) + C * pow(log(Node->N) / ChildNode->N, ((double)MAX_PLY + (double)Ply) / (2.0 * (double)MAX_PLY + (double)Ply));

        if (C == 0.0) {
//            printf("Index = %3d Move = %s%s Q = %13f N = %5.0f Q/N = %13f Exploration = %13f UCT = %9f\n", Index, BoardName[MOVE_FROM(ChildNode->Move.Move)], BoardName[MOVE_TO(ChildNode->Move.Move)], ChildNode->Q, ChildNode->N, ChildNode->Q / ChildNode->N, C * sqrt(2.0 * log(Node->N) / ChildNode->N), UCT);
            printf("Index = %3d Move = %s%s Q = %13f N = %5.0f UCT = %9f\n", Index, BoardName[MOVE_FROM(ChildNode->Move.Move)], BoardName[MOVE_TO(ChildNode->Move.Move)], ChildNode->Q, ChildNode->N, UCT);
        }

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

        Node = BestChild(Node, 1.0, *Ply);

//        printf("TreePolicy (make move): Ply = %d Move = %s%s\n", *Ply, BoardName[MOVE_FROM(Node->Move.Move)], BoardName[MOVE_TO(Node->Move.Move)]);

        MakeMove(Board, Node->Move);

        ++(*Ply);
    }

    return Node;
}

double RolloutRandom(NodeItem* Node, BoardItem* Board, int* Ply)
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

    return (double)GameResult;
}

double RolloutEvaluate(NodeItem* Node, BoardItem* Board, int* Ply)
{
    int GameResult;

    MoveItem TempBestMoves[MAX_PLY];

    int Score;

    static const double Scale = 1.75 / 512;

    if (Node->IsTerminal) {
        if (IsGameOver(Board, *Ply, &GameResult)) { // TODO
            return (double)GameResult;
        }
    }

    TempBestMoves[0] = { 0, 0, 0 };

//    Score = Evaluate(Board);
//    Score = QuiescenceSearch(Board, -INF, INF, 0, *Ply, TempBestMoves, TRUE, IsInCheck(Board, Board->CurrentColor));
    Score = Search(Board, -INF, INF, SEARCH_DEPTH, *Ply, TempBestMoves, TRUE, IsInCheck(Board, Board->CurrentColor), FALSE, 0);

    if (Board->CurrentColor == BLACK) {
        Score = -Score;
    }

//    printf("RolloutEvaluate: Score = %d tanh = %f\n", Score, tanh((double)Score * Scale));

    return tanh((double)Score * Scale);
}

void Backpropagate(NodeItem* Node, BoardItem* Board, int* Ply, const double Result)
{
    while (!IsRootNode(Node)) {
        Node->Q += (Node->Parent->Color == WHITE ? Result : -Result);
        Node->N += 1.0;

//        printf("Backpropagate: Result = %f Q = %f N = %f\n", Result, Node->Q, Node->N);

        --(*Ply);

        UnmakeMove(Board);

//        printf("Backpropagate (unmake move): Ply = %d Move = %s%s\n", *Ply, BoardName[MOVE_FROM(Node->Move.Move)], BoardName[MOVE_TO(Node->Move.Move)]);

        Node = Node->Parent;
    }

    // Root node

    Node->N += 1.0;
}

void MonteCarloTreeSearch(BoardItem* Board, MoveItem* BestMoves, int* BestScore)
{
    int GameResult;

    NodeItem* RootNode;
    NodeItem* Node;

    int Iterations = 0;

    int Ply = 0;

    double Result;

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

        Result = RolloutRandom(Node, Board, &Ply);
//        Result = RolloutEvaluate(Node, Board, &Ply);

        Backpropagate(Node, Board, &Ply, Result);

        ++Iterations;
    }

//    printf("\n");

//    printf("Iterations = %d\n", Iterations);

    Node = RootNode;

    for (Ply = 0; Ply < MAX_PLY; ++Ply) {
        Node = BestChild(Node, 0.0, Ply);

        BestMoves[Ply] = Node->Move;

        if (Node->IsTerminal || !Node->IsFullyExpanded) {
            if (Ply < MAX_PLY - 1) {
                BestMoves[Ply + 1] = { 0, 0, 0 };
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
//    else {
//        *BestScore = -Evaluate(Board);
//    }

    UnmakeMove(Board);

    FreeNodeMCTS(RootNode);
}

#endif // MCTS