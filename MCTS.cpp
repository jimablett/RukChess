// MCTS.cpp

#include "stdafx.h"

#include "MCTS.h"

#include "Board.h"
#include "Def.h"
#include "Gen.h"
#include "Move.h"
#include "Types.h"
#include "Utils.h"

#define MAX_ITERATION   10000

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

BOOL IsGameOver(BoardItem* Board, int* Result)
{
    int LegalMoveCount;
    MoveItem LegalMoveList[MAX_GEN_MOVES];

    if (IsInsufficientMaterial(Board)) {
//        printf("InsufficientMaterial\n");

        *Result = 0;

        return TRUE;
    }

    if (Board->FiftyMove >= 100) {
//        printf("FiftyMove\n");

        if (IsInCheck(Board, Board->CurrentColor)) {
            LegalMoveCount = GenerateAllLegalMoves(Board, LegalMoveList);

            if (LegalMoveCount == 0) { // No legal move (checkmate)
                if (Board->CurrentColor == WHITE) {
                    *Result = -1; // Black win
                }
                else { // BLACK
                    *Result = 1; // White win
                }

                return TRUE;
            }

            *Result = 0;

            return TRUE;
        }

        *Result = 0;

        return TRUE;
    }

    if (PositionRepeat1(Board)) {
//        printf("PositionRepeat\n");

        *Result = 0;

        return TRUE;
    }

    LegalMoveCount = GenerateAllLegalMoves(Board, LegalMoveList);

    if (LegalMoveCount == 0) { // No legal move (checkmate or stalemate)
        if (IsInCheck(Board, Board->CurrentColor)) { // Checkmate
//            printf("Checkmate\n");

            if (Board->CurrentColor == WHITE) {
                *Result = -1; // Black win
            }
            else { // BLACK
                *Result = 1; // White win
            }

            return TRUE;
        }

        // Stalemate

//        printf("Stalemate\n");

        *Result = 0;

        return TRUE;
    }

    *Result = 0;

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

NodeItem* BestChild(NodeItem* Node, const double C)
{
    NodeItem* SelectedNode = nullptr;
    NodeItem* ChildNode;

    double CurrentUCB;
    double MaxUCB = -1.0; // TODO: MIN_DBL

    for (int Index = 0; Index < Node->ChildCount; ++Index) {
        ChildNode = Node->Children[Index];

        if (ChildNode->N == 0) { // TODO
            printf("x");
        }

        if (Node->N == 0) { // TODO
            printf("o");
        }

        CurrentUCB = ((double)ChildNode->Q / (double)ChildNode->N) + C * sqrt(2.0 * log((double)Node->N) / (double)ChildNode->N); // TODO: 2.0?

        if (C == 0.0) { // TODO
            printf("Index = %d Move = %s%s CurrentUCB = %f (Q = %d N = %d)\n", Index, BoardName[MOVE_FROM(ChildNode->Move.Move)], BoardName[MOVE_TO(ChildNode->Move.Move)], CurrentUCB, ChildNode->Q, ChildNode->N);
        }

        if (CurrentUCB > MaxUCB) {
            MaxUCB = CurrentUCB;

            SelectedNode = ChildNode;
        }
    }

    return SelectedNode;
}

BOOL IsRootNode(NodeItem* Node)
{
    return Node->Parent == nullptr;
}

BOOL IsFullyExpandedNode(NodeItem* Node)
{
    return Node->MoveNumber >= Node->LegalMoveCount;
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
    int Result = 0;

    while (!IsGameOver(Board, &Result)) {
        if (!IsFullyExpandedNode(Node)) {
            return Expand(Node, Board);
        }

        Node = BestChild(Node, 1.4); // TODO: 1.4?

        MakeMove(Board, Node->Move);
    }

    return Node;
}

int Rollout(NodeItem* Node, BoardItem* Board)
{
    int Result = 0;

    int LegalMoveCount;
    MoveItem LegalMoveList[MAX_GEN_MOVES];

    U64 RandomValue;

    int Selected;

    int MoveNumber = 0;

    while (!IsGameOver(Board, &Result)) {
        if (Board->HalfMoveNumber >= MAX_GAME_MOVES) { // TODO
            printf("!");

            break;
        }

        LegalMoveCount = GenerateAllLegalMoves(Board, LegalMoveList);

        if (LegalMoveCount == 0) { // TODO
            printf("#");

            break;
        }

        RandomValue = Rand64();

        Selected = (int)(RandomValue & 0x7FFFFFFF) % LegalMoveCount;

        MakeMove(Board, LegalMoveList[Selected]);

//        PrintBoard(Board);

//        printf("MoveNumber = %d Move = %s%s\n", MoveNumber, BoardName[MOVE_FROM(LegalMoveList[Selected].Move)], BoardName[MOVE_TO(LegalMoveList[Selected].Move)]);

        ++MoveNumber;
    }

    while (MoveNumber--) {
        UnmakeMove(Board);
    }

    return Result;
}

void Backpropagate(NodeItem* Node, BoardItem* Board, const int Color, const int Result)
{
    while (!IsRootNode(Node)) {
//        if (Color == WHITE) {
//            Node->Q += (Node->Parent->Color == WHITE ? Result : -Result); // TODO
//        }
//        else { // BLACK
//            Node->Q += (Node->Parent->Color == BLACK ? -Result : Result); // TODO
//        }

        Node->Q += Result * (Node->Parent->Color == WHITE ? 1 : -1); // TODO

        Node->N++;

        UnmakeMove(Board);

        Node = Node->Parent;
    }

    Node->N++;
}

void MonteCarloTreeSearch(BoardItem* Board, MoveItem* BestMoves, int* BestScore)
{
    NodeItem* RootNode = CreateNodeMCTS(nullptr, Board, { 0, 0, 0 });
    NodeItem* Node;
    NodeItem* BestNode;

    int GameResult;

    int RolloutResult;

//    printf("NodeItem = %zd\n", sizeof(NodeItem));

    if (IsGameOver(Board, &GameResult)) {
        BestMoves[0] = { 0, 0, 0 };

        if (GameResult == 0) { // Draw
            *BestScore = 0;
        }
        else { // Checkmate
            *BestScore = -INF + 1;
        }

        return;
    }

    for (int Iteration = 0; Iteration < MAX_ITERATION; ++Iteration) {
        if ((Iteration % 500) == 0) { // TODO
            printf(".");
        }

        Node = TreePolicy(RootNode, Board);

        RolloutResult = Rollout(Node, Board);

        Backpropagate(Node, Board, RootNode->Color, RolloutResult);
    }

    printf("\n"); // TODO

    BestNode = BestChild(RootNode, 0.0);

    BestMoves[0] = BestNode->Move;
    BestMoves[1] = { 0, 0, 0 };

    MakeMove(Board, BestNode->Move);

    if (IsGameOver(Board, &GameResult)) {
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