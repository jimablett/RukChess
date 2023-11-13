// NNUE.cpp

#include "stdafx.h"

#include "NNUE.h"

#include "BitBoard.h"
#include "Board.h"
#include "Def.h"
#include "Game.h"
#include "Types.h"
#include "Utils.h"

#ifdef NNUE_EVALUATION_FUNCTION

#define NNUE_VERSION                    0x7AF32F16U
#define NNUE_HASH                       0x3E5AA6EEU
#define NNUE_ARC_LENGTH                 177

#define NNUE_HEADER_1                   0x5D69D7B8U
#define NNUE_HEADER_2                   0x63337156U

#define NNUE_FILE_SIZE                  21022697

#define HALF_FEATURE_INPUT_DIMENSION    41024 // 64 king squares x 641 (64 squares x 5 pieces x 2 colors + 1)
#define HALF_FEATURE_OUTPUT_DIMENSION   256

#define INPUT_DIMENSION                 512 // 256 x 2
#define HIDDEN_1_DIMENSION              32
#define HIDDEN_2_DIMENSION              32
#define OUTPUT_DIMENSION                1

#define SHIFT                           6 // Divide by 64
#define SCALE                           16

#define WHITE_PAWN                      (1)
#define BLACK_PAWN                      (1 * 64 + 1)
#define WHITE_KNIGHT                    (2 * 64 + 1)
#define BLACK_KNIGHT                    (3 * 64 + 1)
#define WHITE_BISHOP                    (4 * 64 + 1)
#define BLACK_BISHOP                    (5 * 64 + 1)
#define WHITE_ROOK                      (6 * 64 + 1)
#define BLACK_ROOK                      (7 * 64 + 1)
#define WHITE_QUEEN                     (8 * 64 + 1)
#define BLACK_QUEEN                     (9 * 64 + 1)
#define PIECES_END                      (10 * 64 + 1)

#ifdef USE_NNUE_AVX2
#define NUM_REGS                        (HALF_FEATURE_OUTPUT_DIMENSION * sizeof(I16) / sizeof(__m256i)) // 16
#endif // USE_NNUE_AVX2

const int PieceToIndex[2][16] = { // [Perspective][PieceWithColor]
    {
        WHITE_PAWN, WHITE_KNIGHT, WHITE_BISHOP, WHITE_ROOK, WHITE_QUEEN, 0, 0, 0,
        BLACK_PAWN, BLACK_KNIGHT, BLACK_BISHOP, BLACK_ROOK, BLACK_QUEEN, 0, 0, 0
    },
    {
        BLACK_PAWN, BLACK_KNIGHT, BLACK_BISHOP, BLACK_ROOK, BLACK_QUEEN, 0, 0, 0,
        WHITE_PAWN, WHITE_KNIGHT, WHITE_BISHOP, WHITE_ROOK, WHITE_QUEEN, 0, 0, 0
    }
};

_declspec(align(64)) I16 InputBiases[HALF_FEATURE_OUTPUT_DIMENSION]; // 256
_declspec(align(64)) I16 InputWeights[HALF_FEATURE_INPUT_DIMENSION * HALF_FEATURE_OUTPUT_DIMENSION]; // 41024 x 256 = 10502144

_declspec(align(64)) I32 Hidden_1_Biases[HIDDEN_1_DIMENSION]; // 32
_declspec(align(64)) I8 Hidden_1_Weights[INPUT_DIMENSION * HIDDEN_1_DIMENSION]; // 512 x 32 = 16384

_declspec(align(64)) I32 Hidden_2_Biases[HIDDEN_2_DIMENSION]; // 32
_declspec(align(64)) I8 Hidden_2_Weights[HIDDEN_1_DIMENSION * HIDDEN_2_DIMENSION]; // 32 x 32 = 1024

I32 OutputBiases[OUTPUT_DIMENSION]; // 1
_declspec(align(64)) I8 OutputWeights[HIDDEN_2_DIMENSION * OUTPUT_DIMENSION]; // 32 x 1 = 32

BOOL NnueFileLoaded = FALSE;

