// Tuning.cpp

#include "stdafx.h"

#include "Tuning.h"

#include "Board.h"
#include "Def.h"
#include "Evaluate.h"
#include "Game.h"
#include "Gen.h"
#include "Hash.h"
#include "Heuristic.h"
#include "Move.h"
#include "SEE.h"
#include "Sort.h"
#include "Types.h"
#include "Utils.h"

#define STAGE_NONE						1
#define STAGE_TAG						2
#define STAGE_NOTATION					3
#define STAGE_MOVE						4
#define STAGE_COMMENT					5

#define MIN_USE_PLY						0 // 0 moves

#if defined(TUNING) && defined(TOGA_EVALUATION_FUNCTION)

#define MAX_TUNING_PARAMS				1000 // 742 used

// Adam SGD

//#define TUNING_BATCH_SIZE_FENS		100000	// FENs
#define TUNING_BATCH_SIZE_PERCENT		10		// %

typedef struct {
	char Fen[MAX_FEN_LENGTH];

	double Result; // 1.0/0.5/0.0

	double Error;
} PositionItem; // 272 bytes

struct {
	int Count;

	PositionItem** Positions;
} PositionStore;

struct {
	int Count;

	SCORE* Params[MAX_TUNING_PARAMS];

	double Gradients[MAX_TUNING_PARAMS];
} TuningParamStore;

//double K = 1.0;			// Default
//double K = 1.14584914;	// 3334962 FENs (07.09.2021)
double K = 1.14585230;		// 3334962 FENs (29.11.2021)

SCORE TuningSearch(BoardItem* Board, SCORE Alpha, SCORE Beta, const int Ply, const BOOL InCheck)
{
	int GenMoveCount;
	MoveItem MoveList[MAX_GEN_MOVES];

	int LegalMoveCount = 0;

	SCORE Score;
	SCORE BestScore;

	BOOL GiveCheck;

	#ifdef QUIESCENCE_MATE_DISTANCE_PRUNING
	Alpha = MAX(Alpha, -INF + Ply);
	Beta = MIN(Beta, INF - Ply - 1);

	if (Alpha >= Beta) {
		return Alpha;
	}
	#endif // QUIESCENCE_MATE_DISTANCE_PRUNING

	if (IsInsufficientMaterial(Board)) {
		return 0;
	}

	if (Board->FiftyMove >= 100) {
		if (InCheck) {
			GenMoveCount = 0;
			GenerateAllMoves(Board, MoveList, &GenMoveCount);

			for (int MoveNumber = 0; MoveNumber < GenMoveCount; ++MoveNumber) {
				MakeMove(Board, MoveList[MoveNumber]);

				if (IsInCheck(Board, CHANGE_COLOR(Board->CurrentColor))) {
					UnmakeMove(Board);

					continue; // Next move
				}

				// Legal move

				UnmakeMove(Board);

				return 0;
			}

			// No legal move

			return -INF + Ply;
		}

		return 0;
	}

	if (PositionRepeat1(Board)) {
		return 0;
	}

	if (Ply >= MAX_PLY) {
		return Evaluate(Board);
	}

	#ifdef QUIESCENCE_CHECK_EXTENSION
	if (InCheck) {
		BestScore = -INF + Ply;

		GenMoveCount = 0;
		GenerateAllMoves(Board, MoveList, &GenMoveCount);
	}
	else {
	#endif // QUIESCENCE_CHECK_EXTENSION
		BestScore = Evaluate(Board);

		if (BestScore >= Beta) {
			return BestScore;
		}

		if (BestScore > Alpha) {
			Alpha = BestScore;
		}

		GenMoveCount = 0;
		GenerateCaptureMoves(Board, MoveList, &GenMoveCount);
	#ifdef QUIESCENCE_CHECK_EXTENSION
	}
	#endif // QUIESCENCE_CHECK_EXTENSION

	for (int MoveNumber = 0; MoveNumber < GenMoveCount; ++MoveNumber) {
		#if defined(MOVES_SORT_SEE) || defined(MOVES_SORT_MVV_LVA) || defined(MOVES_SORT_HEURISTIC) || defined(MOVES_SORT_SQUARE_SCORE)
		PrepareNextMove(MoveNumber, MoveList, GenMoveCount);
		#endif // MOVES_SORT_SEE || MOVES_SORT_MVV_LVA || MOVES_SORT_HEURISTIC || MOVES_SORT_SQUARE_SCORE

		#ifdef QUIESCENCE_SEE_MOVE_PRUNING
		if (!InCheck) {
			#ifdef MOVES_SORT_SEE
			if (
				(MoveList[MoveNumber].Type & MOVE_CAPTURE)
				&& !(MoveList[MoveNumber].Type & MOVE_PAWN_PROMOTE)
			) {
				if (MoveList[MoveNumber].SortValue < -SORT_CAPTURE_MOVE_BONUS) { // Bad capture move
					break;
				}
			}
			else {
				if (CaptureSEE(Board, MOVE_FROM(MoveList[MoveNumber].Move), MOVE_TO(MoveList[MoveNumber].Move), MOVE_PROMOTE_PIECE(MoveList[MoveNumber].Move), MoveList[MoveNumber].Type) < 0) { // Bad quiet move
					continue; // Next move
				}
			}
			#elif defined(MOVES_SORT_MVV_LVA)
			if (CaptureSEE(Board, MOVE_FROM(MoveList[MoveNumber].Move), MOVE_TO(MoveList[MoveNumber].Move), MOVE_PROMOTE_PIECE(MoveList[MoveNumber].Move), MoveList[MoveNumber].Type) < 0) { // Bad capture/quiet move
				continue; // Next move
			}
			#endif // MOVES_SORT_SEE/MOVES_SORT_MVV_LVA
		}
		#endif // QUIESCENCE_SEE_MOVE_PRUNING

		MakeMove(Board, MoveList[MoveNumber]);

		if (IsInCheck(Board, CHANGE_COLOR(Board->CurrentColor))) {
			UnmakeMove(Board);

			continue; // Next move
		}

		++LegalMoveCount;

		GiveCheck = IsInCheck(Board, Board->CurrentColor);

		Score = -TuningSearch(Board, -Beta, -Alpha, Ply + 1, GiveCheck);

		UnmakeMove(Board);

		if (Score > BestScore) {
			BestScore = Score;

			if (BestScore > Alpha) {
				if (BestScore < Beta) {
					Alpha = BestScore;
				}
				else { // BestScore >= Beta
					return BestScore;
				}
			}
		}
	} // for

	#ifdef QUIESCENCE_CHECK_EXTENSION
	if (InCheck && LegalMoveCount == 0) { // Checkmate
		return -INF + Ply;
	}
	#endif // QUIESCENCE_CHECK_EXTENSION

	return BestScore;
}

