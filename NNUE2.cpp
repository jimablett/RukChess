// NNUE2.cpp

#include "stdafx.h"

#include "NNUE2.h"

#include "BitBoard.h"
#include "Board.h"
#include "Def.h"
#include "Types.h"
#include "Utils.h"

#ifdef NNUE_EVALUATION_FUNCTION_2

#ifdef NET

#define NNUE_FILE                   "rukchess.nnue"
#define NNUE_FILE_MAGIC             ('B' | 'R' << 8 | 'K' << 16 | 'R' << 24)
//#define NNUE_FILE_HASH            0x000071EB63511CB1
#define NNUE_FILE_SIZE              1579024

#define FEATURE_DIMENSION           768
#define HIDDEN_DIMENSION            512
#define OUTPUT_DIMENSION            1

#elif defined(NET_KS)

#define NNUE_FILE                   "berserk_original_ks_768_512_1.nn"
#define NNUE_FILE_MAGIC             ('B' | 'R' << 8 | 'K' << 16 | 'R' << 24)
//#define NNUE_FILE_HASH            0x0000751158752607
#define NNUE_FILE_SIZE              1579024

#define FEATURE_DIMENSION           768
#define HIDDEN_DIMENSION            512
#define OUTPUT_DIMENSION            1

#else // NET_KQ

#define NNUE_FILE                   "berserk_original_kq_1536_512_1.nn"
#define NNUE_FILE_MAGIC             ('B' | 'R' << 8 | 'K' << 16 | 'R' << 24)
//#define NNUE_FILE_HASH            0x0000E68765EA32A3
#define NNUE_FILE_SIZE              3151888

#define FEATURE_DIMENSION           1536
#define HIDDEN_DIMENSION            512
#define OUTPUT_DIMENSION            1

#endif // NET/NET_KS/NET_KQ

#define QUANTIZATION_PRECISION_IN   32
#define QUANTIZATION_PRECISION_OUT  512

#ifdef USE_NNUE_AVX2

#define NUM_REGS                    (HIDDEN_DIMENSION * 16 / 256) // Hidden dimension x 16 bits (I16) / 256 bits (AVX2 register size)

#endif // USE_NNUE_AVX2

_declspec(align(32)) I16 FeatureWeights[FEATURE_DIMENSION * HIDDEN_DIMENSION];  // 768 x 512 = 393216
_declspec(align(32)) I16 HiddenBiases[HIDDEN_DIMENSION];                        // 512
_declspec(align(32)) I16 HiddenWeights[HIDDEN_DIMENSION * 2];                   // 512 x 2 = 1024
I16 OutputBias;                                                                 // 1

#if defined(NET_KS) || defined(NET_KQ)
const int HalfBoardIndex[64] = {
    28, 29, 30, 31, 31, 30, 29, 28,
    24, 25, 26, 27, 27, 26, 25, 24,
    20, 21, 22, 23, 23, 22, 21, 20,
    16, 17, 18, 19, 19, 18, 17, 16,
    12, 13, 14, 15, 15, 14, 13, 12,
     8,  9, 10, 11, 11, 10,  9,  8,
     4,  5,  6,  7,  7,  6,  5,  4,
     0,  1,  2,  3,  3,  2,  1,  0
};
#endif // NET_KS/NET_KQ

I16 LoadWeight(const float Value, const int Precision)
{
    I16 Result;

//  if (Value < (float)SHRT_MIN / (float)Precision) {
//      Result = SHRT_MIN;
//  }
//  else if (Value > (float)SHRT_MAX / (float)Precision) {
//      Result = SHRT_MAX;
//  }
//  else {
    Result = (I16)roundf(Value * (float)Precision);
//  }

    return Result;
}

