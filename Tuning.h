// Tuning.h

#pragma once

#ifndef TUNING_H
#define TUNING_H

#include "Def.h"

void Pgn2Fen(void);

#if defined(TUNING) && defined(TOGA_EVALUATION_FUNCTION)
void FindBestK(void);

void InitTuningParams(void);
void LoadTuningParams(void);
void SaveTuningParams(void);

void TuningLocalSearch(void);
//void TuningAdamSGD(void);
#endif // TUNING && TOGA_EVALUATION_FUNCTION

#endif // !TUNING_H