#endif // TUNING && TOGA_EVALUATION_FUNCTION

void Pgn2Fen(void)
{
	FILE* FileIn;
	FILE* FileOut;

	int GameNumber;

	double Result;

	int Ply;

	BOOL Error;

	char MoveString[16];
	char* Move;

	int Stage;

	char Buf[4096];
	char* Part;

	char FenString[MAX_FEN_LENGTH];
	char* Fen;

	char FenOut[MAX_FEN_LENGTH];

	int GenMoveCount;
	MoveItem MoveList[MAX_GEN_MOVES];

	BOOL MoveFound;

	char NotateMoveStr[16];

	printf("\n");

	printf("Read PGN file...\n");

	fopen_s(&FileIn, "games.pgn", "r");

	if (FileIn == NULL) { // File open error
		printf("File 'games.pgn' open error!\n");

		Sleep(3000);

		exit(0);
	}

	fopen_s(&FileOut, "games.fen", "w");

	if (FileOut == NULL) { // File create (open) error
		printf("File 'games.fen' create (open) error!\n");

		Sleep(3000);

		exit(0);
	}

	// No cache used
	InitHashTable(1);
	ClearHash();

	// Only the main thread is used
	omp_set_num_threads(1);

	// Prepare new game

	GameNumber = 1;

	Result = 0.0; // Draw default

	Ply = 0;

	Error = FALSE;

	Move = MoveString;
	*Move = '\0'; // Nul

	SetFen(&CurrentBoard, StartFen);

	Stage = STAGE_NONE;

	printf("\n");

	while (fgets(Buf, sizeof(Buf), FileIn) != NULL) {
		Part = Buf;

		if (*Part == '\r' || *Part == '\n') { // Empty string
			if (Stage == STAGE_NOTATION) {
				// Prepare new game

				++GameNumber;

				Result = 0.0; // Draw default

				Ply = 0;

				Error = FALSE;

				Move = MoveString;
				*Move = '\0'; // Nul

				SetFen(&CurrentBoard, StartFen);

				Stage = STAGE_NONE;
			}

			continue; // Next string
		}

		if (*Part == '[') { // Tag
			if (Stage == STAGE_NONE) {
				if (GameNumber > 1 && ((GameNumber - 1) % 1000) == 0) {
					printf("Game number = %d\n", GameNumber - 1);
				}

				Stage = STAGE_TAG;
			}

			if (!strncmp(Part, "[Result \"1-0\"]", 14)) { // Result 1-0
				Result = 1.0;

//				printf("Result = %.1f\n", Result);
			}
			else if (!strncmp(Part, "[Result \"1/2-1/2\"]", 18)) { // Result 1/2-1/2
				Result = 0.5;

//				printf("Result = %.1f\n", Result);
			}
			else if (!strncmp(Part, "[Result \"0-1\"]", 14)) { // Result 0-1
				Result = 0.0;

//				printf("Result = %.1f\n", Result);
			}
			else if (!strncmp(Part, "[FEN \"", 6)) { // FEN
				Part += 6;

				Fen = FenString;

				while (*Part != '"') {
					*Fen++ = *Part++; // Copy FEN
				}

				*Fen = '\0'; // Nul

//				printf("FEN = %s\n", FenString);

				SetFen(&CurrentBoard, FenString);
			}

			continue; // Next string
		} // if

		if (Stage == STAGE_TAG) {
			Stage = STAGE_NOTATION;

			if (Ply >= MIN_USE_PLY) {
				GetFen(&CurrentBoard, FenOut); // First FEN in game (StartFen or FenString from FEN-tag)

				fprintf(FileOut, "%s|%.1f\n", FenOut, Result);
			}

			++Ply;
		}

		while (*Part != '\0') { // Scan string
			if (*Part == '{') { // Comment (open)
				Stage = STAGE_COMMENT;
			}
			else if (*Part == '}') { // Comment (close)
				Stage = STAGE_NOTATION;
			}

			if (Stage == STAGE_NOTATION) {
				if (strchr(MoveFirstChar, *Part) != NULL) {
					Stage = STAGE_MOVE;

					Move = MoveString;

					*Move++ = *Part; // Copy move (first char)
				}
			}
			else if (Stage == STAGE_MOVE) {
				if (strchr(MoveSubsequentChar, *Part) != NULL) {
					*Move++ = *Part; // Copy move (subsequent char)
				}
				else { // End of move
					Stage = STAGE_NOTATION;

					*Move = '\0'; // Nul

					if (Error) {
						++Part;

						continue; // Next string
					}

//					printf("Move = %s\n", MoveString);

					GenMoveCount = 0;
					GenerateAllMoves(&CurrentBoard, MoveList, &GenMoveCount);

					MoveFound = FALSE;

					for (int MoveNumber = 0; MoveNumber < GenMoveCount; ++MoveNumber) {
						NotateMove(&CurrentBoard, MoveList[MoveNumber], NotateMoveStr);

						if (strcmp(MoveString, NotateMoveStr) == 0) {
							MakeMove(&CurrentBoard, MoveList[MoveNumber]);

							if (IsInCheck(&CurrentBoard, CHANGE_COLOR(CurrentBoard.CurrentColor))) {
								UnmakeMove(&CurrentBoard);

								PrintBoard(&CurrentBoard);

								printf("\n");

								printf("Move = %s check\n", MoveString);

								printf("\n");

								printf("Gen. move = %s%s", BoardName[MOVE_FROM(MoveList[MoveNumber].Move)], BoardName[MOVE_TO(MoveList[MoveNumber].Move)]);

								if (MoveList[MoveNumber].Type & MOVE_PAWN_PROMOTE) {
									printf("%c", PiecesCharBlack[MOVE_PROMOTE_PIECE(MoveList[MoveNumber].Move)]);
								}

								printf(" (%s)\n\n", NotateMoveStr);

								continue; // Next move
							}

							MoveFound = TRUE;

							break; // for
						}
					}

					if (MoveFound) {
						if (Ply >= MIN_USE_PLY) {
							GetFen(&CurrentBoard, FenOut);

							fprintf(FileOut, "%s|%.1f\n", FenOut, Result);
						}
					}
					else { // No move found
						Error = TRUE;

						PrintBoard(&CurrentBoard);

						printf("\n");

						printf("Move = %s not found\n", MoveString);

						printf("\n");

						printf("Gen. moves =");

						for (int MoveNumber = 0; MoveNumber < GenMoveCount; ++MoveNumber) {
							NotateMove(&CurrentBoard, MoveList[MoveNumber], NotateMoveStr);

							printf(" %s%s", BoardName[MOVE_FROM(MoveList[MoveNumber].Move)], BoardName[MOVE_TO(MoveList[MoveNumber].Move)]);

							if (MoveList[MoveNumber].Type & MOVE_PAWN_PROMOTE) {
								printf("%c", PiecesCharBlack[MOVE_PROMOTE_PIECE(MoveList[MoveNumber].Move)]);
							}

							printf(" (%s)", NotateMoveStr);
						}

						printf("\n\n");
					}

					++Ply;
				}
			}

			++Part;
		} // while
	} // while

	printf("Game number = %d\n", GameNumber - 1);

	fclose(FileOut);
	fclose(FileIn);

	printf("\n");

	printf("Read PGN file...DONE\n");
}

