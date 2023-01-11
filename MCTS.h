// MCTS.h

#pragma once

#ifndef MCTS_H
#define MCTS_H

#include "Board.h"
#include "Def.h"

#ifdef MCTS
void MonteCarloTreeSearch(BoardItem* Board, MoveItem* BestMoves, int* BestScore);
#endif // MCTS

#endif // !MCTS_H