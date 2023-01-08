// MCTS.cpp

#include "stdafx.h"

#include "MCTS.h"

#include "Board.h"
#include "Def.h"
#include "Gen.h"
#include "Move.h"
#include "Types.h"
#include "Utils.h"

#define MAX_ITERATION   50000

typedef struct Node {
    struct Node* Parent;
    struct Node* Children[MAX_GEN_MOVES];

    int ChildCount;

    MoveItem Move;

    int Color;

    int LegalMoveCount;
    MoveItem LegalMoveList[MAX_GEN_MOVES];

    int MoveNumber;

    int Q;
    int N;
} NodeItem; // 5164 bytes (aligned 5168 bytes)

BOOL IsGameOver(BoardItem* Board, const int LegalMoveCount, int* Result)
{
    if (LegalMoveCount == 0) { // No legal move (checkmate or stalemate)
        if (IsInCheck(Board, Board->CurrentColor)) { // Checkmate
            if (Board->CurrentColor == WHITE) {
                *Result = -1; // Black win
            }
            else { // BLACK
                *Result = 1; // White win
            }
        }
        else { // Stalemate
            *Result = 0; // Draw
        }

        return TRUE;
    }

    if (Board->FiftyMove >= 100) {
        *Result = 0; // Draw

        return TRUE;
    }

    if (IsInsufficientMaterial(Board)) {
        *Result = 0; // Draw

        return TRUE;
    }

    if (PositionRepeat1(Board)) {
        *Result = 0; // Draw

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

    Node->Q = 0;
    Node->N = 0;

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
    NodeItem* SelectedNode = nullptr;
    NodeItem* ChildNode;

    double UCT;
    double MaxUCT = -DBL_MAX;

    for (int Index = 0; Index < Node->ChildCount; ++Index) {
        ChildNode = Node->Children[Index];

        UCT = ((double)ChildNode->Q / (double)ChildNode->N) + C * sqrt(2.0 * log((double)Node->N) / (double)ChildNode->N);

        if (C == 0.0) {
            printf("Index = %d Move = %s%s UCT = %f (Q = %d N = %d)\n", Index, BoardName[MOVE_FROM(ChildNode->Move.Move)], BoardName[MOVE_TO(ChildNode->Move.Move)], UCT, ChildNode->Q, ChildNode->N);
        }

        if (UCT > MaxUCT) {
            MaxUCT = UCT;

            SelectedNode = ChildNode;
        }
    }

    return SelectedNode;
}

NodeItem* Expand(NodeItem* Node, BoardItem* Board)
{
    MoveItem Move = Node->LegalMoveList[Node->MoveNumber++];

    MakeMove(Board, Move);

    NodeItem* ChildNode = CreateNodeMCTS(Node, Board, Move);

    Node->Children[Node->ChildCount++] = ChildNode;

    return ChildNode;
}

NodeItem* TreePolicy(NodeItem* Node, BoardItem* Board)
{
    int GameResult;

    while (!IsGameOver(Board, Node->LegalMoveCount, &GameResult)) {
        if (!IsFullyExpandedNode(Node)) {
            return Expand(Node, Board);
        }

        Node = BestChild(Node, 1.4);

        MakeMove(Board, Node->Move);
    }

    return Node;
}

int Rollout(NodeItem* Node, BoardItem* Board)
{
    int GameResult;

    int LegalMoveCount;
    MoveItem LegalMoveList[MAX_GEN_MOVES];

    U64 RandomValue;
    int SelectedMoveNumber;

    int Ply = 0;

    while (TRUE) {
        if (Ply == 0) {
            LegalMoveCount = Node->LegalMoveCount;

            memcpy(LegalMoveList, Node->LegalMoveList, sizeof(LegalMoveList));
        }
        else {
            LegalMoveCount = GenerateAllLegalMoves(Board, LegalMoveList);
        }

        if (IsGameOver(Board, LegalMoveCount, &GameResult)) {
            break;
        }

        if (Board->HalfMoveNumber >= MAX_GAME_MOVES) { // TODO
            printf("!");

            break;
        }

        RandomValue = Rand64();
        SelectedMoveNumber = (int)(RandomValue & 0x7FFFFFFF) % LegalMoveCount;

        MakeMove(Board, LegalMoveList[SelectedMoveNumber]);

//        PrintBoard(Board);

//        printf("Ply = %d Move = %s%s\n", Ply, BoardName[MOVE_FROM(LegalMoveList[SelectedMoveNumber].Move)], BoardName[MOVE_TO(LegalMoveList[SelectedMoveNumber].Move)]);

        ++Ply;
    }

    while (Ply--) {
        UnmakeMove(Board);
    }

    return GameResult;
}

void Backpropagate(NodeItem* Node, BoardItem* Board, const int SimulationResult)
{
    while (!IsRootNode(Node)) {
        Node->Q += (Node->Parent->Color == WHITE ? SimulationResult : -SimulationResult);

        ++Node->N;

        UnmakeMove(Board);

        Node = Node->Parent;
    }

    ++Node->N;
}

void MonteCarloTreeSearch(BoardItem* Board, MoveItem* BestMoves, int* BestScore)
{
    NodeItem* RootNode = CreateNodeMCTS(nullptr, Board, { 0, 0, 0 });
    NodeItem* Node;
    NodeItem* BestNode;

    int GameResult;
    int SimulationResult;

    int LegalMoveCount;
    MoveItem LegalMoveList[MAX_GEN_MOVES];

//    printf("NodeItem = %zd\n", sizeof(NodeItem));

    LegalMoveCount = GenerateAllLegalMoves(Board, LegalMoveList);

    if (IsGameOver(Board, LegalMoveCount, &GameResult)) {
        BestMoves[0] = { 0, 0, 0 };
        BestMoves[1] = { 0, 0, 0 };

        if (GameResult == 0) { // Draw
            *BestScore = 0;
        }
        else { // Checkmate
            *BestScore = -INF + 1;
        }

        return;
    }

    for (int Iteration = 0; Iteration < MAX_ITERATION; ++Iteration) {
        if ((Iteration % 1000) == 0) {
            printf(".");
        }

        Node = TreePolicy(RootNode, Board);

        SimulationResult = Rollout(Node, Board);

        Backpropagate(Node, Board, SimulationResult);
    }

    printf("\n");

    BestNode = BestChild(RootNode, 0.0);

    BestMoves[0] = BestNode->Move;
    BestMoves[1] = { 0, 0, 0 };

    MakeMove(Board, BestNode->Move);

    LegalMoveCount = GenerateAllLegalMoves(Board, LegalMoveList);

    if (IsGameOver(Board, LegalMoveCount, &GameResult)) {
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