#if defined(TUNING) && defined(TOGA_EVALUATION_FUNCTION)

void ReadFenFile(void)
{
	FILE* File;

	char Buf[512];
	char* Part;

	int PositionNumber;

	char FenString[MAX_FEN_LENGTH];
	char* Fen;

	double Result;

	PositionItem** PositionPointer;
	PositionItem* PositionItemPointer;

	printf("\n");

	printf("Read FEN file...\n");

	fopen_s(&File, "games.fen", "r");

	if (File == NULL) { // File open error
		printf("File 'games.fen' open error!\n");

		Sleep(3000);

		exit(0);
	}

	// The first cycle to get the number of positions

	PositionStore.Count = 0;

	while (fgets(Buf, sizeof(Buf), File) != NULL) {
		++PositionStore.Count;
	}

	// Allocate memory to store pointers to positions

	PositionStore.Positions = (PositionItem**)malloc(PositionStore.Count * sizeof(PositionItem*));

	if (PositionStore.Positions == NULL) { // Allocate memory error
		printf("Allocate memory to store pointers to positions error!\n");

		Sleep(3000);

		exit(0);
	}

	// Set the pointer to the beginning of the file

	fseek(File, 0, SEEK_SET);

	// The second cycle for reading positions

	PositionNumber = 0;

	while (fgets(Buf, sizeof(Buf), File) != NULL) {
		Part = Buf;

		Fen = FenString;

		while (*Part != '|') {
			*Fen++ = *Part++; // Copy FEN
		}

		*Fen = '\0'; // Nul

		++Part; // |

		Result = atof(Part);

		// Allocate memory to store position

		PositionItemPointer = (PositionItem*)malloc(sizeof(PositionItem));

		if (PositionItemPointer == NULL) { // Allocates memory error
			printf("Allocate memory to store position error!\n");

			Sleep(3000);

			exit(0);
		}

		PositionPointer = &PositionStore.Positions[PositionNumber];

		*PositionPointer = PositionItemPointer;

		strcpy_s(PositionItemPointer->Fen, MAX_FEN_LENGTH, FenString);

		PositionItemPointer->Result = Result;

		PositionItemPointer->Error = 0.0;

		++PositionNumber;
	} // while

	fclose(File);

	printf("Read FEN file...DONE (%d)\n", PositionStore.Count);
}

