// NNUE2.cpp

#include "stdafx.h"

#include "NNUE2.h"

#include "BitBoard.h"
#include "Board.h"
#include "Def.h"
#include "Types.h"
#include "Utils.h"

#ifdef NNUE_EVALUATION_FUNCTION_2

#define NNUE_FILE_NAME              "rukchess.nnue"
#define NNUE_FILE_MAGIC             ('B' | 'R' << 8 | 'K' << 16 | 'R' << 24)
//#define NNUE_FILE_HASH            0x000071EB63511CB1
#define NNUE_FILE_SIZE              1579024

#define FEATURE_DIMENSION           768
#define HIDDEN_DIMENSION            512
#define OUTPUT_DIMENSION            1

#define QUANTIZATION_PRECISION_IN   32

#ifndef LAST_LAYER_AS_FLOAT
#define QUANTIZATION_PRECISION_OUT  512
#endif // !LAST_LAYER_AS_FLOAT

#ifdef USE_NNUE_AVX2
#define NUM_REGS                    (HIDDEN_DIMENSION * sizeof(I16) / sizeof(__m256i)) // 32
#endif // USE_NNUE_AVX2

_declspec(align(64)) I16 FeatureWeights[FEATURE_DIMENSION * HIDDEN_DIMENSION];  // 768 x 512 = 393216
_declspec(align(64)) I16 HiddenBiases[HIDDEN_DIMENSION];                        // 512

#ifdef LAST_LAYER_AS_FLOAT
_declspec(align(64)) float HiddenWeights[HIDDEN_DIMENSION * 2];                 // 512 x 2 = 1024
float OutputBias;                                                               // 1
#else
_declspec(align(64)) I16 HiddenWeights[HIDDEN_DIMENSION * 2];                   // 512 x 2 = 1024
I16 OutputBias;                                                                 // 1
#endif // LAST_LAYER_AS_FLOAT