I32 ReadInt32(FILE* File, const char* NnueFileName)
{
    char Buf[sizeof(I32) + 1]; // 4 bytes + 1
    char* Part = Buf;

    I8 Byte;

    I32 Result = 0;

    if (fread_s(Buf, sizeof(Buf), sizeof(char), sizeof(I32), File) != sizeof(I32)) {
        if (PrintMode == PRINT_MODE_NORMAL) {
            printf("File '%s' read error!\n", NnueFileName);
        }
        else if (PrintMode == PRINT_MODE_UCI) {
            printf("info string File '%s' read error!\n", NnueFileName);
        }

        Sleep(3000);

        exit(0);
    }

    for (int Index = 0; Index < sizeof(I32); ++Index) { // 4
        Byte = *Part;

        Result |= (Byte & 0xFF) << (Index << 3);

        ++Part;
    }

    return Result;
}

I16 ReadInt16(FILE* File, const char* NnueFileName)
{
    char Buf[sizeof(I16) + 1]; // 2 bytes + 1
    char* Part = Buf;

    I8 Byte;

    I16 Result = 0;

    if (fread_s(Buf, sizeof(Buf), sizeof(char), sizeof(I16), File) != sizeof(I16)) {
        if (PrintMode == PRINT_MODE_NORMAL) {
            printf("File '%s' read error!\n", NnueFileName);
        }
        else if (PrintMode == PRINT_MODE_UCI) {
            printf("info string File '%s' read error!\n", NnueFileName);
        }

        Sleep(3000);

        exit(0);
    }

    for (int Index = 0; Index < sizeof(I16); ++Index) { // 2
        Byte = *Part;

        Result |= (Byte & 0xFF) << (Index << 3);

        ++Part;
    }

    return Result;
}

I8 ReadInt8(FILE* File, const char* NnueFileName)
{
    char Buf[sizeof(I8) + 1]; // 1 byte + 1
    char* Part = Buf;

    I8 Byte;

    I8 Result = 0;

    if (fread_s(Buf, sizeof(Buf), sizeof(char), sizeof(I8), File) != sizeof(I8)) {
        if (PrintMode == PRINT_MODE_NORMAL) {
            printf("File '%s' read error!\n", NnueFileName);
        }
        else if (PrintMode == PRINT_MODE_UCI) {
            printf("info string File '%s' read error!\n", NnueFileName);
        }

        Sleep(3000);

        exit(0);
    }

    for (int Index = 0; Index < sizeof(I8); ++Index) { // 1
        Byte = *Part;

        Result |= (Byte & 0xFF) << (Index << 3);

        ++Part;
    }

    return Result;
}

int WeightIndex(int Row, int Col, const int Dims)
{
#ifdef USE_NNUE_AVX2
    if (Dims > 32) {
        int Temp = Col & 0x18;

        Temp = (Temp << 1) | (Temp >> 1);

        Col = (Col & ~0x18) | (Temp & 0x18);
    }
#endif // USE_NNUE_AVX2

    return Col * 32 + Row;
}

#ifdef USE_NNUE_AVX2
void PermuteBiases(I32* Biases)
{
    __m128i* BiasesTile = (__m128i*)Biases;

    __m128i Temp[8];

    Temp[0] = BiasesTile[0];
    Temp[1] = BiasesTile[4];
    Temp[2] = BiasesTile[1];
    Temp[3] = BiasesTile[5];
    Temp[4] = BiasesTile[2];
    Temp[5] = BiasesTile[6];
    Temp[6] = BiasesTile[3];
    Temp[7] = BiasesTile[7];

    memcpy(BiasesTile, Temp, 8 * sizeof(__m128i));
}
#endif // USE_NNUE_AVX2