void PositionStoreFree(void)
{
	PositionItem* PositionItemPointer;

	for (int PositionNumber = 0; PositionNumber < PositionStore.Count; ++PositionNumber) {
		PositionItemPointer = PositionStore.Positions[PositionNumber];

		free(PositionItemPointer);
	}

	free(PositionStore.Positions);
}

double Sigmoid(SCORE Score)
{
	return 1.0 / (1.0 + pow(10.0, -K * (double)Score / 400.0));
}

double CalculateError(const int BatchSize)
{
	PositionItem* PositionItemPointer;

	BoardItem ThreadBoard;

	BOOL InCheck;

	SCORE Score;

	double Result = 0.0;

	double C = 0.0;
	double Y;
	double T;

//	printf("\n");

	#pragma omp parallel for private(PositionItemPointer, ThreadBoard, InCheck, Score)
	for (int PositionNumber = 0; PositionNumber < BatchSize; ++PositionNumber) {
		PositionItemPointer = PositionStore.Positions[PositionNumber];

		ThreadBoard = CurrentBoard;

		SetFen(&ThreadBoard, PositionItemPointer->Fen);

		InCheck = IsInCheck(&ThreadBoard, ThreadBoard.CurrentColor);

		Score = TuningSearch(&ThreadBoard, -INF, INF, 0, InCheck);

		if (ThreadBoard.CurrentColor == BLACK) {
			Score = -Score;
		}

//		printf("Position number = %d FEN = %s Result = %.1f Score = %.2f\n", PositionNumber + 1, PositionItemPointer->Fen, PositionItemPointer->Result, Score);

		PositionItemPointer->Error = pow((PositionItemPointer->Result - Sigmoid(Score)), 2.0);
	}

	// Kahan summation algorithm (https://en.wikipedia.org/wiki/Kahan_summation_algorithm)

	for (int PositionNumber = 0; PositionNumber < PositionStore.Count; ++PositionNumber) {
		PositionItemPointer = PositionStore.Positions[PositionNumber];

		Y = PositionItemPointer->Error - C;
		T = Result + Y;
		C = (T - Result) - Y;

		Result = T;
	}

	Result /= PositionStore.Count;

	return Result;
}

void FindBestK(void)
{
	int InputThreads;

	double Min = -10.0;
	double Max = 10.0;

	double Delta = 1.0;

	double BestError = DBL_MAX;
	double Error;

	double BestK = 1.0;

	// No cache used
	InitHashTable(1);
	ClearHash();

	// Input max. threads

	printf("\n");

	printf("Threads (min. 1 max. %d; 0 = %d): ", MaxThreads, DEFAULT_THREADS);
	scanf_s("%d", &InputThreads);

	InputThreads = (InputThreads >= 1 && InputThreads <= MaxThreads) ? InputThreads : DEFAULT_THREADS;

	omp_set_num_threads(InputThreads);

	// Prepare board

	SetFen(&CurrentBoard, StartFen);

	#ifdef MOVES_SORT_HEURISTIC
	ClearHeuristic(&CurrentBoard);
	#endif // MOVES_SORT_HEURISTIC

	ReadFenFile();

	printf("\n");

	printf("Find best K...\n");

//	printf("\n");

	for (int Precision = 0; Precision < 9; ++Precision) {
//		printf("Min = %.8f; Max = %.8f; Delta = %.8f\n", Min, Max, Delta);

		while (Min < Max) {
			K = Min;

			Error = CalculateError(PositionStore.Count);

			if (Error < BestError) {
				BestError = Error;

				BestK = K;

//				printf("\n");

//				printf("Best K = %.8f\n", BestK);
			}

			Min += Delta;
		}

		Min = BestK - Delta;
		Max = BestK + Delta;

		Delta /= 10.0;
	}

	K = BestK;

	printf("\n");

	printf("Best K = %.8f\n", K);

	printf("\n");

	printf("Find best K...DONE\n");

	PositionStoreFree();
}

