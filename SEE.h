// SEE.h

#pragma once

#ifndef SEE_H
#define SEE_H

#include "Board.h"
#include "Def.h"

#if defined(PROBCUT) || defined(BAD_CAPTURE_LAST) || defined(SEE_CAPTURE_MOVE_PRUNING) || defined(SEE_QUIET_MOVE_PRUNING) || defined(QUIESCENCE_SEE_MOVE_PRUNING)
int CaptureSEE(const BoardItem* Board, const int From, const int To, const int PromotePiece, const int MoveType);
#endif // PROBCUT || BAD_CAPTURE_LAST || SEE_CAPTURE_MOVE_PRUNING || SEE_QUIET_MOVE_PRUNING || QUIESCENCE_SEE_MOVE_PRUNING

#endif // !SEE_H