BOOL LoadNetwork(const char* NnueFileName)
{
    FILE* File;

    I32 Version;
    I32 Hash;
    I32 ArcLength;

    char Architecture[NNUE_ARC_LENGTH + 1];

    I32 Header1;
    I32 Header2;

    fpos_t FilePos;

    if (PrintMode == PRINT_MODE_NORMAL) {
        printf("\n");

        printf("Load network...\n");
    }

    fopen_s(&File, NnueFileName, "rb");

    if (File == NULL) { // File open error
        if (PrintMode == PRINT_MODE_NORMAL) {
            printf("File '%s' open error!\n", NnueFileName);
        }
        else if (PrintMode == PRINT_MODE_UCI) {
            printf("info string File '%s' open error!\n", NnueFileName);
        }

        return FALSE;
    }

    // Version

    Version = ReadInt32(File, NnueFileName);

//    printf("Version = 0x%8X\n", Version);

    if (Version != NNUE_VERSION) { // File format error
        if (PrintMode == PRINT_MODE_NORMAL) {
            printf("File '%s' format error!\n", NnueFileName);
        }
        else if (PrintMode == PRINT_MODE_UCI) {
            printf("info string File '%s' format error!\n", NnueFileName);
        }

        return FALSE;
    }

    // Hash

    Hash = ReadInt32(File, NnueFileName);

//    printf("Hash = 0x%8X\n", Hash);

    if (Hash != NNUE_HASH) { // File format error
        if (PrintMode == PRINT_MODE_NORMAL) {
            printf("File '%s' format error!\n", NnueFileName);
        }
        else if (PrintMode == PRINT_MODE_UCI) {
            printf("info string File '%s' format error!\n", NnueFileName);
        }

        return FALSE;
    }

    // Architecture length

    ArcLength = ReadInt32(File, NnueFileName);

//    printf("Architecture length = %d\n", ArcLength);

    if (ArcLength != NNUE_ARC_LENGTH) { // File format error
        if (PrintMode == PRINT_MODE_NORMAL) {
            printf("File '%s' format error!\n", NnueFileName);
        }
        else if (PrintMode == PRINT_MODE_UCI) {
            printf("info string File '%s' format error!\n", NnueFileName);
        }

        return FALSE;
    }

    // Architecture

    if (fgets(Architecture, NNUE_ARC_LENGTH + 1, File) == NULL) { // File read error
        if (PrintMode == PRINT_MODE_NORMAL) {
            printf("File '%s' read error!\n", NnueFileName);
        }
        else if (PrintMode == PRINT_MODE_UCI) {
            printf("info string File '%s' read error!\n", NnueFileName);
        }

        return FALSE;
    }

//    printf("Architecture = %s\n", Architecture);

    // Header 1

    Header1 = ReadInt32(File, NnueFileName);

//    printf("Header 1 = 0x%8X\n", Header1);

    if (Header1 != NNUE_HEADER_1) { // File format error
        if (PrintMode == PRINT_MODE_NORMAL) {
            printf("File '%s' format error!\n", NnueFileName);
        }
        else if (PrintMode == PRINT_MODE_UCI) {
            printf("info string File '%s' format error!\n", NnueFileName);
        }

        return FALSE;
    }

    // Input biases

    for (int Index = 0; Index < HALF_FEATURE_OUTPUT_DIMENSION; ++Index) { // 256
        InputBiases[Index] = ReadInt16(File, NnueFileName);

//        printf("InputBiases[%d] = %d\n", Index, InputBiases[Index]);
    }

    // Input weights

    for (int Index = 0; Index < HALF_FEATURE_INPUT_DIMENSION * HALF_FEATURE_OUTPUT_DIMENSION; ++Index) { // 41024 x 256 = 10502144
        InputWeights[Index] = ReadInt16(File, NnueFileName);

//        printf("InputWeights[%d] = %d\n", Index, InputWeights[Index]);
    }

    // Header 2

    Header2 = ReadInt32(File, NnueFileName);

//    printf("Header 2 = 0x%8X\n", Header2);

    if (Header2 != NNUE_HEADER_2) { // File format error
        if (PrintMode == PRINT_MODE_NORMAL) {
            printf("File '%s' format error!\n", NnueFileName);
        }
        else if (PrintMode == PRINT_MODE_UCI) {
            printf("info string File '%s' format error!\n", NnueFileName);
        }

        return FALSE;
    }

    // Hidden 1 biases

    for (int Index = 0; Index < HIDDEN_1_DIMENSION; ++Index) { // 32
        Hidden_1_Biases[Index] = ReadInt32(File, NnueFileName);

//        printf("Hidden_1_Biases[%d] = %d\n", Index, Hidden_1_Biases[Index]);
    }

#ifdef USE_NNUE_AVX2
    PermuteBiases(Hidden_1_Biases);
#endif // USE_NNUE_AVX2

    // Hidden 1 weights - 512 x 32 = 16384

    for (int Row = 0; Row < HIDDEN_1_DIMENSION; ++Row) { // 32
        for (int Col = 0; Col < INPUT_DIMENSION; ++Col) { // 512
            int Index = WeightIndex(Row, Col, INPUT_DIMENSION);

            Hidden_1_Weights[Index] = ReadInt8(File, NnueFileName);

//            printf("Hidden_1_Weights[%d] = %d\n", Index, Hidden_1_Weights[Index]);
        }
    }

    // Hidden 2 biases

    for (int Index = 0; Index < HIDDEN_2_DIMENSION; ++Index) { // 32
        Hidden_2_Biases[Index] = ReadInt32(File, NnueFileName);

//        printf("Hidden_2_Biases[%d] = %d\n", Index, Hidden_2_Biases[Index]);
    }

#ifdef USE_NNUE_AVX2
    PermuteBiases(Hidden_2_Biases);
#endif // USE_NNUE_AVX2

    // Hidden 2 weights - 32 x 32 = 1024

    for (int Row = 0; Row < HIDDEN_2_DIMENSION; ++Row) { // 32
        for (int Col = 0; Col < HIDDEN_1_DIMENSION; ++Col) { // 32
            int Index = WeightIndex(Row, Col, HIDDEN_1_DIMENSION);

            Hidden_2_Weights[Index] = ReadInt8(File, NnueFileName);

//            printf("Hidden_2_Weights[%d] = %d\n", Index, Hidden_2_Weights[Index]);
        }
    }

    // Output biases

    for (int Index = 0; Index < OUTPUT_DIMENSION; ++Index) { // 1
        OutputBiases[Index] = ReadInt32(File, NnueFileName);

//        printf("OutputBiases[%d] = %d\n", Index, OutputBiases[Index]);
    }

    // Output weights

    for (int Index = 0; Index < HIDDEN_2_DIMENSION * OUTPUT_DIMENSION; ++Index) { // 32 x 1 = 32
        OutputWeights[Index] = ReadInt8(File, NnueFileName);

//        printf("OutputWeights[%d] = %d\n", Index, OutputWeights[Index]);
    }

    fgetpos(File, &FilePos);

//    printf("File position = %llu\n", FilePos);

    if (FilePos != NNUE_FILE_SIZE) { // File format error
        if (PrintMode == PRINT_MODE_NORMAL) {
            printf("File '%s' format error!\n", NnueFileName);
        }
        else if (PrintMode == PRINT_MODE_UCI) {
            printf("info string File '%s' format error!\n", NnueFileName);
        }

        return FALSE;
    }

    fclose(File);

    if (PrintMode == PRINT_MODE_NORMAL) {
        printf("Load network...DONE (%s)\n", NnueFileName);
    }
    else if (PrintMode == PRINT_MODE_UCI) {
        printf("info string Network loaded (%s)\n", NnueFileName);
    }

    return TRUE;
}