void InitTuningParams(void)
{
	TuningParamStore.Count = 0;

	// Game phase (4.1)

	// Material (4.2)

	for (int Piece = 0; Piece < 5; ++Piece) { // PNBRQ
		TuningParamStore.Params[TuningParamStore.Count++] = &PiecesScoreOpening[Piece];
	}

	for (int Piece = 0; Piece < 5; ++Piece) { // PNBRQ
		TuningParamStore.Params[TuningParamStore.Count++] = &PiecesScoreEnding[Piece];
	}

	TuningParamStore.Params[TuningParamStore.Count++] = &BishopPairOpening;
	TuningParamStore.Params[TuningParamStore.Count++] = &BishopPairEnding;

	// Piece-square tables (4.3)

	for (int Square = 8; Square < 56; ++Square) {
		TuningParamStore.Params[TuningParamStore.Count++] = &PawnSquareScoreOpening[Square];
	}
/*
	for (int Square = 8; Square < 56; ++Square) {
		TuningParamStore.Params[TuningParamStore.Count++] = &PawnSquareScoreEnding[Square];
	}
*/
	for (int Square = 0; Square < 64; ++Square) {
		TuningParamStore.Params[TuningParamStore.Count++] = &KnightSquareScoreOpening[Square];
	}

	for (int Square = 0; Square < 64; ++Square) {
		TuningParamStore.Params[TuningParamStore.Count++] = &KnightSquareScoreEnding[Square];
	}

	for (int Square = 0; Square < 64; ++Square) {
		TuningParamStore.Params[TuningParamStore.Count++] = &BishopSquareScoreOpening[Square];
	}

	for (int Square = 0; Square < 64; ++Square) {
		TuningParamStore.Params[TuningParamStore.Count++] = &BishopSquareScoreEnding[Square];
	}

	for (int Square = 0; Square < 64; ++Square) {
		TuningParamStore.Params[TuningParamStore.Count++] = &RookSquareScoreOpening[Square];
	}
/*
	for (int Square = 0; Square < 64; ++Square) {
		TuningParamStore.Params[TuningParamStore.Count++] = &RookSquareScoreEnding[Square];
	}
*/
	for (int Square = 0; Square < 64; ++Square) {
		TuningParamStore.Params[TuningParamStore.Count++] = &QueenSquareScoreOpening[Square];
	}

	for (int Square = 0; Square < 64; ++Square) {
		TuningParamStore.Params[TuningParamStore.Count++] = &QueenSquareScoreEnding[Square];
	}

	for (int Square = 0; Square < 64; ++Square) {
		TuningParamStore.Params[TuningParamStore.Count++] = &KingSquareScoreOpening[Square];
	}

	for (int Square = 0; Square < 64; ++Square) {
		TuningParamStore.Params[TuningParamStore.Count++] = &KingSquareScoreEnding[Square];
	}

	// Pawns (4.4)

	TuningParamStore.Params[TuningParamStore.Count++] = &PawnDoubledOpening;
	TuningParamStore.Params[TuningParamStore.Count++] = &PawnDoubledEnding;

	TuningParamStore.Params[TuningParamStore.Count++] = &PawnIsolatedOpening;
	TuningParamStore.Params[TuningParamStore.Count++] = &PawnIsolatedEnding;

	TuningParamStore.Params[TuningParamStore.Count++] = &PawnIsolatedOpenOpening;
	TuningParamStore.Params[TuningParamStore.Count++] = &PawnIsolatedOpenEnding;

	TuningParamStore.Params[TuningParamStore.Count++] = &PawnBackwardOpening;
	TuningParamStore.Params[TuningParamStore.Count++] = &PawnBackwardEnding;

	TuningParamStore.Params[TuningParamStore.Count++] = &PawnBackwardOpenOpening;
	TuningParamStore.Params[TuningParamStore.Count++] = &PawnBackwardOpenEnding;

	for (int Rank = 1; Rank < 7; ++Rank) {
		TuningParamStore.Params[TuningParamStore.Count++] = &PawnCandidateOpening[Rank];
	}

	for (int Rank = 1; Rank < 7; ++Rank) {
		TuningParamStore.Params[TuningParamStore.Count++] = &PawnCandidateEnding[Rank];
	}

	// Tempo (4.5)

	TuningParamStore.Params[TuningParamStore.Count++] = &TempoOpening;
	TuningParamStore.Params[TuningParamStore.Count++] = &TempoEnding;

	// Pattern (4.6)

	TuningParamStore.Params[TuningParamStore.Count++] = &TrappedBishopOpening;
	TuningParamStore.Params[TuningParamStore.Count++] = &TrappedBishopEnding;

	TuningParamStore.Params[TuningParamStore.Count++] = &BlockedBishopOpening;
	TuningParamStore.Params[TuningParamStore.Count++] = &BlockedBishopEnding;

	TuningParamStore.Params[TuningParamStore.Count++] = &BlockedRookOpening;
	TuningParamStore.Params[TuningParamStore.Count++] = &BlockedRookEnding;

	// Piece (4.7)

	// Mobility (4.7.1)

	TuningParamStore.Params[TuningParamStore.Count++] = &KnightMobilityMoveDecrease;
	TuningParamStore.Params[TuningParamStore.Count++] = &BishopMobilityMoveDecrease;
	TuningParamStore.Params[TuningParamStore.Count++] = &RookMobilityMoveDecrease;

	TuningParamStore.Params[TuningParamStore.Count++] = &KnightMobility;
	TuningParamStore.Params[TuningParamStore.Count++] = &BishopMobility;

	TuningParamStore.Params[TuningParamStore.Count++] = &RookMobilityOpening;
	TuningParamStore.Params[TuningParamStore.Count++] = &RookMobilityEnding;

	// Open file (4.7.2)

	TuningParamStore.Params[TuningParamStore.Count++] = &RookOnClosedFileOpening;
	TuningParamStore.Params[TuningParamStore.Count++] = &RookOnClosedFileEnding;

	TuningParamStore.Params[TuningParamStore.Count++] = &RookOnSemiOpenFileOpening;
	TuningParamStore.Params[TuningParamStore.Count++] = &RookOnSemiOpenFileEnding;

	TuningParamStore.Params[TuningParamStore.Count++] = &RookOnSemiOpenFileAdjacentToEnemyKingOpening;
	TuningParamStore.Params[TuningParamStore.Count++] = &RookOnSemiOpenFileAdjacentToEnemyKingEnding;

	TuningParamStore.Params[TuningParamStore.Count++] = &RookOnSemiOpenFileSameToEnemyKingOpening;
	TuningParamStore.Params[TuningParamStore.Count++] = &RookOnSemiOpenFileSameToEnemyKingEnding;

	TuningParamStore.Params[TuningParamStore.Count++] = &RookOnOpenFileOpening;
	TuningParamStore.Params[TuningParamStore.Count++] = &RookOnOpenFileEnding;

	TuningParamStore.Params[TuningParamStore.Count++] = &RookOnOpenFileAdjacentToEnemyKingOpening;
	TuningParamStore.Params[TuningParamStore.Count++] = &RookOnOpenFileAdjacentToEnemyKingEnding;

	TuningParamStore.Params[TuningParamStore.Count++] = &RookOnOpenFileSameToEnemyKingOpening;
	TuningParamStore.Params[TuningParamStore.Count++] = &RookOnOpenFileSameToEnemyKingEnding;

	// Outpost (4.7.3)

	TuningParamStore.Params[TuningParamStore.Count++] = &KnightOutpost[18]; // c6
	TuningParamStore.Params[TuningParamStore.Count++] = &KnightOutpost[19]; // d6
	TuningParamStore.Params[TuningParamStore.Count++] = &KnightOutpost[20]; // e6
	TuningParamStore.Params[TuningParamStore.Count++] = &KnightOutpost[21]; // f6

	TuningParamStore.Params[TuningParamStore.Count++] = &KnightOutpost[25]; // b5
	TuningParamStore.Params[TuningParamStore.Count++] = &KnightOutpost[26]; // c5
	TuningParamStore.Params[TuningParamStore.Count++] = &KnightOutpost[27]; // d5
	TuningParamStore.Params[TuningParamStore.Count++] = &KnightOutpost[28]; // e5
	TuningParamStore.Params[TuningParamStore.Count++] = &KnightOutpost[29]; // f5
	TuningParamStore.Params[TuningParamStore.Count++] = &KnightOutpost[30]; // g5

	TuningParamStore.Params[TuningParamStore.Count++] = &KnightOutpost[33]; // b4
	TuningParamStore.Params[TuningParamStore.Count++] = &KnightOutpost[34]; // c4
	TuningParamStore.Params[TuningParamStore.Count++] = &KnightOutpost[35]; // d4
	TuningParamStore.Params[TuningParamStore.Count++] = &KnightOutpost[36]; // e4
	TuningParamStore.Params[TuningParamStore.Count++] = &KnightOutpost[37]; // f4
	TuningParamStore.Params[TuningParamStore.Count++] = &KnightOutpost[38]; // g4

	// Seventh rank (4.7.4)

	TuningParamStore.Params[TuningParamStore.Count++] = &RookOnSeventhOpening;
	TuningParamStore.Params[TuningParamStore.Count++] = &RookOnSeventhEnding;

	TuningParamStore.Params[TuningParamStore.Count++] = &QueenOnSeventhOpening;
	TuningParamStore.Params[TuningParamStore.Count++] = &QueenOnSeventhEnding;

	// King distance (4.7.5)

	// King (4.8)

	// Pawn shelter (4.8.1)

	TuningParamStore.Params[TuningParamStore.Count++] = &KingFriendlyPawnAdvanceBase;
	TuningParamStore.Params[TuningParamStore.Count++] = &KingFriendlyPawnAdvanceDefault;

	// Pawn storm (4.8.2)

	for (int Rank = 1; Rank < 7; ++Rank) {
		TuningParamStore.Params[TuningParamStore.Count++] = &KingPawnStorm[Rank];
	}

	// Piece attack (4.8.3)

	TuningParamStore.Params[TuningParamStore.Count++] = &KingZoneAttackedBase;

	for (int Attackers = 0; Attackers < 8; ++Attackers) {
		TuningParamStore.Params[TuningParamStore.Count++] = &KingZoneAttackedWeight[Attackers];
	}

	// Passed pawns (4.9)

	TuningParamStore.Params[TuningParamStore.Count++] = &PawnPassedEndingBase1;
	TuningParamStore.Params[TuningParamStore.Count++] = &PawnPassedEndingBase2;

	TuningParamStore.Params[TuningParamStore.Count++] = &PawnPassedEndingHostileKingDistance;
	TuningParamStore.Params[TuningParamStore.Count++] = &PawnPassedEndingFriendlyKingDistance;

	TuningParamStore.Params[TuningParamStore.Count++] = &PawnPassedEndingConsideredFree;

	TuningParamStore.Params[TuningParamStore.Count++] = &PawnPassedEndingUnstoppable;

	for (int Rank = 1; Rank < 7; ++Rank) {
		TuningParamStore.Params[TuningParamStore.Count++] = &PawnPassedOpening[Rank];
	}

	for (int Rank = 1; Rank < 7; ++Rank) {
		TuningParamStore.Params[TuningParamStore.Count++] = &PawnPassedEndingWeight[Rank];
	}
}

