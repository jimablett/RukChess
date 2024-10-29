// Move.cpp

#include "stdafx.h"

#include "Move.h"

#include "BitBoard.h"
#include "Board.h"
#include "Def.h"
#include "Hash.h"
#include "Types.h"

void MakeMove(BoardItem* Board, const MoveItem Move)
{
    HistoryItem* Info = &Board->MoveTable[Board->HalfMoveNumber++];

    int From = MOVE_FROM(Move.Move);
    int To = MOVE_TO(Move.Move);

    assert(From >= 0 && From <= 63);
    assert(To >= 0 && To <= 63);

    Info->Type = Move.Type;

    Info->From = From;
    Info->PieceFrom = PIECE_TYPE(Board->Pieces[From]);

    assert(Info->PieceFrom >= PAWN && Info->PieceFrom <= KING);

    Info->To = To;
    Info->PieceTo = PIECE_TYPE(Board->Pieces[To]);

    assert((Info->PieceTo >= PAWN && Info->PieceTo <= QUEEN) || Info->PieceTo == PIECE_TYPE(NO_PIECE));

    Info->PromotePiece = MOVE_PROMOTE_PIECE_TYPE(Move.Move);

    assert((Info->PromotePiece >= KNIGHT && Info->PromotePiece <= QUEEN) || Info->PromotePiece == 0);

    Info->PassantSquare = Board->PassantSquare;

    assert((Info->PassantSquare >= 16 && Info->PassantSquare <= 23) || (Info->PassantSquare >= 40 && Info->PassantSquare <= 47) || Info->PassantSquare == -1);

    Info->CastleFlags = Board->CastleFlags;
    Info->FiftyMove = Board->FiftyMove;

    Info->Hash = Board->Hash;

#ifdef DEBUG_MOVE
    Info->BB_WhitePieces = Board->BB_WhitePieces;
    Info->BB_BlackPieces = Board->BB_BlackPieces;

    for (int Color = 0; Color < 2; ++Color) {
        for (int Piece = 0; Piece < 6; ++Piece) {
            Info->BB_Pieces[Color][Piece] = Board->BB_Pieces[Color][Piece];
        }
    }
#endif // DEBUG_MOVE

    Info->Accumulator = Board->Accumulator;

    if (Info->PassantSquare != -1) {
        Board->PassantSquare = -1;

        Board->Hash ^= PassantHash[Info->PassantSquare];
    }

    if (Board->CurrentColor == WHITE) {
        if (Move.Type & MOVE_PAWN_2) {
            Board->PassantSquare = From - 8;

            Board->Hash ^= PassantHash[Board->PassantSquare];
        }

        if (Move.Type & MOVE_CASTLE_KING) { // White O-O
            Board->Pieces[SQ_H1] = NO_PIECE;

            Board->BB_WhitePieces &= ~BB_SQUARE(SQ_H1);
            Board->BB_Pieces[WHITE][ROOK] &= ~BB_SQUARE(SQ_H1);

            Board->Hash ^= PieceHash[WHITE][ROOK][SQ_H1];

            Board->Pieces[SQ_F1] = PIECE_CREATE(ROOK, WHITE);

            Board->BB_WhitePieces |= BB_SQUARE(SQ_F1);
            Board->BB_Pieces[WHITE][ROOK] |= BB_SQUARE(SQ_F1);

            Board->Hash ^= PieceHash[WHITE][ROOK][SQ_F1];
        }

        if (Move.Type & MOVE_CASTLE_QUEEN) { // White O-O-O
            Board->Pieces[SQ_A1] = NO_PIECE;

            Board->BB_WhitePieces &= ~BB_SQUARE(SQ_A1);
            Board->BB_Pieces[WHITE][ROOK] &= ~BB_SQUARE(SQ_A1);

            Board->Hash ^= PieceHash[WHITE][ROOK][SQ_A1];

            Board->Pieces[SQ_D1] = PIECE_CREATE(ROOK, WHITE);

            Board->BB_WhitePieces |= BB_SQUARE(SQ_D1);
            Board->BB_Pieces[WHITE][ROOK] |= BB_SQUARE(SQ_D1);

            Board->Hash ^= PieceHash[WHITE][ROOK][SQ_D1];
        }

        Board->CastleFlags &= CastleMask[From] & CastleMask[To];

        if (Move.Type & MOVE_PAWN_PASSANT) {
            Info->EatPawnSquare = To + 8;

            assert(Info->EatPawnSquare >= 24 && Info->EatPawnSquare <= 31);

            Board->Pieces[Info->EatPawnSquare] = NO_PIECE;

            Board->BB_BlackPieces &= ~BB_SQUARE(Info->EatPawnSquare);
            Board->BB_Pieces[BLACK][PAWN] &= ~BB_SQUARE(Info->EatPawnSquare);

            Board->Hash ^= PieceHash[BLACK][PAWN][Info->EatPawnSquare];
        }
        else if (Move.Type & MOVE_CAPTURE) {
            Board->BB_BlackPieces &= ~BB_SQUARE(To);
            Board->BB_Pieces[BLACK][Info->PieceTo] &= ~BB_SQUARE(To);

            Board->Hash ^= PieceHash[BLACK][Info->PieceTo][To];
        }

        if (Move.Type & MOVE_PAWN_PROMOTE) {
            Board->Pieces[To] = PIECE_CREATE(Info->PromotePiece, WHITE);

            Board->BB_WhitePieces |= BB_SQUARE(To);
            Board->BB_Pieces[WHITE][Info->PromotePiece] |= BB_SQUARE(To);

            Board->Hash ^= PieceHash[WHITE][Info->PromotePiece][To];
        }
        else {
            Board->Pieces[To] = Board->Pieces[From];

            Board->BB_WhitePieces |= BB_SQUARE(To);
            Board->BB_Pieces[WHITE][Info->PieceFrom] |= BB_SQUARE(To);

            Board->Hash ^= PieceHash[WHITE][Info->PieceFrom][To];
        }

        Board->Pieces[From] = NO_PIECE;

        Board->BB_WhitePieces &= ~BB_SQUARE(From);
        Board->BB_Pieces[WHITE][Info->PieceFrom] &= ~BB_SQUARE(From);

        Board->Hash ^= PieceHash[WHITE][Info->PieceFrom][From];
    }
    else { // BLACK
        if (Move.Type & MOVE_PAWN_2) {
            Board->PassantSquare = From + 8;

            Board->Hash ^= PassantHash[Board->PassantSquare];
        }

        if (Move.Type & MOVE_CASTLE_KING) { // Black O-O
            Board->Pieces[SQ_H8] = NO_PIECE;

            Board->BB_BlackPieces &= ~BB_SQUARE(SQ_H8);
            Board->BB_Pieces[BLACK][ROOK] &= ~BB_SQUARE(SQ_H8);

            Board->Hash ^= PieceHash[BLACK][ROOK][SQ_H8];

            Board->Pieces[SQ_F8] = PIECE_CREATE(ROOK, BLACK);

            Board->BB_BlackPieces |= BB_SQUARE(SQ_F8);
            Board->BB_Pieces[BLACK][ROOK] |= BB_SQUARE(SQ_F8);

            Board->Hash ^= PieceHash[BLACK][ROOK][SQ_F8];
        }

        if (Move.Type & MOVE_CASTLE_QUEEN) { // Black O-O-O
            Board->Pieces[SQ_A8] = NO_PIECE;

            Board->BB_BlackPieces &= ~BB_SQUARE(SQ_A8);
            Board->BB_Pieces[BLACK][ROOK] &= ~BB_SQUARE(SQ_A8);

            Board->Hash ^= PieceHash[BLACK][ROOK][SQ_A8];

            Board->Pieces[SQ_D8] = PIECE_CREATE(ROOK, BLACK);

            Board->BB_BlackPieces |= BB_SQUARE(SQ_D8);
            Board->BB_Pieces[BLACK][ROOK] |= BB_SQUARE(SQ_D8);

            Board->Hash ^= PieceHash[BLACK][ROOK][SQ_D8];
        }

        Board->CastleFlags &= CastleMask[From] & CastleMask[To];

        if (Move.Type & MOVE_PAWN_PASSANT) {
            Info->EatPawnSquare = To - 8;

            assert(Info->EatPawnSquare >= 32 && Info->EatPawnSquare <= 39);

            Board->Pieces[Info->EatPawnSquare] = NO_PIECE;

            Board->BB_WhitePieces &= ~BB_SQUARE(Info->EatPawnSquare);
            Board->BB_Pieces[WHITE][PAWN] &= ~BB_SQUARE(Info->EatPawnSquare);

            Board->Hash ^= PieceHash[WHITE][PAWN][Info->EatPawnSquare];
        }
        else if (Move.Type & MOVE_CAPTURE) {
            Board->BB_WhitePieces &= ~BB_SQUARE(To);
            Board->BB_Pieces[WHITE][Info->PieceTo] &= ~BB_SQUARE(To);

            Board->Hash ^= PieceHash[WHITE][Info->PieceTo][To];
        }

        if (Move.Type & MOVE_PAWN_PROMOTE) {
            Board->Pieces[To] = PIECE_CREATE(Info->PromotePiece, BLACK);

            Board->BB_BlackPieces |= BB_SQUARE(To);
            Board->BB_Pieces[BLACK][Info->PromotePiece] |= BB_SQUARE(To);

            Board->Hash ^= PieceHash[BLACK][Info->PromotePiece][To];
        }
        else {
            Board->Pieces[To] = Board->Pieces[From];

            Board->BB_BlackPieces |= BB_SQUARE(To);
            Board->BB_Pieces[BLACK][Info->PieceFrom] |= BB_SQUARE(To);

            Board->Hash ^= PieceHash[BLACK][Info->PieceFrom][To];
        }

        Board->Pieces[From] = NO_PIECE;

        Board->BB_BlackPieces &= ~BB_SQUARE(From);
        Board->BB_Pieces[BLACK][Info->PieceFrom] &= ~BB_SQUARE(From);

        Board->Hash ^= PieceHash[BLACK][Info->PieceFrom][From];
    }

    if (Move.Type & (MOVE_CAPTURE | MOVE_PAWN | MOVE_PAWN_2)) {
        Board->FiftyMove = 0;
    }
    else {
        ++Board->FiftyMove;
    }

    Board->CurrentColor ^= 1;

    Board->Hash ^= ColorHash;

#ifdef DEBUG_HASH
    U64 OriginalHash = Board->Hash;

    InitHash(Board);

    if (Board->Hash != OriginalHash) {
        printf("-- Board hash error! BoardHash = 0x%016llx OriginalHash = 0x%016llx\n", Board->Hash, OriginalHash);
    }
#endif // DEBUG_HASH

#ifdef USE_NNUE_UPDATE
    Board->Accumulator.AccumulationComputed = FALSE;
#endif // USE_NNUE_UPDATE
}