int Orient(const int Perspective, const int Square)
{
    return Square ^ (Perspective == WHITE ? 63 : 0);
}

int CalculateWeightIndex(const int Perspective, const int OrientKingSquare, const int Square, const int PieceWithColor)
{
    return OrientKingSquare * PIECES_END + PieceToIndex[Perspective][PieceWithColor] + Orient(Perspective, Square);
}

void RefreshAccumulator(BoardItem* Board)
{
    I16(*Accumulation)[2][HALF_FEATURE_OUTPUT_DIMENSION] = &Board->Accumulator.Accumulation;

#ifdef USE_NNUE_AVX2
    for (int Perspective = 0; Perspective < 2; ++Perspective) { // White/Black
        __m256i* BiasesTile = (__m256i*)&InputBiases[0];
        __m256i* AccumulatorTile = (__m256i*)(*Accumulation)[Perspective];

        __m256i Acc[NUM_REGS]; // 16

        for (int Reg = 0; Reg < NUM_REGS; ++Reg) { // 16
            Acc[Reg] = BiasesTile[Reg];
        }

        int OrientKingSquare = Orient(Perspective, LSB(Board->BB_Pieces[Perspective][KING]));

        U64 Pieces = (Board->BB_WhitePieces | Board->BB_BlackPieces) & ~Board->BB_Pieces[WHITE][KING] & ~Board->BB_Pieces[BLACK][KING];

        while (Pieces) {
            int Square = LSB(Pieces);

            int PieceWithColor = Board->Pieces[Square];

            int WeightIndex = CalculateWeightIndex(Perspective, OrientKingSquare, Square, PieceWithColor);

            __m256i* Column = (__m256i*)&InputWeights[WeightIndex * HALF_FEATURE_OUTPUT_DIMENSION];

            for (int Reg = 0; Reg < NUM_REGS; ++Reg) { // 16
                Acc[Reg] = _mm256_add_epi16(Acc[Reg], Column[Reg]);
            }

            Pieces &= Pieces - 1;
        }

        for (int Reg = 0; Reg < NUM_REGS; ++Reg) { // 16
            AccumulatorTile[Reg] = Acc[Reg];
        }
    }
#else
    for (int Perspective = 0; Perspective < 2; ++Perspective) { // White/Black
        for (int IndexOutput = 0; IndexOutput < HALF_FEATURE_OUTPUT_DIMENSION; ++IndexOutput) { // 256
            (*Accumulation)[Perspective][IndexOutput] = InputBiases[IndexOutput];
        }

        int OrientKingSquare = Orient(Perspective, LSB(Board->BB_Pieces[Perspective][KING]));

        U64 Pieces = (Board->BB_WhitePieces | Board->BB_BlackPieces) & ~Board->BB_Pieces[WHITE][KING] & ~Board->BB_Pieces[BLACK][KING];

        while (Pieces) {
            int Square = LSB(Pieces);

            int PieceWithColor = Board->Pieces[Square];

            int WeightIndex = CalculateWeightIndex(Perspective, OrientKingSquare, Square, PieceWithColor);

            for (int IndexOutput = 0; IndexOutput < HALF_FEATURE_OUTPUT_DIMENSION; ++IndexOutput) { // 256
                (*Accumulation)[Perspective][IndexOutput] += InputWeights[WeightIndex * HALF_FEATURE_OUTPUT_DIMENSION + IndexOutput];
            }

            Pieces &= Pieces - 1;
        }
    }
#endif // USE_NNUE_AVX2

#ifdef USE_NNUE_UPDATE
    Board->Accumulator.AccumulationComputed = TRUE;
#endif // USE_NNUE_UPDATE
}