void LoadTuningParams(void)
{
	FILE* File;

	char Buf[64];

	printf("\n");

	printf("Load params...\n");

	fopen_s(&File, "params.txt", "r");

	if (File == NULL) { // File open error
		printf("File 'params.txt' open error!\n");

		return;
	}

	for (int ParamIndex = 0; ParamIndex < TuningParamStore.Count; ++ParamIndex) {
		fgets(Buf, sizeof(Buf), File);

//		*TuningParamStore.Params[ParamIndex] = atoi(Buf);
		*TuningParamStore.Params[ParamIndex] = atof(Buf);
	}

	fclose(File);

	printf("Load params...DONE (%d)\n", TuningParamStore.Count);
}

void SaveTuningParams(void)
{
	FILE* File;

	fopen_s(&File, "params.txt", "w");

	if (File == NULL) { // File create (open) error
		printf("File 'params.txt' create (open) error!\n");

		Sleep(3000);

		exit(0);
	}

	for (int ParamIndex = 0; ParamIndex < TuningParamStore.Count; ++ParamIndex) {
//		fprintf(File, "%d\n", *TuningParamStore.Params[ParamIndex]);
		fprintf(File, "%.2f\n", *TuningParamStore.Params[ParamIndex]);
	}

	fclose(File);
}

