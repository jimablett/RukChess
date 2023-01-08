// MCTS.h

#pragma once

#ifndef MCTS_H
#define MCTS_H

#include "Board.h"
#include "Def.h"

void MonteCarloTreeSearch(BoardItem* Board, MoveItem* BestMoves, int* BestScore);

#endif // !MCTS_H