#ifdef USE_NNUE_UPDATE

void AccumulatorAdd(BoardItem* Board, const int Perspective, const int WeightIndex)
{
    I16(*Accumulation)[2][HALF_FEATURE_OUTPUT_DIMENSION] = &Board->Accumulator.Accumulation;

#ifdef USE_NNUE_AVX2
    __m256i* AccumulatorTile = (__m256i*)(*Accumulation)[Perspective];

    __m256i* Column = (__m256i*)&InputWeights[WeightIndex * HALF_FEATURE_OUTPUT_DIMENSION];

    for (int Reg = 0; Reg < NUM_REGS; ++Reg) { // 16
        AccumulatorTile[Reg] = _mm256_add_epi16(AccumulatorTile[Reg], Column[Reg]);
    }
#else
    for (int IndexOutput = 0; IndexOutput < HALF_FEATURE_OUTPUT_DIMENSION; ++IndexOutput) { // 256
        (*Accumulation)[Perspective][IndexOutput] += InputWeights[WeightIndex * HALF_FEATURE_OUTPUT_DIMENSION + IndexOutput];
    }
#endif // USE_NNUE_AVX2
}

void AccumulatorSub(BoardItem* Board, const int Perspective, const int WeightIndex)
{
    I16(*Accumulation)[2][HALF_FEATURE_OUTPUT_DIMENSION] = &Board->Accumulator.Accumulation;

#ifdef USE_NNUE_AVX2
    __m256i* AccumulatorTile = (__m256i*)(*Accumulation)[Perspective];

    __m256i* Column = (__m256i*)&InputWeights[WeightIndex * HALF_FEATURE_OUTPUT_DIMENSION];

    for (int Reg = 0; Reg < NUM_REGS; ++Reg) { // 16
        AccumulatorTile[Reg] = _mm256_sub_epi16(AccumulatorTile[Reg], Column[Reg]);
    }
#else
    for (int IndexOutput = 0; IndexOutput < HALF_FEATURE_OUTPUT_DIMENSION; ++IndexOutput) { // 256
        (*Accumulation)[Perspective][IndexOutput] -= InputWeights[WeightIndex * HALF_FEATURE_OUTPUT_DIMENSION + IndexOutput];
    }
#endif // USE_NNUE_AVX2
}

