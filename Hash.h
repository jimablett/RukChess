// Hash.h

#pragma once

#ifndef HASH_H
#define HASH_H

#include "Board.h"
#include "Def.h"
#include "Types.h"

#define HASH_EXACT			1
#define HASH_ALPHA			2
#define HASH_BETA			4
#define HASH_STATIC_SCORE	8

extern U64 PieceHash[2][6][64]; // [Color][Piece][Square]
extern U64 ColorHash;
extern U64 PassantHash[64]; // [Square]

void InitHashTable(int SizeInMb); // Xiphos

void InitHashBoards(void);

void InitHash(BoardItem* Board);

void ClearHash(void);

void AddHashStoreIteration(void);

#if defined(HASH_SCORE) || defined(HASH_MOVE) || defined(QUIESCENCE_HASH_SCORE) || defined(QUIESCENCE_HASH_MOVE)
void SaveHash(const U64 Hash, const int Depth, const int Ply, const int Score, const int StaticScore, const int Move, const int Flag);
void LoadHash(const U64 Hash, int* Depth, const int Ply, int* Score, int* StaticScore, int* Move, int* Flag);
#endif // HASH_SCORE || HASH_MOVE || QUIESCENCE_HASH_SCORE || QUIESCENCE_HASH_MOVE

int FullHash(void);

#if defined(HASH_PREFETCH) && (defined(HASH_SCORE) || defined(HASH_MOVE))
void Prefetch(const U64 Hash);
#endif // HASH_PREFETCH && (HASH_SCORE || HASH_MOVE)

#endif // !HASH_H