void UnmakeMove(BoardItem* Board)
{
    HistoryItem* Info = &Board->MoveTable[--Board->HalfMoveNumber];

    Board->CurrentColor ^= 1;

    if (Board->CurrentColor == WHITE) {
        Board->Pieces[Info->From] = PIECE_CREATE(Info->PieceFrom, WHITE);

        Board->BB_WhitePieces |= BB_SQUARE(Info->From);
        Board->BB_Pieces[WHITE][Info->PieceFrom] |= BB_SQUARE(Info->From);

        if (Info->Type & MOVE_CASTLE_KING) { // White O-O
            Board->Pieces[SQ_F1] = NO_PIECE;

            Board->BB_WhitePieces &= ~BB_SQUARE(SQ_F1);
            Board->BB_Pieces[WHITE][ROOK] &= ~BB_SQUARE(SQ_F1);

            Board->Pieces[SQ_H1] = PIECE_CREATE(ROOK, WHITE);

            Board->BB_WhitePieces |= BB_SQUARE(SQ_H1);
            Board->BB_Pieces[WHITE][ROOK] |= BB_SQUARE(SQ_H1);
        }

        if (Info->Type & MOVE_CASTLE_QUEEN) { // White O-O-O
            Board->Pieces[SQ_D1] = NO_PIECE;

            Board->BB_WhitePieces &= ~BB_SQUARE(SQ_D1);
            Board->BB_Pieces[WHITE][ROOK] &= ~BB_SQUARE(SQ_D1);

            Board->Pieces[SQ_A1] = PIECE_CREATE(ROOK, WHITE);

            Board->BB_WhitePieces |= BB_SQUARE(SQ_A1);
            Board->BB_Pieces[WHITE][ROOK] |= BB_SQUARE(SQ_A1);
        }

        if (Info->Type & MOVE_PAWN_PASSANT) {
            Board->Pieces[Info->To] = NO_PIECE;

            Board->BB_WhitePieces &= ~BB_SQUARE(Info->To);
            Board->BB_Pieces[WHITE][PAWN] &= ~BB_SQUARE(Info->To);

            Board->Pieces[Info->EatPawnSquare] = PIECE_CREATE(PAWN, BLACK);

            Board->BB_BlackPieces |= BB_SQUARE(Info->EatPawnSquare);
            Board->BB_Pieces[BLACK][PAWN] |= BB_SQUARE(Info->EatPawnSquare);
        }
        else if (Info->Type & MOVE_CAPTURE) {
            //Board->Pieces[Info->To] = NO_PIECE;

            Board->BB_WhitePieces &= ~BB_SQUARE(Info->To);

            if (Info->Type & MOVE_PAWN_PROMOTE) {
                Board->BB_Pieces[WHITE][Info->PromotePiece] &= ~BB_SQUARE(Info->To);
            }
            else {
                Board->BB_Pieces[WHITE][Info->PieceFrom] &= ~BB_SQUARE(Info->To);
            }

            Board->Pieces[Info->To] = PIECE_CREATE(Info->PieceTo, BLACK);

            Board->BB_BlackPieces |= BB_SQUARE(Info->To);
            Board->BB_Pieces[BLACK][Info->PieceTo] |= BB_SQUARE(Info->To);
        }
        else {
            Board->Pieces[Info->To] = NO_PIECE;

            Board->BB_WhitePieces &= ~BB_SQUARE(Info->To);

            if (Info->Type & MOVE_PAWN_PROMOTE) {
                Board->BB_Pieces[WHITE][Info->PromotePiece] &= ~BB_SQUARE(Info->To);
            }
            else {
                Board->BB_Pieces[WHITE][Info->PieceFrom] &= ~BB_SQUARE(Info->To);
            }
        }
    }
    else { // BLACK
        Board->Pieces[Info->From] = PIECE_CREATE(Info->PieceFrom, BLACK);

        Board->BB_BlackPieces |= BB_SQUARE(Info->From);
        Board->BB_Pieces[BLACK][Info->PieceFrom] |= BB_SQUARE(Info->From);

        if (Info->Type & MOVE_CASTLE_KING) { // Black O-O
            Board->Pieces[SQ_F8] = NO_PIECE;

            Board->BB_BlackPieces &= ~BB_SQUARE(SQ_F8);
            Board->BB_Pieces[BLACK][ROOK] &= ~BB_SQUARE(SQ_F8);

            Board->Pieces[SQ_H8] = PIECE_CREATE(ROOK, BLACK);

            Board->BB_BlackPieces |= BB_SQUARE(SQ_H8);
            Board->BB_Pieces[BLACK][ROOK] |= BB_SQUARE(SQ_H8);
        }

        if (Info->Type & MOVE_CASTLE_QUEEN) { // Black O-O-O
            Board->Pieces[SQ_D8] = NO_PIECE;

            Board->BB_BlackPieces &= ~BB_SQUARE(SQ_D8);
            Board->BB_Pieces[BLACK][ROOK] &= ~BB_SQUARE(SQ_D8);

            Board->Pieces[SQ_A8] = PIECE_CREATE(ROOK, BLACK);

            Board->BB_BlackPieces |= BB_SQUARE(SQ_A8);
            Board->BB_Pieces[BLACK][ROOK] |= BB_SQUARE(SQ_A8);
        }

        if (Info->Type & MOVE_PAWN_PASSANT) {
            Board->Pieces[Info->To] = NO_PIECE;

            Board->BB_BlackPieces &= ~BB_SQUARE(Info->To);
            Board->BB_Pieces[BLACK][PAWN] &= ~BB_SQUARE(Info->To);

            Board->Pieces[Info->EatPawnSquare] = PIECE_CREATE(PAWN, WHITE);

            Board->BB_WhitePieces |= BB_SQUARE(Info->EatPawnSquare);
            Board->BB_Pieces[WHITE][PAWN] |= BB_SQUARE(Info->EatPawnSquare);
        }
        else if (Info->Type & MOVE_CAPTURE) {
            //Board->Pieces[Info->To] = NO_PIECE;

            Board->BB_BlackPieces &= ~BB_SQUARE(Info->To);

            if (Info->Type & MOVE_PAWN_PROMOTE) {
                Board->BB_Pieces[BLACK][Info->PromotePiece] &= ~BB_SQUARE(Info->To);
            }
            else {
                Board->BB_Pieces[BLACK][Info->PieceFrom] &= ~BB_SQUARE(Info->To);
            }

            Board->Pieces[Info->To] = PIECE_CREATE(Info->PieceTo, WHITE);

            Board->BB_WhitePieces |= BB_SQUARE(Info->To);
            Board->BB_Pieces[WHITE][Info->PieceTo] |= BB_SQUARE(Info->To);
        }
        else {
            Board->Pieces[Info->To] = NO_PIECE;

            Board->BB_BlackPieces &= ~BB_SQUARE(Info->To);

            if (Info->Type & MOVE_PAWN_PROMOTE) {
                Board->BB_Pieces[BLACK][Info->PromotePiece] &= ~BB_SQUARE(Info->To);
            }
            else {
                Board->BB_Pieces[BLACK][Info->PieceFrom] &= ~BB_SQUARE(Info->To);
            }
        }
    }

    Board->PassantSquare = Info->PassantSquare;

    Board->CastleFlags = Info->CastleFlags;
    Board->FiftyMove = Info->FiftyMove;

    Board->Hash = Info->Hash;

#ifdef DEBUG_MOVE
    if (Board->BB_WhitePieces != Info->BB_WhitePieces) {
        printf("-- BB_WhitePieces error! From = %d To = %d Move type = %d\n", Info->From, Info->To, Info->Type);
    }

    if (Board->BB_BlackPieces != Info->BB_BlackPieces) {
        printf("-- BB_BlackPieces error! From = %d To = %d Move type = %d\n", Info->From, Info->To, Info->Type);
    }

    for (int Color = 0; Color < 2; ++Color) {
        for (int Piece = 0; Piece < 6; ++Piece) {
            if (Board->BB_Pieces[Color][Piece] != Info->BB_Pieces[Color][Piece]) {
                printf("-- BB_Pieces error! Color = %d Piece = %d From = %d To = %d Move type = %d\n", Color, Piece, Info->From, Info->To, Info->Type);
            }
        }
    }
#endif // DEBUG_MOVE

    Board->Accumulator = Info->Accumulator;
}