BOOL UpdateAccumulator(BoardItem* Board)
{
    HistoryItem* Info;

    int OrientKingSquare;

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
        OrientKingSquare = Orient(Perspective, LSB(Board->BB_Pieces[Perspective][KING]));

        // Delete piece (from)

        PieceWithColor = PIECE_AND_COLOR(Info->PieceFrom, CHANGE_COLOR(Board->CurrentColor));

        WeightIndex = CalculateWeightIndex(Perspective, OrientKingSquare, Info->From, PieceWithColor);

        AccumulatorSub(Board, Perspective, WeightIndex);

        // Delete piece (captured)

        if (Info->Type & MOVE_PAWN_PASSANT) {
            PieceWithColor = PIECE_AND_COLOR(PAWN, Board->CurrentColor);

            WeightIndex = CalculateWeightIndex(Perspective, OrientKingSquare, Info->EatPawnSquare, PieceWithColor);

            AccumulatorSub(Board, Perspective, WeightIndex);
        }
        else if (Info->Type & MOVE_CAPTURE) {
            PieceWithColor = PIECE_AND_COLOR(Info->PieceTo, Board->CurrentColor);

            WeightIndex = CalculateWeightIndex(Perspective, OrientKingSquare, Info->To, PieceWithColor);

            AccumulatorSub(Board, Perspective, WeightIndex);
        }

        // Add piece (to)

        if (Info->Type & MOVE_PAWN_PROMOTE) {
            PieceWithColor = PIECE_AND_COLOR(Info->PromotePiece, CHANGE_COLOR(Board->CurrentColor));

            WeightIndex = CalculateWeightIndex(Perspective, OrientKingSquare, Info->To, PieceWithColor);

            AccumulatorAdd(Board, Perspective, WeightIndex);
        }
        else {
            PieceWithColor = PIECE_AND_COLOR(Info->PieceFrom, CHANGE_COLOR(Board->CurrentColor));

            WeightIndex = CalculateWeightIndex(Perspective, OrientKingSquare, Info->To, PieceWithColor);

            AccumulatorAdd(Board, Perspective, WeightIndex);
        }
    } // for

#ifdef DEBUG_NNUE
    AccumulatorItem Accumulator = Board->Accumulator;

    RefreshAccumulator(Board);

    for (int Perspective = 0; Perspective < 2; ++Perspective) { // White/Black
        for (int IndexOutput = 0; IndexOutput < HALF_FEATURE_OUTPUT_DIMENSION; ++IndexOutput) { // 256
            if (Board->Accumulator.Accumulation[Perspective][IndexOutput] != Accumulator.Accumulation[Perspective][IndexOutput]) {
                printf("-- Accumulator error! Color = %d Piece = %d From = %d To = %d Move type = %d\n", CHANGE_COLOR(Board->CurrentColor), Info->PieceFrom, Info->From, Info->To, Info->Type);

                break; // for (256)
            }
        }
    }
#endif // DEBUG_NNUE

    Board->Accumulator.AccumulationComputed = TRUE;

    return TRUE;
}

#endif // USE_NNUE_UPDATE

