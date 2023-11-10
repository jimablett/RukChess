// Book.h

#pragma once

#ifndef BOOK_H
#define BOOK_H

#include "Board.h"
#include "Def.h"
#include "Types.h"

extern BOOL BookFileLoaded;

void GenerateBook(void);

BOOL LoadBook(const char* BookFileName);

BOOL GetBookMove(const BoardItem* Board, MoveItem* BestMoves);

#endif // !BOOK_H