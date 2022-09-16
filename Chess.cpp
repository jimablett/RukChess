// Chess.cpp

#include "stdafx.h"

#include "BitBoard.h"
#include "Board.h"
#include "Book.h"
#include "Def.h"
#include "Evaluate.h"
#include "Game.h"
#include "Gen.h"
#include "Hash.h"
#include "NNUE.h"
#include "NNUE2.h"
#include "Tests.h"
#include "Tuning.h"
#include "UCI.h"
#include "Utils.h"

int main(int, char**)
{
    char Buf[10];

    int Choice;

    printf("%s %s\n", NAME, VERSION);
    printf("Copyright (C) %s %s\n", YEARS, AUTHOR);

//  printf("MoveItem = %zd\n", sizeof(MoveItem));
//  printf("AccumulatorItem = %zd\n", sizeof(AccumulatorItem));
//  printf("HistoryItem = %zd\n", sizeof(HistoryItem));
//  printf("BoardItem = %zd\n", sizeof(BoardItem));

    omp_set_dynamic(0); // Disable dynamic thread control

    MaxThreads = omp_get_max_threads(); // Save hardware max. threads

    if (MaxThreads > MAX_THREADS) {
        MaxThreads = MAX_THREADS;
    }

    omp_set_num_threads(DEFAULT_THREADS);

    printf("\n");

    printf("Max. threads = %d\n", MaxThreads);
    printf("Threads = %d\n", DEFAULT_THREADS);

#ifdef BIND_THREAD
    InitThreadNode();
#endif // BIND_THREAD

    InitBitBoards();

    InitHashTable(DEFAULT_HASH_TABLE_SIZE);
    ClearHash();

    printf("\n");

    printf("Max. hash table size = %d Mb\n", MAX_HASH_TABLE_SIZE);
    printf("Hash table size = %d Mb\n", DEFAULT_HASH_TABLE_SIZE);

    InitHashBoards();

#if defined(SIMPLIFIED_EVALUATION_FUNCTION) || defined(TOGA_EVALUATION_FUNCTION)
    InitEvaluation();
#elif defined(NNUE_EVALUATION_FUNCTION) || defined(NNUE_EVALUATION_FUNCTION_2)
    ReadNetwork();
#endif // SIMPLIFIED_EVALUATION_FUNCTION || TOGA_EVALUATION_FUNCTION || NNUE_EVALUATION_FUNCTION || NNUE_EVALUATION_FUNCTION_2

#ifdef LATE_MOVE_PRUNING
    for (int Depth = 0; Depth < (6 + 1); ++Depth) {
        LateMovePruningTable[Depth] = (int)round(2.98484 + pow(Depth, 1.74716)); // Hakkapeliitta

//      printf("LateMovePruningTable[%d] = %d\n", Depth, LateMovePruningTable[Depth]);
    }
#endif // LATE_MOVE_PRUNING

#if defined(NEGA_SCOUT) && defined(LATE_MOVE_REDUCTION)
    for (int Depth = 0; Depth < 64; ++Depth) {
//      printf("LateMoveReductionTable[%2d] = ", Depth);

        for (int MoveNumber = 0; MoveNumber < 64; ++MoveNumber) {
            LateMoveReductionTable[Depth][MoveNumber] = MAX((int)(log(Depth + 1) * log(MoveNumber + 1) / 1.70), 1); // Hakkapeliitta

//          printf("%d", LateMoveReductionTable[Depth][MoveNumber]);
        }

//      printf("\n");
    }
#endif // NEGA_SCOUT && LATE_MOVE_REDUCTION

#if defined(TUNING) && defined(TOGA_EVALUATION_FUNCTION)
    InitTuningParams();
    LoadTuningParams();
#endif // TUNING && TOGA_EVALUATION_FUNCTION

    UseBook = LoadBook();

    printf("\n");

    printf("Use UCI commands or press Enter to display the menu\n");

    fgets(Buf, sizeof(Buf), stdin);

    if (!strncmp(Buf, "uci", 3)) {
        PrintMode = PRINT_MODE_UCI;

        UCI();

        return 0;
    }

    PrintMode = PRINT_MODE_NORMAL;

    while (TRUE) {
        printf("Menu:\n");

        printf("\n");

        printf(" 1: New white game\n");
        printf(" 2: New black game\n");
        printf(" 3: New auto game\n");

        printf(" 4: Load game from file (chess.fen) and white game\n");
        printf(" 5: Load game from file (chess.fen) and black game\n");
        printf(" 6: Load game from file (chess.fen) and auto game\n");

        printf(" 7: Built-in drive generator test\n");
        printf(" 8: Load game from file (chess.fen) and drive generator test\n");

        printf(" 9: Built-in performance test\n");

        printf("10: Bratko-Kopec test (24 positions)\n");
        printf("11: Win-At-Chess test (300 positions)\n");

        printf("12: Generate book file (book.txt) from PGN file (book.pgn)\n");
        printf("13: Convert PGN file (games.pgn) to FEN file (games.fen)\n");

#if defined(TUNING) && defined(TOGA_EVALUATION_FUNCTION)

        printf("14: Find best K (required games.fen)\n");

        printf("15: Tuning evaluation function - Local search (required games.fen)\n");
        printf("16: Tuning evaluation function - Adam SGD (required games.fen)\n");

        printf("17: Load tuning params (params.txt)\n");
        printf("18: Save tuning params (params.txt)\n");

        printf("19: Exit\n");

#else

        printf("14: Exit\n");

#endif // TUNING && TOGA_EVALUATION_FUNCTION

        printf("\n");

        printf("Choice: ");
        scanf_s("%d", &Choice);

        switch (Choice) {
        case 1:
            SetFen(&CurrentBoard, StartFen);
            Game(WHITE, BLACK);
            break;

        case 2:
            SetFen(&CurrentBoard, StartFen);
            Game(BLACK, WHITE);
            break;

        case 3:
            SetFen(&CurrentBoard, StartFen);
            GameAuto();
            break;

        case 4:
            LoadGame(&CurrentBoard);
            Game(WHITE, BLACK);
            break;

        case 5:
            LoadGame(&CurrentBoard);
            Game(BLACK, WHITE);
            break;

        case 6:
            LoadGame(&CurrentBoard);
            GameAuto();
            break;

        case 7:
            GeneratorTest1();
            break;

        case 8:
            LoadGame(&CurrentBoard);
            GeneratorTest2();
            break;

        case 9:
            PerformanceTest();
            break;

        case 10:
            BratkoKopecTest();
            break;

        case 11:
            WinAtChessTest();
            break;

        case 12:
            GenerateBook();
            break;

        case 13:
            Pgn2Fen();
            break;

#if defined(TUNING) && defined(TOGA_EVALUATION_FUNCTION)

        case 14:
            FindBestK();
            break;

        case 15:
            TuningLocalSearch();
            break;

        case 16:
            TuningAdamSGD();
            break;

        case 17:
            LoadTuningParams();
            break;

        case 18:
            SaveTuningParams();
            break;

        case 19: // Exit
            return 0;

#else

        case 14: // Exit
            return 0;

#endif // TUNING && TOGA_EVALUATION_FUNCTION
        } // switch

        printf("\n");
    } // while

    return 0;
}