void Transform(BoardItem* Board, I8* Output, U32* OutputMask)
{
    I16(*Accumulation)[2][HALF_FEATURE_OUTPUT_DIMENSION] = &Board->Accumulator.Accumulation;

    if (!Board->Accumulator.AccumulationComputed) {
#ifdef USE_NNUE_UPDATE
        if (!UpdateAccumulator(Board)) {
#endif // USE_NNUE_UPDATE
            RefreshAccumulator(Board);
#ifdef USE_NNUE_UPDATE
        }
#endif // USE_NNUE_UPDATE
    }

#ifdef USE_NNUE_AVX2
    const __m256i ConstZero = _mm256_setzero_si256();

    // Active

    __m256i* Out_1 = (__m256i*)&Output[0]; // Offset 0

    for (int Chunk = 0; Chunk < NUM_REGS / 2; ++Chunk) { // 16 / 2 = 8
        __m256i Sum_0 = ((__m256i*)(*Accumulation)[Board->CurrentColor])[Chunk * 2];
        __m256i Sum_1 = ((__m256i*)(*Accumulation)[Board->CurrentColor])[Chunk * 2 + 1];

        Out_1[Chunk] = _mm256_packs_epi16(Sum_0, Sum_1);

        *OutputMask++ = _mm256_movemask_epi8(_mm256_cmpgt_epi8(Out_1[Chunk], ConstZero));
    }

    // Non active

    __m256i* Out_2 = (__m256i*)&Output[HALF_FEATURE_OUTPUT_DIMENSION]; // Offset 256

    for (int Chunk = 0; Chunk < NUM_REGS / 2; ++Chunk) { // 16 / 2 = 8
        __m256i Sum_0 = ((__m256i*)(*Accumulation)[CHANGE_COLOR(Board->CurrentColor)])[Chunk * 2];
        __m256i Sum_1 = ((__m256i*)(*Accumulation)[CHANGE_COLOR(Board->CurrentColor)])[Chunk * 2 + 1];

        Out_2[Chunk] = _mm256_packs_epi16(Sum_0, Sum_1);

        *OutputMask++ = _mm256_movemask_epi8(_mm256_cmpgt_epi8(Out_2[Chunk], ConstZero));
    }
#else
    // Active

    for (int IndexOutput = 0; IndexOutput < HALF_FEATURE_OUTPUT_DIMENSION; ++IndexOutput) { // 256
        I16 Sum = (*Accumulation)[Board->CurrentColor][IndexOutput];

        Output[IndexOutput] = (I8)MAX(0, MIN(127, Sum)); // Offset 0
    }

    // Non active

    for (int IndexOutput = 0; IndexOutput < HALF_FEATURE_OUTPUT_DIMENSION; ++IndexOutput) { // 256
        I16 Sum = (*Accumulation)[CHANGE_COLOR(Board->CurrentColor)][IndexOutput];

        Output[HALF_FEATURE_OUTPUT_DIMENSION + IndexOutput] = (I8)MAX(0, MIN(127, Sum)); // Offset 256
    }
#endif // USE_NNUE_AVX2
}

#ifdef USE_NNUE_AVX2
BOOL NextIndex(int* Index, int* Offset, U64* Value, const U32* Mask, const int Dims)
{
    while (*Value == 0) {
        *Offset += 8 * sizeof(U64);

        if (*Offset >= Dims) {
            return FALSE;
        }

        memcpy(Value, (char*)Mask + *Offset / 8, sizeof(U64));
    }

    *Index = *Offset + LSB(*Value);

    *Value &= *Value - 1;

    return TRUE;
}
#endif // USE_NNUE_AVX2

void HiddenLayer(const I8* Input, void* Output, const int InputDims, const int OutputDims, const I32* Biases, const I8* Weights, const U32* InputMask, U32* OutputMask, const BOOL CalcMask)
{
#ifdef USE_NNUE_AVX2
    const __m256i ConstZero = _mm256_setzero_si256();

    __m256i Out_0 = ((__m256i*)Biases)[0];
    __m256i Out_1 = ((__m256i*)Biases)[1];
    __m256i Out_2 = ((__m256i*)Biases)[2];
    __m256i Out_3 = ((__m256i*)Biases)[3];

    __m256i First;
    __m256i Second;

    U64 Value;

    int Index;

    memcpy(&Value, InputMask, sizeof(U64));

    for (int Offset = 0; Offset < InputDims;) { // 512/32
        if (!NextIndex(&Index, &Offset, &Value, InputMask, InputDims)) {
            break;
        }

        First = ((__m256i*)Weights)[Index];

        U16 Factor = Input[Index];

        if (NextIndex(&Index, &Offset, &Value, InputMask, InputDims)) {
            Second = ((__m256i*)Weights)[Index];

            Factor |= Input[Index] << 8;
        }
        else {
            Second = ConstZero;
        }

        __m256i Mul = _mm256_set1_epi16(Factor);

        __m256i Product;
        __m256i Signs;

        Product = _mm256_maddubs_epi16(Mul, _mm256_unpacklo_epi8(First, Second));

        Signs = _mm256_cmpgt_epi16(ConstZero, Product);

        Out_0 = _mm256_add_epi32(Out_0, _mm256_unpacklo_epi16(Product, Signs));
        Out_1 = _mm256_add_epi32(Out_1, _mm256_unpackhi_epi16(Product, Signs));

        Product = _mm256_maddubs_epi16(Mul, _mm256_unpackhi_epi8(First, Second));

        Signs = _mm256_cmpgt_epi16(ConstZero, Product);

        Out_2 = _mm256_add_epi32(Out_2, _mm256_unpacklo_epi16(Product, Signs));
        Out_3 = _mm256_add_epi32(Out_3, _mm256_unpackhi_epi16(Product, Signs));
    }

    __m256i Out_16_0 = _mm256_srai_epi16(_mm256_packs_epi32(Out_0, Out_1), SHIFT);
    __m256i Out_16_1 = _mm256_srai_epi16(_mm256_packs_epi32(Out_2, Out_3), SHIFT);

    __m256i* OutputVector = (__m256i*)Output;

    OutputVector[0] = _mm256_packs_epi16(Out_16_0, Out_16_1);

    if (CalcMask) {
        OutputMask[0] = _mm256_movemask_epi8(_mm256_cmpgt_epi8(OutputVector[0], ConstZero));
    }
    else {
        OutputVector[0] = _mm256_max_epi8(OutputVector[0], ConstZero);
    }
#else
    I32 Temp[32]; // OutputDims

    for (int IndexOutput = 0; IndexOutput < OutputDims; ++IndexOutput) { // 32/32
        Temp[IndexOutput] = Biases[IndexOutput];
    }

    for (int IndexInput = 0; IndexInput < InputDims; ++IndexInput) { // 512/32
        if (Input[IndexInput]) {
            for (int IndexOutput = 0; IndexOutput < OutputDims; ++IndexOutput) { // 32/32
                Temp[IndexOutput] += Input[IndexInput] * Weights[IndexInput * OutputDims + IndexOutput];
            }
        }
    }

    I8* OutputVector = (I8*)Output;

    for (int IndexOutput = 0; IndexOutput < OutputDims; ++IndexOutput) { // 32/32
        OutputVector[IndexOutput] = (I8)MAX(0, MIN(127, Temp[IndexOutput] >> SHIFT));
    }
#endif // USE_NNUE_AVX2
}