void ReadNetwork(void)
{
    FILE* File;

    int FileMagic;
    U64 FileHash;

    float Value;

    fpos_t FilePos;

#ifdef PRINT_MIN_MAX_VALUES
    float MinValue;
    float MaxValue;
#endif // PRINT_MIN_MAX_VALUES

    printf("\n");

    printf("Load network...\n");

    fopen_s(&File, NNUE_FILE, "rb");

    if (File == NULL) { // File open error
        printf("File '%s' open error!\n", NNUE_FILE);

        Sleep(3000);

        exit(0);
    }

    // File magic

    fread(&FileMagic, 4, 1, File);

//  printf("FileMagic = %d\n", FileMagic);

    if (FileMagic != NNUE_FILE_MAGIC) { // File format error
        printf("File '%s' format error!\n", NNUE_FILE);

        Sleep(3000);

        exit(0);
    }

    // File hash

    fread(&FileHash, sizeof(U64), 1, File);

//  printf("FileHash = 0x%016llX\n", FileHash);
/*
    if (FileHash != NNUE_FILE_HASH) { // File format error
        printf("File '%s' format error!\n", NNUE_FILE);

        Sleep(3000);

        exit(0);
    }
*/
    // Feature weights

#ifdef PRINT_MIN_MAX_VALUES
    MinValue = FLT_MAX;
    MaxValue = FLT_MIN;
#endif // PRINT_MIN_MAX_VALUES

    for (int Index = 0; Index < FEATURE_DIMENSION * HIDDEN_DIMENSION; ++Index) { // 768 x 512 = 393216
        fread(&Value, sizeof(float), 1, File);

#ifdef PRINT_MIN_MAX_VALUES
        if (Value < MinValue) {
            MinValue = Value;
        }

        if (Value > MaxValue) {
            MaxValue = Value;
        }
#endif // PRINT_MIN_MAX_VALUES

        FeatureWeights[Index] = LoadWeight(Value, QUANTIZATION_PRECISION_IN);
    }

#ifdef PRINT_MIN_MAX_VALUES
    printf("Feature weights: MinValue = %f MaxValue = %f\n", MinValue, MaxValue);
#endif // PRINT_MIN_MAX_VALUES

    // Hidden biases

#ifdef PRINT_MIN_MAX_VALUES
    MinValue = FLT_MAX;
    MaxValue = FLT_MIN;
#endif // PRINT_MIN_MAX_VALUES

    for (int Index = 0; Index < HIDDEN_DIMENSION; ++Index) { // 512
        fread(&Value, sizeof(float), 1, File);

#ifdef PRINT_MIN_MAX_VALUES
        if (Value < MinValue) {
            MinValue = Value;
        }

        if (Value > MaxValue) {
            MaxValue = Value;
        }
#endif // PRINT_MIN_MAX_VALUES

        HiddenBiases[Index] = LoadWeight(Value, QUANTIZATION_PRECISION_IN);
    }

#ifdef PRINT_MIN_MAX_VALUES
    printf("Hidden biases: MinValue = %f MaxValue = %f\n", MinValue, MaxValue);
#endif // PRINT_MIN_MAX_VALUES

    // Hidden weights

#ifdef PRINT_MIN_MAX_VALUES
    MinValue = FLT_MAX;
    MaxValue = FLT_MIN;
#endif // PRINT_MIN_MAX_VALUES

    for (int Index = 0; Index < HIDDEN_DIMENSION * 2; ++Index) { // 512 x 2 = 1024
        fread(&Value, sizeof(float), 1, File);

#ifdef PRINT_MIN_MAX_VALUES
        if (Value < MinValue) {
            MinValue = Value;
        }

        if (Value > MaxValue) {
            MaxValue = Value;
        }
#endif // PRINT_MIN_MAX_VALUES

        HiddenWeights[Index] = LoadWeight(Value, QUANTIZATION_PRECISION_OUT);
    }

#ifdef PRINT_MIN_MAX_VALUES
    printf("Hidden weights: MinValue = %f MaxValue = %f\n", MinValue, MaxValue);
#endif // PRINT_MIN_MAX_VALUES

    // Output bias

    fread(&Value, sizeof(float), 1, File);

    OutputBias = LoadWeight(Value, QUANTIZATION_PRECISION_OUT);

#ifdef PRINT_MIN_MAX_VALUES
    printf("OutputBias: Value = %f\n", Value);
#endif // PRINT_MIN_MAX_VALUES

    fgetpos(File, &FilePos);

//  printf("File position = %llu\n", FilePos);

    if (FilePos != NNUE_FILE_SIZE) { // File format error
        printf("File '%s' format error!\n", NNUE_FILE);

        Sleep(3000);

        exit(0);
    }

    fclose(File);

    printf("Load network...DONE (%s; hash = 0x%016llX)\n", NNUE_FILE, FileHash);
}