I16 LoadWeight(const float Value, const int Precision)
{
    I16 Result;

//    if (Value < (float)SHRT_MIN / (float)Precision) {
//        Result = 0;
//        Result = SHRT_MIN;

//        printf("Value = %f\n", Value);
//    }
//    else if (Value > (float)SHRT_MAX / (float)Precision) {
//        Result = 0;
//        Result = SHRT_MAX;

//        printf("Value = %f\n", Value);
//    }
//    else {
        Result = (I16)roundf(Value * (float)Precision);
//    }

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

    fopen_s(&File, NNUE_FILE_NAME, "rb");

    if (File == NULL) { // File open error
        printf("File '%s' open error!\n", NNUE_FILE_NAME);

        Sleep(3000);

        exit(0);
    }

    // File magic

    fread(&FileMagic, 4, 1, File);

//    printf("FileMagic = %d\n", FileMagic);

    if (FileMagic != NNUE_FILE_MAGIC) { // File format error
        printf("File '%s' format error!\n", NNUE_FILE_NAME);

        Sleep(3000);

        exit(0);
    }

    // File hash

    fread(&FileHash, sizeof(U64), 1, File);

//    printf("FileHash = 0x%016llX\n", FileHash);
/*
    if (FileHash != NNUE_FILE_HASH) { // File format error
        printf("File '%s' format error!\n", NNUE_FILE_NAME);

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

#ifdef LAST_LAYER_AS_FLOAT
        HiddenWeights[Index] = Value;
#else
        HiddenWeights[Index] = LoadWeight(Value, QUANTIZATION_PRECISION_OUT);
#endif // LAST_LAYER_AS_FLOAT
    }

#ifdef PRINT_MIN_MAX_VALUES
    printf("Hidden weights: MinValue = %f MaxValue = %f\n", MinValue, MaxValue);
#endif // PRINT_MIN_MAX_VALUES

    // Output bias

    fread(&Value, sizeof(float), 1, File);

#ifdef LAST_LAYER_AS_FLOAT
    OutputBias = Value;
#else
    OutputBias = LoadWeight(Value, QUANTIZATION_PRECISION_OUT);
#endif // LAST_LAYER_AS_FLOAT

#ifdef PRINT_MIN_MAX_VALUES
    printf("OutputBias: Value = %f\n", Value);
#endif // PRINT_MIN_MAX_VALUES

    fgetpos(File, &FilePos);

//    printf("File position = %llu\n", FilePos);

    if (FilePos != NNUE_FILE_SIZE) { // File format error
        printf("File '%s' format error!\n", NNUE_FILE_NAME);

        Sleep(3000);

        exit(0);
    }

    fclose(File);

    printf("Load network...DONE (%s; hash = 0x%016llX)\n", NNUE_FILE_NAME, FileHash);
}

int CalculateWeightIndex(const int Perspective, const int Square, const int PieceWithColor)
{
    int PieceIndex;
    int WeightIndex;

    if (Perspective == WHITE) {
        PieceIndex = PIECE(PieceWithColor) + 6 * COLOR(PieceWithColor);

        WeightIndex = (PieceIndex << 6) + Square;
    }
    else { // BLACK
        PieceIndex = PIECE(PieceWithColor) + 6 * CHANGE_COLOR(COLOR(PieceWithColor));

        WeightIndex = (PieceIndex << 6) + (Square ^ 56);
    }

#ifdef PRINT_WEIGHT_INDEX
    printf("Perspective = %d Square = %d Piece = %d Color = %d PieceIndex = %d WeightIndex = %d\n", Perspective, Square, PIECE(PieceWithColor), COLOR(PieceWithColor), PieceIndex, WeightIndex);
#endif // PRINT_WEIGHT_INDEX

    return WeightIndex;
}

void RefreshAccumulator(BoardItem* Board)
{
    I16 (*Accumulation)[2][HIDDEN_DIMENSION] = &Board->Accumulator.Accumulation;

    for (int Perspective = 0; Perspective < 2; ++Perspective) { // White/Black
        memcpy((*Accumulation)[Perspective], HiddenBiases, sizeof(HiddenBiases));

        U64 Pieces = (Board->BB_WhitePieces | Board->BB_BlackPieces);

        while (Pieces) {
            int Square = LSB(Pieces);

            int PieceWithColor = Board->Pieces[Square];

            int WeightIndex = CalculateWeightIndex(Perspective, Square, PieceWithColor);

#ifdef USE_NNUE_AVX2
            __m256i* AccumulatorTile = (__m256i*)(*Accumulation)[Perspective];

            __m256i* Column = (__m256i*)&FeatureWeights[WeightIndex * HIDDEN_DIMENSION];

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

#ifdef USE_NNUE_UPDATE
    Board->Accumulator.AccumulationComputed = TRUE;
#endif // USE_NNUE_UPDATE
}

#ifdef USE_NNUE_UPDATE

void AccumulatorAdd(BoardItem* Board, const int Perspective, const int WeightIndex)
{
    I16 (*Accumulation)[2][HIDDEN_DIMENSION] = &Board->Accumulator.Accumulation;

#ifdef USE_NNUE_AVX2
    __m256i* AccumulatorTile = (__m256i*)(*Accumulation)[Perspective];

    __m256i* Column = (__m256i*)&FeatureWeights[WeightIndex * HIDDEN_DIMENSION];

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
    I16 (*Accumulation)[2][HIDDEN_DIMENSION] = &Board->Accumulator.Accumulation;

#ifdef USE_NNUE_AVX2
    __m256i* AccumulatorTile = (__m256i*)(*Accumulation)[Perspective];

    __m256i* Column = (__m256i*)&FeatureWeights[WeightIndex * HIDDEN_DIMENSION];

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

    int PieceWithColor;

    int WeightIndex;

    if (Board->HalfMoveNumber == 0) {
        return FALSE;
    }

    Info = &Board->MoveTable[Board->HalfMoveNumber - 1]; // Prev. move info

    if (!Info->Accumulator.AccumulationComputed) {
        return FALSE;
    }

    if (Info->Type & (MOVE_CASTLE_KING | MOVE_CASTLE_QUEEN)) {
        return FALSE;
    }

    if (Info->Type & MOVE_NULL) {
        Board->Accumulator.AccumulationComputed = TRUE;

        return TRUE;
    }

    for (int Perspective = 0; Perspective < 2; ++Perspective) { // White/Black
        // Delete piece (from)

        PieceWithColor = PIECE_AND_COLOR(Info->PieceFrom, CHANGE_COLOR(Board->CurrentColor));

        WeightIndex = CalculateWeightIndex(Perspective, Info->From, PieceWithColor);

        AccumulatorSub(Board, Perspective, WeightIndex);

        // Delete piece (captured)

        if (Info->Type & MOVE_PAWN_PASSANT) {
            PieceWithColor = PIECE_AND_COLOR(PAWN, Board->CurrentColor);

            WeightIndex = CalculateWeightIndex(Perspective, Info->EatPawnSquare, PieceWithColor);

            AccumulatorSub(Board, Perspective, WeightIndex);
        }
        else if (Info->Type & MOVE_CAPTURE) {
            PieceWithColor = PIECE_AND_COLOR(Info->PieceTo, Board->CurrentColor);

            WeightIndex = CalculateWeightIndex(Perspective, Info->To, PieceWithColor);

            AccumulatorSub(Board, Perspective, WeightIndex);
        }

        // Add piece (to)

        if (Info->Type & MOVE_PAWN_PROMOTE) {
            PieceWithColor = PIECE_AND_COLOR(Info->PromotePiece, CHANGE_COLOR(Board->CurrentColor));

            WeightIndex = CalculateWeightIndex(Perspective, Info->To, PieceWithColor);

            AccumulatorAdd(Board, Perspective, WeightIndex);
        }
        else {
            PieceWithColor = PIECE_AND_COLOR(Info->PieceFrom, CHANGE_COLOR(Board->CurrentColor));

            WeightIndex = CalculateWeightIndex(Perspective, Info->To, PieceWithColor);

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

#endif // USE_NNUE_UPDATE

#ifdef LAST_LAYER_AS_FLOAT
I32 OutputLayer(BoardItem* Board)
{
    float Result = OutputBias * QUANTIZATION_PRECISION_IN;

//#ifdef USE_NNUE_AVX2
    // TODO
//#else
    I16 (*Accumulation)[2][HIDDEN_DIMENSION] = &Board->Accumulator.Accumulation;

    for (int Index = 0; Index < HIDDEN_DIMENSION; ++Index) { // 512
        const I16 Acc0 = MAX(0, (*Accumulation)[Board->CurrentColor][Index]); // ReLU
        const I16 Acc1 = MAX(0, (*Accumulation)[CHANGE_COLOR(Board->CurrentColor)][Index]); // ReLU

        Result += (float)Acc0 * HiddenWeights[Index]; // Offset 0
        Result += (float)Acc1 * HiddenWeights[HIDDEN_DIMENSION + Index]; // Offset 512
    }
//#endif // USE_NNUE_AVX2

    return (I32)(Result / QUANTIZATION_PRECISION_IN);
}
#else
I32 OutputLayer(BoardItem* Board)
{
    I32 Result = (I32)OutputBias * QUANTIZATION_PRECISION_IN;

#ifdef PRINT_ACCUMULATOR
    for (int Index = 0; Index < HIDDEN_DIMENSION; ++Index) { // 512
        const I16 Acc0 = Board->Accumulator.Accumulation[Board->CurrentColor][Index];
        const I16 Acc1 = Board->Accumulator.Accumulation[CHANGE_COLOR(Board->CurrentColor)][Index];

        printf("Index = %d Acc0 = %d Acc1 = %d\n", Index, Acc0, Acc1);
    }
#endif // PRINT_ACCUMULATOR

#ifdef USE_NNUE_AVX2
    const __m256i ConstZero = _mm256_setzero_si256();

    __m256i Sum0 = _mm256_setzero_si256();
    __m256i Sum1 = _mm256_setzero_si256();

    __m256i* AccumulatorTile0 = (__m256i*)&Board->Accumulator.Accumulation[Board->CurrentColor];
    __m256i* AccumulatorTile1 = (__m256i*)&Board->Accumulator.Accumulation[CHANGE_COLOR(Board->CurrentColor)];

    __m256i* Weights0 = (__m256i*)&HiddenWeights;
    __m256i* Weights1 = (__m256i*)&HiddenWeights[HIDDEN_DIMENSION];

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
    I16 (*Accumulation)[2][HIDDEN_DIMENSION] = &Board->Accumulator.Accumulation;

    for (int Index = 0; Index < HIDDEN_DIMENSION; ++Index) { // 512
        const I16 Acc0 = MAX(0, (*Accumulation)[Board->CurrentColor][Index]); // ReLU
        const I16 Acc1 = MAX(0, (*Accumulation)[CHANGE_COLOR(Board->CurrentColor)][Index]); // ReLU

        Result += Acc0 * HiddenWeights[Index]; // Offset 0
        Result += Acc1 * HiddenWeights[HIDDEN_DIMENSION + Index]; // Offset 512
    }
#endif // USE_NNUE_AVX2

    return Result / QUANTIZATION_PRECISION_IN / QUANTIZATION_PRECISION_OUT;
}
#endif // LAST_LAYER_AS_FLOAT

int NetworkEvaluate(BoardItem* Board)
{
    I32 OutputValue;

    // Transform: Board -> (512 x 2)

    if (!Board->Accumulator.AccumulationComputed) {
#ifdef USE_NNUE_UPDATE
        if (!UpdateAccumulator(Board)) {
#endif // USE_NNUE_UPDATE
            RefreshAccumulator(Board);
#ifdef USE_NNUE_UPDATE
        }
#endif // USE_NNUE_UPDATE
    }

    // Output: (512 x 2) -> 1

    OutputValue = OutputLayer(Board);

    return OutputValue;
}

#endif // NNUE_EVALUATION_FUNCTION_2