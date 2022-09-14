// Hash.cpp

#include "stdafx.h"

#include "Hash.h"

#include "BitBoard.h"
#include "Def.h"
#include "Types.h"
#include "Utils.h"

typedef struct {
	I16 Score;
	I16 StaticScore;

	U16 Move;

	I8 Depth;

	U8 Flag : 4, Iteration : 4;
} HashDataS; // 8 bytes

typedef union {
	U64 RawData;

	HashDataS Data;
} HashDataU; // 8 bytes

typedef struct {
	U64 KeyValue;

	HashDataU Value;
} HashItem; // 16 bytes

struct {
	U64 Size;
	U64 Mask;

	U8 Iteration; // 4 bits

	HashItem* Item;
} HashStore;

U64 PieceHash[2][6][64]; // [Color][Piece][Square]
U64 ColorHash;
U64 PassantHash[64]; // [Square]

void InitHashTable(int SizeInMb) // Xiphos
{
	U64 Items;
	U64 RoundItems = 1;

	Items = ((U64)SizeInMb << 20) / sizeof(HashItem);

	while (Items >>= 1) {
		RoundItems <<= 1;
	}

	HashStore.Size = RoundItems * sizeof(HashItem);
	HashStore.Mask = RoundItems - 1;

	HashStore.Item = (HashItem*)realloc(HashStore.Item, HashStore.Size);
}

void InitHashBoards(void)
{
//	printf("HashDataS = %zd HashDataU = %zd HashItem = %zd\n", sizeof(HashDataS), sizeof(HashDataU), sizeof(HashItem));

	SetRandState(0ULL);

	for (int Color = 0; Color < 2; ++Color) {
		for (int Piece = 0; Piece < 6; ++Piece) {
			for (int Square = 0; Square < 64; ++Square) {
				PieceHash[Color][Piece][Square] = Rand64();
			}
		}
	}

	ColorHash = Rand64();

	for (int Square = 0; Square < 64; ++Square) {
		PassantHash[Square] = Rand64();
	}
}

void InitHash(BoardItem* Board)
{
	U64 Pieces;

	int Square;

	U64 Hash = 0ULL;

	// White pieces
	Pieces = Board->BB_WhitePieces;

	while (Pieces) {
		Square = LSB(Pieces);

		Hash ^= PieceHash[WHITE][PIECE(Board->Pieces[Square])][Square];

		Pieces &= Pieces - 1;
	}

	// Black pieces
	Pieces = Board->BB_BlackPieces;

	while (Pieces) {
		Square = LSB(Pieces);

		Hash ^= PieceHash[BLACK][PIECE(Board->Pieces[Square])][Square];

		Pieces &= Pieces - 1;
	}

	// Color
	if (Board->CurrentColor == BLACK) {
		Hash ^= ColorHash;
	}

	// En passant
	if (Board->PassantSquare != -1) {
		Hash ^= PassantHash[Board->PassantSquare];
	}

	Board->Hash = Hash;
}

void ClearHash(void)
{
	memset(HashStore.Item, 0, HashStore.Size);

	HashStore.Iteration = 0;
}

void AddHashStoreIteration(void)
{
	HashStore.Iteration = (HashStore.Iteration + (U8)1) & (U8)15; // 4 bits
}

#if defined(HASH_SCORE) || defined(HASH_MOVE) || defined(QUIESCENCE_HASH_SCORE) || defined(QUIESCENCE_HASH_MOVE)
void SaveHash(const U64 Hash, const int Depth, const int Ply, const int Score, const int StaticScore, const int Move, const int Flag)
{
	HashItem* HashItemPointer = HashStore.Item + (Hash & HashStore.Mask);

	HashDataU DataU = HashItemPointer->Value; // Load data from record

	if (
		(HashItemPointer->KeyValue ^ Hash) == DataU.RawData
		|| Depth >= DataU.Data.Depth
		|| DataU.Data.Iteration != HashStore.Iteration
	) { // Xiphos
		// Replace record

		// Adjust the score
		if (Score > INF - MAX_PLY) {
			DataU.Data.Score = (I16)(Score + Ply);
		}
		else if (Score < -INF + MAX_PLY) {
			DataU.Data.Score = (I16)(Score - Ply);
		}
		else {
			DataU.Data.Score = (I16)Score;
		}

		DataU.Data.StaticScore = (I16)StaticScore;
		DataU.Data.Move = (U16)Move;
		DataU.Data.Depth = (I8)Depth;
		DataU.Data.Flag = (U8)Flag;
		DataU.Data.Iteration = HashStore.Iteration;

		// Save record
		HashItemPointer->KeyValue = (Hash ^ DataU.RawData);
		HashItemPointer->Value = DataU;
	}
}

void LoadHash(const U64 Hash, int* Depth, const int Ply, int* Score, int* StaticScore, int* Move, int* Flag)
{
	HashItem* HashItemPointer = HashStore.Item + (Hash & HashStore.Mask);

	HashDataU DataU = HashItemPointer->Value; // Load data from record

	if ((HashItemPointer->KeyValue ^ Hash) != DataU.RawData) { // Hash does not match or data is corrupted (SMP)
		return;
	}

	// Adjust the score
	if (DataU.Data.Score > INF - MAX_PLY) {
		*Score = DataU.Data.Score - Ply;
	}
	else if (DataU.Data.Score < -INF + MAX_PLY) {
		*Score = DataU.Data.Score + Ply;
	}
	else {
		*Score = DataU.Data.Score;
	}

	*StaticScore = DataU.Data.StaticScore;
	*Move = DataU.Data.Move;
	*Depth = DataU.Data.Depth;
	*Flag = DataU.Data.Flag;
}
#endif // HASH_SCORE || HASH_MOVE || QUIESCENCE_HASH_SCORE || QUIESCENCE_HASH_MOVE

int FullHash(void)
{
	int HashHit = 0;

	for (int Index = 0; Index < 1000; ++Index) {
		if (HashStore.Item[Index].Value.Data.Iteration == HashStore.Iteration) {
			++HashHit;
		}
	}

	return HashHit; // In per mille (0.1%)
}

#if defined(HASH_PREFETCH) && (defined(HASH_SCORE) || defined(HASH_MOVE) || defined(QUIESCENCE_HASH_SCORE) || defined(QUIESCENCE_HASH_MOVE))
void Prefetch(const U64 Hash)
{
	_mm_prefetch((char*)&HashStore.Item[Hash & HashStore.Mask], _MM_HINT_T0);
}
#endif // HASH_PREFETCH && (HASH_SCORE || HASH_MOVE || QUIESCENCE_HASH_SCORE || QUIESCENCE_HASH_MOVE)