#ifdef NET_KS
int KingIndex(const int KingSquare, const int Square)
{
    return (KingSquare & 4) == (Square & 4);
}
#elif defined(NET_KQ)
int KingIndex(const int KingSquare, const int Square)
{
    return (((KingSquare & 4) == (Square & 4)) << 1) + ((KingSquare & 32) == (Square & 32));
}
#endif // NET_KS/NET_KQ

int CalculateWeightIndex(const int Perspective, const int KingSquare, const int Square, const int PieceWithColor)
{
    int PieceIndex;
    int WeightIndex;

    if (Perspective == WHITE) {
        PieceIndex = PIECE(PieceWithColor) + 6 * COLOR(PieceWithColor);

#ifdef NET
        WeightIndex = (PieceIndex << 6) + Square;
#elif defined(NET_KS)
        WeightIndex = (PieceIndex << 6) + (KingIndex(KingSquare, Square) << 5) + HalfBoardIndex[Square];
#else // NET_KQ
        WeightIndex = (PieceIndex << 7) + (KingIndex(KingSquare, Square) << 5) + HalfBoardIndex[Square];
#endif // NET/NET_KS/NET_KQ
    }
    else { // BLACK
        PieceIndex = PIECE(PieceWithColor) + 6 * CHANGE_COLOR(COLOR(PieceWithColor));

#ifdef NET
        WeightIndex = (PieceIndex << 6) + (Square ^ 56);
#elif defined(NET_KS)
        WeightIndex = (PieceIndex << 6) + (KingIndex(KingSquare, Square) << 5) + HalfBoardIndex[Square ^ 56];
#else // NET_KQ
        WeightIndex = (PieceIndex << 7) + (KingIndex(KingSquare, Square) << 5) + HalfBoardIndex[Square ^ 56];
#endif // NET/NET_KS/NET_KQ
    }

//  printf("Perspective = %d KingSquare = %d Square = %d PieceWithColor = %d PieceIndex = %d KingIndex = %d WeightIndex = %d\n", Perspective, KingSquare, Square, PieceWithColor, PieceIndex, KingIndex(KingSquare, Square) << 5, WeightIndex);

    return WeightIndex;
}

void RefreshAccumulator(BoardItem* Board)
{
    I16(*Accumulation)[2][HIDDEN_DIMENSION] = &Board->Accumulator.Accumulation;

    for (int Perspective = 0; Perspective < 2; ++Perspective) { // White/Black
        memcpy((*Accumulation)[Perspective], HiddenBiases, sizeof(HiddenBiases));

        int KingSquare = LSB(Board->BB_Pieces[Perspective][KING]);

        U64 Pieces = (Board->BB_WhitePieces | Board->BB_BlackPieces);

        while (Pieces) {
            int Square = LSB(Pieces);

            int PieceWithColor = Board->Pieces[Square];

            int WeightIndex = CalculateWeightIndex(Perspective, KingSquare, Square, PieceWithColor);

//          printf("Square = %d Piece = %d Color = %d WeightIndex = %d\n", Square, PIECE(PieceWithColor), COLOR(PieceWithColor), WeightIndex);

#ifdef USE_NNUE_AVX2
            __m256i* AccumulatorTile = (__m256i*)(*Accumulation)[Perspective];

            __m256i* Column = (__m256i*) & FeatureWeights[WeightIndex * HIDDEN_DIMENSION];

            for (int Reg = 0; Reg < NUM_REGS; ++Reg) { // 32
                AccumulatorTile[Reg] = _mm256_add_epi16(AccumulatorTile[Reg], Column[Reg]);
            }
#else
            for (int Index = 0; Index < HIDDEN_DIMENSION; ++Index) { // 512
                (*Accumulation)[Perspective][Index] += FeatureWeights[WeightIndex * HIDDEN_DIMENSION + Index];
            }
#endif // USE_NNUE_AVX2

            Pieces &= Pieces - 1;
        }
    }

#ifdef USE_NNUE_REFRESH
    Board->Accumulator.AccumulationComputed = TRUE;
#endif // USE_NNUE_REFRESH
}

#ifdef USE_NNUE_REFRESH