void PrintTuningParams(void)
{
	printf("\n");

	for (int ParamIndex = 0; ParamIndex < TuningParamStore.Count; ++ParamIndex) {
//		printf("ParamIndex = %d Value = %d\n", ParamIndex + 1, *TuningParamStore.Params[ParamIndex]);
		printf("ParamIndex = %d Value = %.2f\n", ParamIndex + 1, *TuningParamStore.Params[ParamIndex]);
	}
}

void TuningLocalSearch(void)
{
	int InputThreads;

	int Epoch;

	double BestError;
	double Error;

	BOOL Improved;

	// No cache used
	InitHashTable(1);
	ClearHash();

	// Input max. threads

	printf("\n");

	printf("Threads (min. 1 max. %d; 0 = %d): ", MaxThreads, DEFAULT_THREADS);
	scanf_s("%d", &InputThreads);

	InputThreads = (InputThreads >= 1 && InputThreads <= MaxThreads) ? InputThreads : DEFAULT_THREADS;

	omp_set_num_threads(InputThreads);

	// Prepare board

	SetFen(&CurrentBoard, StartFen);

	#ifdef MOVES_SORT_HEURISTIC
	ClearHeuristic(&CurrentBoard);
	#endif // MOVES_SORT_HEURISTIC

	ReadFenFile();

	printf("\n");

	printf("Tuning...\n");

	Epoch = 1;

	BestError = CalculateError(PositionStore.Count);

	Improved = TRUE;

	while (Improved) {
		printf("\n");

		printf("Epoch = %d Best error = %.8f\n", Epoch, BestError);

//		PrintTuningParams();

		SaveTuningParams();

		Improved = FALSE;

		for (int ParamIndex = 0; ParamIndex < TuningParamStore.Count; ++ParamIndex) {
			*TuningParamStore.Params[ParamIndex] += 1; // Param + 1

			Error = CalculateError(PositionStore.Count);

			if (Error < BestError) {
				BestError = Error;

				Improved = TRUE;
			}
			else {
				*TuningParamStore.Params[ParamIndex] -= 2; // Param - 1

				Error = CalculateError(PositionStore.Count);

				if (Error < BestError) {
					BestError = Error;

					Improved = TRUE;
				}
				else {
					*TuningParamStore.Params[ParamIndex] += 1; // Param = Old param
				}
			}
		}

		++Epoch;
	} // while

	printf("\n");

	printf("Tuning...DONE\n");

	PositionStoreFree();
}

void ShufflePositions(void)
{
	PositionItem** PositionPointer1;
	PositionItem** PositionPointer2;

	PositionItem* TempItemPointer;

	U64 RandomValue;

	SetRandState(Clock());

	for (int PositionNumber = 0; PositionNumber < PositionStore.Count; ++PositionNumber) {
		PositionPointer1 = &PositionStore.Positions[PositionNumber];

		RandomValue = Rand64();

		PositionPointer2 = &PositionStore.Positions[RandomValue % PositionStore.Count];

		TempItemPointer = *PositionPointer1;
		*PositionPointer1 = *PositionPointer2;
		*PositionPointer2 = TempItemPointer;
	}
}