#ifdef NULL_MOVE_PRUNING

void MakeNullMove(BoardItem* Board)
{
    HistoryItem* Info = &Board->MoveTable[Board->HalfMoveNumber++];

    Info->Type = MOVE_NULL;

    Info->PassantSquare = Board->PassantSquare;

    Info->FiftyMove = Board->FiftyMove;

    Info->Hash = Board->Hash;

    Info->Accumulator = Board->Accumulator;

    if (Info->PassantSquare != -1) {
        Board->PassantSquare = -1;

        Board->Hash ^= PassantHash[Info->PassantSquare];
    }

    ++Board->FiftyMove;

    Board->CurrentColor ^= 1;

    Board->Hash ^= ColorHash;

#ifdef DEBUG_HASH
    U64 OriginalHash = Board->Hash;

    InitHash(Board);

    if (Board->Hash != OriginalHash) {
        printf("-- Board hash error! BoardHash = 0x%016llx OriginalHash = 0x%016llx\n", Board->Hash, OriginalHash);
    }
#endif // DEBUG_HASH
}

void UnmakeNullMove(BoardItem* Board)
{
    HistoryItem* Info = &Board->MoveTable[--Board->HalfMoveNumber];

    Board->CurrentColor ^= 1;

    Board->PassantSquare = Info->PassantSquare;

    Board->FiftyMove = Info->FiftyMove;

    Board->Hash = Info->Hash;

    Board->Accumulator = Info->Accumulator;
}

#endif // NULL_MOVE_PRUNING