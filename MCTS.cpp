// MCTS.cpp

#include "stdafx.h"

#include "MCTS.h"

#include "Board.h"
#include "Def.h"
#include "Evaluate.h"
#include "Game.h"
#include "Gen.h"
#include "Hash.h"
#include "Move.h"
#include "QuiescenceSearch.h"
#include "Search.h"
#include "Types.h"
#include "Utils.h"

#ifdef MCTS

//#define MAX_ITERATIONS  10000

typedef struct Node {
    struct Node* Parent;
    struct Node* Children[MAX_GEN_MOVES];

    int ChildCount;

    MoveItem Move;

    int Color;

    int LegalMoveCount;
    MoveItem LegalMoveList[MAX_GEN_MOVES];

    int MoveNumber;

    double Q;
    double N;
} NodeItem; // 5172 bytes (aligned 5176 bytes)

BOOL IsGameOver(BoardItem* Board, const int LegalMoveCount, const int Ply, int* Result)
{
    if (Ply > 0) {
        if (IsInsufficientMaterial(Board)) {
            *Result = 0; // Draw

//        printf("GameOver: IsInsufficientMaterial\n");

            return TRUE;
        }

        if (Board->FiftyMove >= 100) {
            if (IsInCheck(Board, Board->CurrentColor)) {
                if (LegalMoveCount == 0) {
                    if (Board->CurrentColor == WHITE) {
                        *Result = -1; // Black win
                    }
                    else { // BLACK
                        *Result = 1; // White win
                    }

//                printf("GameOver: FiftyMove -> Checkmate (%d)\n", *Result);

                    return TRUE;
                }
            }

            *Result = 0; // Draw

//        printf("GameOver: FiftyMove\n");

            return TRUE;
        }

        if (PositionRepeat1(Board)) {
            *Result = 0; // Draw

//        printf("GameOver: PositionRepeat1\n");

            return TRUE;
        }
    } // if

    if (LegalMoveCount == 0) {
        if (IsInCheck(Board, Board->CurrentColor)) {
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

    *Result = 0; // Draw (default)

    return FALSE;
}

NodeItem* CreateNodeMCTS(NodeItem* Parent, BoardItem* Board, const MoveItem Move, int* Ply)
{
    NodeItem* Node = (NodeItem*)calloc(1, sizeof(NodeItem));

    Node->Parent = Parent;

    Node->ChildCount = 0;

    Node->Move = Move;

    Node->Color = Board->CurrentColor;

    Node->LegalMoveCount = GenerateAllLegalMoves(Board, Node->LegalMoveList);

    Node->MoveNumber = 0;

    Node->Q = 0.0;
    Node->N = 0.0;

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

BOOL IsFullyExpandedNode(NodeItem* Node)
{
    return Node->MoveNumber >= Node->LegalMoveCount;
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

        UCT = (ChildNode->Q / ChildNode->N) + C * sqrt(2.0 * log(Node->N) / ChildNode->N);

//        if (C == 0.0) {
//            printf("Index = %d Move = %s%s UCT = %f (Q = %f N = %f)\n", Index, BoardName[MOVE_FROM(ChildNode->Move.Move)], BoardName[MOVE_TO(ChildNode->Move.Move)], UCT, ChildNode->Q, ChildNode->N);
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
    MoveItem Move = Node->LegalMoveList[Node->MoveNumber++];

//    printf("Expand: Move = %s%s\n", BoardName[MOVE_FROM(Move.Move)], BoardName[MOVE_TO(Move.Move)]);

    MakeMove(Board, Move);

    ++Board->Nodes;

    ++(*Ply);

    NodeItem* ChildNode = CreateNodeMCTS(Node, Board, Move, Ply);

    Node->Children[Node->ChildCount++] = ChildNode;

    return ChildNode;
}

NodeItem* TreePolicy(NodeItem* Node, BoardItem* Board, int* Ply)
{
    int GameResult;

    while (!IsGameOver(Board, Node->LegalMoveCount, *Ply, &GameResult)) {
        if (!IsFullyExpandedNode(Node)) {
            return Expand(Node, Board, Ply);
        }

        Node = BestChild(Node, 1.4);

//        printf("TreePolicy: Move = %s%s\n", BoardName[MOVE_FROM(Node->Move.Move)], BoardName[MOVE_TO(Node->Move.Move)]);

        MakeMove(Board, Node->Move);

        ++Board->Nodes;

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
        if (Ply2 == 0) {
            LegalMoveCount = Node->LegalMoveCount;

            memcpy(LegalMoveList, Node->LegalMoveList, sizeof(LegalMoveList));
        }
        else {
            LegalMoveCount = GenerateAllLegalMoves(Board, LegalMoveList);
        }

        if (IsGameOver(Board, LegalMoveCount, *Ply, &GameResult)) {
            break;
        }

        if (Board->HalfMoveNumber >= MAX_GAME_MOVES) { // TODO
            printf("!");

            break;
        }

        RandomValue = Rand64();
        SelectedMoveNumber = (int)(RandomValue & 0x7FFFFFFF) % LegalMoveCount;

//        printf("Rollout: Move = %s%s\n", BoardName[MOVE_FROM(LegalMoveList[SelectedMoveNumber].Move)], BoardName[MOVE_TO(LegalMoveList[SelectedMoveNumber].Move)]);

        MakeMove(Board, LegalMoveList[SelectedMoveNumber]);

        ++Board->Nodes;

        ++(*Ply);

        ++Ply2;
    }

    while (Ply2--) {
        UnmakeMove(Board);
    }

    return (double)GameResult;
}

double RolloutEvaluate(NodeItem* Node, BoardItem* Board, int* Ply)
{
    static const double Scale = 1.75 / 512;

    int Score = Evaluate(Board);

    if (Board->CurrentColor == BLACK) {
        Score = -Score;
    }

//    printf("Rollout: Score = %d\n", Score);

//    if (Score > INF - MAX_PLY) {
//        return tanh((double)Score * Scale) + (double)(*Ply) / 100.0;
//    }
//    else if (Score < -INF + MAX_PLY) {
//        return tanh((double)Score * Scale) - (double)(*Ply) / 100.0;
//    }
//    else {
        return tanh((double)Score * Scale);
//    }
}

void Backpropagate(NodeItem* Node, BoardItem* Board, const double SimulationResult)
{
    while (!IsRootNode(Node)) {
        Node->Q += (Node->Parent->Color == WHITE ? SimulationResult : -SimulationResult);

        Node->N += 1.0;

//        printf("Backpropagate: Move = %s%s (Q = %f N = %f)\n", BoardName[MOVE_FROM(Node->Move.Move)], BoardName[MOVE_TO(Node->Move.Move)], Node->Q, Node->N);

        UnmakeMove(Board);

        Node = Node->Parent;
    }

    // Root node

    Node->N += 1.0;
}

void MonteCarloTreeSearch(BoardItem* Board, MoveItem* BestMoves, int* BestScore)
{
    int Ply = 0;

    NodeItem* RootNode = CreateNodeMCTS(nullptr, Board, { 0, 0, 0 }, &Ply);
    NodeItem* Node;
    NodeItem* BestNode;

    int GameResult;

    double SimulationResult;

    int LegalMoveCount;
    MoveItem LegalMoveList[MAX_GEN_MOVES];

    int Iteration;

//    printf("NodeItem = %zd\n", sizeof(NodeItem));

    LegalMoveCount = GenerateAllLegalMoves(Board, LegalMoveList);

    if (IsGameOver(Board, LegalMoveCount, 0, &GameResult)) {
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

    Iteration = 0;

    while (TRUE) {
//        if (Iteration >= MAX_ITERATIONS) {
//            break;
//        }

//        if ((Iteration % 1000) == 0) {
//            printf(".");
//        }

        if (Clock() >= TimeStop) {
            break;
        }

        Ply = 0;

        Node = TreePolicy(RootNode, Board, &Ply);

//        SimulationResult = RolloutRandom(Node, Board, &Ply);
        SimulationResult = RolloutEvaluate(Node, Board, &Ply);

        Backpropagate(Node, Board, SimulationResult);

//        printf("\n");

        ++Iteration;
    }

//    printf("Iteration = %d\n", Iteration);

    BestNode = BestChild(RootNode, 0.0);

    BestMoves[0] = BestNode->Move;
    BestMoves[1] = { 0, 0, 0 };

    MakeMove(Board, BestNode->Move);

    LegalMoveCount = GenerateAllLegalMoves(Board, LegalMoveList);

    if (IsGameOver(Board, LegalMoveCount, 1, &GameResult)) {
        if (GameResult == 0) { // Draw
            *BestScore = 0;
        }
        else { // Checkmate
            *BestScore = -INF + 1;
        }
    }

    UnmakeMove(Board);

    FreeNodeMCTS(RootNode);
}

#endif // MCTS