I32 OutputLayer(const I8* Input, const I32* Biases, const I8* Weights)
{
#ifdef USE_NNUE_AVX2
    __m256i* InputVector = (__m256i*)Input;
    __m256i* WeightsVector = (__m256i*)Weights;

    __m256i Product = _mm256_maddubs_epi16(InputVector[0], WeightsVector[0]);
    Product = _mm256_madd_epi16(Product, _mm256_set1_epi16(1));

    __m128i Sum = _mm_add_epi32(_mm256_castsi256_si128(Product), _mm256_extracti128_si256(Product, 1));
    Sum = _mm_add_epi32(Sum, _mm_shuffle_epi32(Sum, 0x1B));

    return _mm_cvtsi128_si32(Sum) + _mm_extract_epi32(Sum, 1) + Biases[0];
#else
    I32 Sum = Biases[0];

    for (int IndexInput = 0; IndexInput < HIDDEN_2_DIMENSION; ++IndexInput) { // 32
        Sum += Input[IndexInput] * Weights[IndexInput];
    }

    return Sum;
#endif // USE_NNUE_AVX2
}

int NetworkEvaluate(BoardItem* Board)
{
    _declspec(align(64)) I8 Input[INPUT_DIMENSION]; // 512 (256 x 2)

    /*
        The mask is 32 bits (see _mm256_movemask_epi8)
        The array of masks must be a multiple of 64 bits (see NextIndex)
    */
    _declspec(align(8)) U32 Hidden_1_Mask[INPUT_DIMENSION / (8 * sizeof(U32))]; // 512 / 32 = 16
    _declspec(align(8)) U32 Hidden_2_Mask[64 / (8 * sizeof(U32))] = { 0, 0 }; // 64 / 32 = 2

    I8 Hidden_1_Output[HIDDEN_1_DIMENSION]; // 32
    I8 Hidden_2_Output[HIDDEN_2_DIMENSION]; // 32

    I32 OutputValue;

    // Transform: Board -> (256 x 2)

    Transform(Board, Input, Hidden_1_Mask);

    // Hidden 1: (256 x 2) -> 32

    HiddenLayer(Input, Hidden_1_Output, 512, 32, Hidden_1_Biases, Hidden_1_Weights, Hidden_1_Mask, Hidden_2_Mask, TRUE);

    // Hidden 2: 32 -> 32

    HiddenLayer(Hidden_1_Output, Hidden_2_Output, 32, 32, Hidden_2_Biases, Hidden_2_Weights, Hidden_2_Mask, NULL, FALSE);

    // Output: 32 -> 1

    OutputValue = OutputLayer(Hidden_2_Output, OutputBiases, OutputWeights);

    return OutputValue / SCALE;
}

#endif // NNUE_EVALUATION_FUNCTION