void CalculateGradients(const int BatchSize, const double BaseError)
{
	double Error;

	printf("\n");

	printf("Calculation gradients...\n");

//	printf("\n");

	for (int ParamIndex = 0; ParamIndex < TuningParamStore.Count; ++ParamIndex) {
		*TuningParamStore.Params[ParamIndex] += 2; // Param + 2

		Error = CalculateError(BatchSize);

		TuningParamStore.Gradients[ParamIndex] = (Error - BaseError) / 2.0;

//		printf("ParamIndex = %d Gradient = %.8f\n", ParamIndex + 1, TuningParamStore.Gradients[ParamIndex]);

		*TuningParamStore.Params[ParamIndex] -= 2; // Param = Old param
	}

	printf("\n");

	printf("Calculation gradients...DONE\n");
}

void TuningAdamSGD(void)
{
	int InputThreads;

	const double Alpha = 0.1; // 0.001
	const double Beta1 = 0.9;
	const double Beta2 = 0.999;
	const double Epsilon = 1.0e-8;

	double M[MAX_TUNING_PARAMS];
	double V[MAX_TUNING_PARAMS];

	double M_Corrected;
	double V_Corrected;

	int BatchSize;

	int Epoch;

	double BestError;
	double CompleteError;
	double DiffError;

	double BaseError;
	double CurrentError;

	double Gradient;

	double Delta;

	// No cache used
	InitHashTable(1);
	ClearHash();

	// Input max. threads

	printf("\n");

	printf("Threads (min. 1 max. %d; 0 = %d): ", MaxThreads, DEFAULT_THREADS);
	scanf_s("%d", &InputThreads);

	InputThreads = (InputThreads >= 1 && InputThreads <= MaxThreads) ? InputThreads : DEFAULT_THREADS;

	omp_set_num_threads(InputThreads);

	// Prepare board

	SetFen(&CurrentBoard, StartFen);

	#ifdef MOVES_SORT_HEURISTIC
	ClearHeuristic(&CurrentBoard);
	#endif // MOVES_SORT_HEURISTIC

	ReadFenFile();

	// Initialize M[] and V[]

	for (int ParamIndex = 0; ParamIndex < TuningParamStore.Count; ++ParamIndex) {
		M[ParamIndex] = 0.0;
		V[ParamIndex] = 0.0;
	}

	#ifdef TUNING_BATCH_SIZE_FENS
	BatchSize = TUNING_BATCH_SIZE_FENS;
	#elif defined(TUNING_BATCH_SIZE_PERCENT)
	BatchSize = PositionStore.Count * TUNING_BATCH_SIZE_PERCENT / 100;
	#else // NONE
	BatchSize = PositionStore.Count;
	#endif // TUNING_BATCH_SIZE_FENS/TUNING_BATCH_SIZE_PERCENT/NONE

	printf("\n");

	printf("Tuning...\n");

	Epoch = 1;

	BestError = CalculateError(PositionStore.Count);

	printf("\n");

	printf("Best error = %.8f\n", BestError);

//	PrintTuningParams();

	SaveTuningParams();

	while (TRUE) {
		printf("\n");

		printf("Epoch = %d\n", Epoch);

		ShufflePositions();

		BaseError = CalculateError(BatchSize);

		CalculateGradients(BatchSize, BaseError);

//		printf("\n");

		for (int ParamIndex = 0; ParamIndex < TuningParamStore.Count; ++ParamIndex) {
			Gradient = TuningParamStore.Gradients[ParamIndex];

			M[ParamIndex] = Beta1 * M[ParamIndex] + (1 - Beta1) * Gradient;
			V[ParamIndex] = Beta2 * V[ParamIndex] + (1 - Beta2) * Gradient * Gradient;

			M_Corrected = M[ParamIndex] / (1.0 - pow(Beta1, Epoch));
			V_Corrected = V[ParamIndex] / (1.0 - pow(Beta2, Epoch));

			Delta = Alpha * M_Corrected / (sqrt(V_Corrected) + Epsilon);

//			printf("ParamIndex = %d M_Corrected = %.8f V_Corrected = %.8f Delta = %.8f\n", ParamIndex + 1, M_Corrected, V_Corrected, Delta);

			*TuningParamStore.Params[ParamIndex] -= Delta;
		}

		CurrentError = CalculateError(BatchSize);

		printf("\n");

		printf("BaseError = %.8f CurrentError = %.8f DiffError = %.8f\n", BaseError, CurrentError, BaseError - CurrentError);

		if ((Epoch % 10) == 0) {
			CompleteError = CalculateError(PositionStore.Count);

			DiffError = BestError - CompleteError;

			printf("\n");

			printf("Best error = %.8f CompleteError = %.8f DiffError = %.8f\n", BestError, CompleteError, DiffError);

//			PrintTuningParams();

			SaveTuningParams();

			if (fabs(DiffError) < 1.0e-8) {
				break; // while
			}

			BestError = CompleteError;
		}

		++Epoch;
	} // while

	printf("\n");

	printf("Tuning...DONE\n");

	PositionStoreFree();
}

#endif // TUNING && TOGA_EVALUATION_FUNCTION