void AccumulatorAdd(BoardItem* Board, const int Perspective, const int WeightIndex)
{
    I16(*Accumulation)[2][HIDDEN_DIMENSION] = &Board->Accumulator.Accumulation;

#ifdef USE_NNUE_AVX2
    __m256i* AccumulatorTile = (__m256i*)(*Accumulation)[Perspective];

    __m256i* Column = (__m256i*) & FeatureWeights[WeightIndex * HIDDEN_DIMENSION];

    for (int Reg = 0; Reg < NUM_REGS; ++Reg) { // 32
        AccumulatorTile[Reg] = _mm256_add_epi16(AccumulatorTile[Reg], Column[Reg]);
    }
#else
    for (int Index = 0; Index < HIDDEN_DIMENSION; ++Index) { // 512
        (*Accumulation)[Perspective][Index] += FeatureWeights[WeightIndex * HIDDEN_DIMENSION + Index];
    }
#endif // USE_NNUE_AVX2
}

void AccumulatorSub(BoardItem* Board, const int Perspective, const int WeightIndex)
{
    I16(*Accumulation)[2][HIDDEN_DIMENSION] = &Board->Accumulator.Accumulation;

#ifdef USE_NNUE_AVX2
    __m256i* AccumulatorTile = (__m256i*)(*Accumulation)[Perspective];

    __m256i* Column = (__m256i*) & FeatureWeights[WeightIndex * HIDDEN_DIMENSION];

    for (int Reg = 0; Reg < NUM_REGS; ++Reg) { // 32
        AccumulatorTile[Reg] = _mm256_sub_epi16(AccumulatorTile[Reg], Column[Reg]);
    }
#else
    for (int Index = 0; Index < HIDDEN_DIMENSION; ++Index) { // 512
        (*Accumulation)[Perspective][Index] -= FeatureWeights[WeightIndex * HIDDEN_DIMENSION + Index];
    }
#endif // USE_NNUE_AVX2
}

BOOL UpdateAccumulator(BoardItem* Board)
{
    HistoryItem* Info;

    int KingSquare;

    int PieceWithColor;

    int WeightIndex;

    if (Board->HalfMoveNumber == 0) {
        return FALSE;
    }

    Info = &Board->MoveTable[Board->HalfMoveNumber - 1]; // Prev. move info

    if (!Info->Accumulator.AccumulationComputed) {
        return FALSE;
    }

    if (Info->PieceFrom == KING) {
        return FALSE;
    }

    if (Info->Type & MOVE_NULL) {
        Board->Accumulator.AccumulationComputed = TRUE;

        return TRUE;
    }

    for (int Perspective = 0; Perspective < 2; ++Perspective) { // White/Black
        KingSquare = LSB(Board->BB_Pieces[Perspective][KING]);

        // Delete piece (from)

        PieceWithColor = PIECE_AND_COLOR(Info->PieceFrom, CHANGE_COLOR(Board->CurrentColor));

        WeightIndex = CalculateWeightIndex(Perspective, KingSquare, Info->From, PieceWithColor);

        AccumulatorSub(Board, Perspective, WeightIndex);

        // Delete piece (captured)

        if (Info->Type & MOVE_PAWN_PASSANT) {
            PieceWithColor = PIECE_AND_COLOR(PAWN, Board->CurrentColor);

            WeightIndex = CalculateWeightIndex(Perspective, KingSquare, Info->EatPawnSquare, PieceWithColor);

            AccumulatorSub(Board, Perspective, WeightIndex);
        }
        else if (Info->Type & MOVE_CAPTURE) {
            PieceWithColor = PIECE_AND_COLOR(Info->PieceTo, Board->CurrentColor);

            WeightIndex = CalculateWeightIndex(Perspective, KingSquare, Info->To, PieceWithColor);

            AccumulatorSub(Board, Perspective, WeightIndex);
        }

        // Add piece (to)

        if (Info->Type & MOVE_PAWN_PROMOTE) {
            PieceWithColor = PIECE_AND_COLOR(Info->PromotePiece, CHANGE_COLOR(Board->CurrentColor));

            WeightIndex = CalculateWeightIndex(Perspective, KingSquare, Info->To, PieceWithColor);

            AccumulatorAdd(Board, Perspective, WeightIndex);
        }
        else {
            PieceWithColor = PIECE_AND_COLOR(Info->PieceFrom, CHANGE_COLOR(Board->CurrentColor));

            WeightIndex = CalculateWeightIndex(Perspective, KingSquare, Info->To, PieceWithColor);

            AccumulatorAdd(Board, Perspective, WeightIndex);
        }
    } // for

#ifdef DEBUG_NNUE_2
    AccumulatorItem Accumulator = Board->Accumulator;

    RefreshAccumulator(Board);

    for (int Perspective = 0; Perspective < 2; ++Perspective) { // White/Black
        for (int Index = 0; Index < HIDDEN_DIMENSION; ++Index) { // 512
            if (Board->Accumulator.Accumulation[Perspective][Index] != Accumulator.Accumulation[Perspective][Index]) {
                printf("-- Accumulator error! Color = %d Piece = %d From = %d To = %d Move type = %d\n", CHANGE_COLOR(Board->CurrentColor), Info->PieceFrom, Info->From, Info->To, Info->Type);

                break; // for (512)
            }
        }
    }
#endif // DEBUG_NNUE_2

    Board->Accumulator.AccumulationComputed = TRUE;

    return TRUE;
}

