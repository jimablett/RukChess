// Book.h

#pragma once

#ifndef BOOK_H
#define BOOK_H

#include "Board.h"
#include "Def.h"
#include "Types.h"

extern BOOL UseBook;

void GenerateBook(void);

BOOL LoadBook(void);

BOOL GetBookMove(const BoardItem* Board, MoveItem* BestMoves);

#endif // !BOOK_H