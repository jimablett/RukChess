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

#define MAX_ITERATIONS  10000

#define SEARCH_DEPTH    5

typedef struct Node {
    struct Node* Parent;
    struct Node* Children[MAX_GEN_MOVES];

    int ChildCount;

    MoveItem Move;

    int Color;

    int LegalMoveCount;
    MoveItem LegalMoveList[MAX_GEN_MOVES];

    int MoveNumber;

    int Score;

    double Q;
    double N;
} NodeItem; // 5176 bytes (aligned 5176 bytes)

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

NodeItem* CreateNodeMCTS(NodeItem* Parent, BoardItem* Board, const MoveItem Move)
{
    NodeItem* Node = (NodeItem*)calloc(1, sizeof(NodeItem));

    Node->Parent = Parent;

    Node->ChildCount = 0;

    Node->Move = Move;

    Node->Color = Board->CurrentColor;

    Node->LegalMoveCount = GenerateAllLegalMoves(Board, Node->LegalMoveList);

    Node->MoveNumber = 0;

//    Node->Score = Evaluate(Board);
//    Node->Score = QuiescenceSearch(Board, -INF, INF, 0, 0, Board->BestMovesRoot, TRUE, IsInCheck(Board, Board->CurrentColor));
    Node->Score = Search(Board, -INF, INF, SEARCH_DEPTH, 0, Board->BestMovesRoot, TRUE, IsInCheck(Board, Board->CurrentColor), FALSE, 0);

    if (Board->CurrentColor == BLACK) {
        Node->Score = -Node->Score;
    }

//    if (Move.Move) {
//        printf("Move = %s%s Score = %d\n", BoardName[MOVE_FROM(Move.Move)], BoardName[MOVE_TO(Move.Move)], Node->Score);
//    }

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
/*
double Sigmoid(const int Score)
{
    static const double Scale = 3.5 / 512;

    return 1.0 / (1.0 + exp(-(double)Score * Scale));
}

double UCB_Score(NodeItem* Node, NodeItem* ChildNode, const double C)
{
    double P = Sigmoid(ChildNode->Score);

    double UCB = (ChildNode->Q / ChildNode->N) + C * P * sqrt(Node->N) / (1.0 + ChildNode->N);

    return UCB;
}
*/
NodeItem* BestChild(NodeItem* Node, const double C)
{
    NodeItem* SelectedNode = nullptr;
    NodeItem* ChildNode;

    double UCB;
    double MaxUCB = -DBL_MAX;

    for (int Index = 0; Index < Node->ChildCount; ++Index) {
        ChildNode = Node->Children[Index];

        UCB = (ChildNode->Q / ChildNode->N) + C * sqrt(log(Node->N) / ChildNode->N);
//        UCB = UCB_Score(Node, ChildNode, C);

//        if (C == 0.0) {
//            printf("Index = %d Move = %s%s Score = %d UCB = %f (Q = %f N = %f)\n", Index, BoardName[MOVE_FROM(ChildNode->Move.Move)], BoardName[MOVE_TO(ChildNode->Move.Move)], ChildNode->Score, UCB, ChildNode->Q, ChildNode->N);
//        }

        if (UCB > MaxUCB) {
            MaxUCB = UCB;

            SelectedNode = ChildNode;
        }
    }

    return SelectedNode;
}

NodeItem* Expand(NodeItem* Node, BoardItem* Board, int* Ply)
{
    MoveItem Move = Node->LegalMoveList[Node->MoveNumber++];

    MakeMove(Board, Move);

//    printf("Expand: Move = %s%s\n", BoardName[MOVE_FROM(Move.Move)], BoardName[MOVE_TO(Move.Move)]);

    ++Board->Nodes;

    ++(*Ply);

    NodeItem* ChildNode = CreateNodeMCTS(Node, Board, Move);

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

        MakeMove(Board, Node->Move);

//        printf("TreePolicy: Move = %s%s\n", BoardName[MOVE_FROM(Node->Move.Move)], BoardName[MOVE_TO(Node->Move.Move)]);

        ++Board->Nodes;

        ++(*Ply);
    }

    return Node;
}
/*
int BestMoveNumber(BoardItem* Board, const MoveItem* LegalMoveList, const int LegalMoveCount)
{
    int SelectedMoveNumber = -1;

    int Score;
    int MaxScore = -INT_MAX;

    for (int MoveNumber = 0; MoveNumber < LegalMoveCount; ++MoveNumber) {
        MakeMove(Board, LegalMoveList[MoveNumber]);

        Score = -Evaluate(Board);

//        printf("MoveNumber = %d Move = %s%s Score = %d\n", MoveNumber, BoardName[MOVE_FROM(LegalMoveList[MoveNumber].Move)], BoardName[MOVE_TO(LegalMoveList[MoveNumber].Move)], Score);

        if (Score > MaxScore) {
            MaxScore = Score;

            SelectedMoveNumber = MoveNumber;
        }

        UnmakeMove(Board);
    }

    return SelectedMoveNumber;
}
*//*
double Rollout(NodeItem* Node, BoardItem* Board, int* Ply)
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

//        SelectedMoveNumber = BestMoveNumber(Board, LegalMoveList, LegalMoveCount);

        MakeMove(Board, LegalMoveList[SelectedMoveNumber]);

//        printf("Rollout: Move = %s%s\n", BoardName[MOVE_FROM(LegalMoveList[SelectedMoveNumber].Move)], BoardName[MOVE_TO(LegalMoveList[SelectedMoveNumber].Move)]);

        ++Board->Nodes;

        ++(*Ply);

        ++Ply2;
    }

    while (Ply2--) {
        UnmakeMove(Board);
    }

    return (double)GameResult;
}
*/
double Rollout(NodeItem* Node, BoardItem* Board, int* Ply)
{
    static const double Scale = 1.75 / 512;

    if (Node->Score <= -INF + MAX_PLY || Node->Score >= INF - MAX_PLY) {
        return (double)Node->Score;
    }

    return tanh((double)Node->Score * Scale);
}

void Backpropagate(NodeItem* Node, BoardItem* Board, const double SimulationResult)
{
    while (!IsRootNode(Node)) {
        Node->Q += (Node->Parent->Color == WHITE ? SimulationResult : -SimulationResult);

        Node->N += 1.0;

//        printf("Backpropagate: Move = %s%s Q = %f N = %f\n", BoardName[MOVE_FROM(Node->Move.Move)], BoardName[MOVE_TO(Node->Move.Move)], Node->Q, Node->N);

        UnmakeMove(Board);

        Node = Node->Parent;
    }

//    printf("\n");

    Node->N += 1.0;
}

void MonteCarloTreeSearch(BoardItem* Board, MoveItem* BestMoves, int* BestScore)
{
    NodeItem* RootNode = CreateNodeMCTS(nullptr, Board, { 0, 0, 0 });
    NodeItem* Node;
    NodeItem* BestNode;

    int GameResult;

    double SimulationResult;

    int LegalMoveCount;
    MoveItem LegalMoveList[MAX_GEN_MOVES];

    int Iteration;

    int Ply;

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
//        printf(".");

        if (Iteration >= MAX_ITERATIONS) {
            break;
        }

        if (Clock() >= TimeStop - 150) {
            break;
        }

        Ply = 0;

        Node = TreePolicy(RootNode, Board, &Ply);

        SimulationResult = Rollout(Node, Board, &Ply);

        Backpropagate(Node, Board, SimulationResult);

        ++Iteration;
    }

    printf("\n");

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