#endif // USE_NNUE_REFRESH

I32 OutputLayer(BoardItem* Board)
{
    I32 Result = (I32)OutputBias * QUANTIZATION_PRECISION_IN;

#ifdef USE_NNUE_AVX2
    const __m256i ConstZero = _mm256_setzero_si256();

    __m256i Sum0 = _mm256_setzero_si256();
    __m256i Sum1 = _mm256_setzero_si256();

    __m256i* AccumulatorTile0 = (__m256i*) & Board->Accumulator.Accumulation[Board->CurrentColor];
    __m256i* AccumulatorTile1 = (__m256i*) & Board->Accumulator.Accumulation[CHANGE_COLOR(Board->CurrentColor)];

    __m256i* Weights0 = (__m256i*) & HiddenWeights;
    __m256i* Weights1 = (__m256i*) & HiddenWeights[HIDDEN_DIMENSION];

    for (int Reg = 0; Reg < NUM_REGS; ++Reg) { // 32
        const __m256i Acc0 = _mm256_max_epi16(ConstZero, AccumulatorTile0[Reg]); // ReLU
        const __m256i Acc1 = _mm256_max_epi16(ConstZero, AccumulatorTile1[Reg]); // ReLU

        Sum0 = _mm256_add_epi32(Sum0, _mm256_madd_epi16(Acc0, Weights0[Reg]));
        Sum1 = _mm256_add_epi32(Sum1, _mm256_madd_epi16(Acc1, Weights1[Reg]));
    }

    const __m256i R8 = _mm256_add_epi32(Sum0, Sum1);
    const __m128i R4 = _mm_add_epi32(_mm256_castsi256_si128(R8), _mm256_extractf128_si256(R8, 1));
    const __m128i R2 = _mm_add_epi32(R4, _mm_srli_si128(R4, 8));
    const __m128i R1 = _mm_add_epi32(R2, _mm_srli_si128(R2, 4));

    Result += _mm_cvtsi128_si32(R1);
#else
    I16(*Accumulation)[2][HIDDEN_DIMENSION] = &Board->Accumulator.Accumulation;

    for (int Index = 0; Index < HIDDEN_DIMENSION; ++Index) { // 512
        const I16 Acc0 = MAX(0, (*Accumulation)[Board->CurrentColor][Index]); // ReLU
        const I16 Acc1 = MAX(0, (*Accumulation)[CHANGE_COLOR(Board->CurrentColor)][Index]); // ReLU

        Result += Acc0 * HiddenWeights[Index]; // Offset 0
        Result += Acc1 * HiddenWeights[HIDDEN_DIMENSION + Index]; // Offset 512
    }
#endif // USE_NNUE_AVX2

    return Result / QUANTIZATION_PRECISION_IN / QUANTIZATION_PRECISION_OUT;
}

int NetworkEvaluate(BoardItem* Board)
{
    I32 OutputValue;

    // Transform: Board -> (512 x 2)

    if (!Board->Accumulator.AccumulationComputed) {
#ifdef USE_NNUE_REFRESH
        if (!UpdateAccumulator(Board)) {
#endif // USE_NNUE_REFRESH
            RefreshAccumulator(Board);
#ifdef USE_NNUE_REFRESH
        }
#endif // USE_NNUE_REFRESH
    }

    // Output: (512 x 2) -> 1

    OutputValue = OutputLayer(Board);

    return OutputValue;
}

#endif // NNUE_EVALUATION_FUNCTION_2