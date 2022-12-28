// Tuning.h

#pragma once

#ifndef TUNING_H
#define TUNING_H

#include "Def.h"

void Pgn2Fen(void);

#if defined(TUNING_LOCAL_SEARCH) || defined(TUNING_ADAM_SGD)

void FindBestK(void);

void InitTuningParams(void);
void LoadTuningParams(void);
void SaveTuningParams(void);

#endif // TUNING_LOCAL_SEARCH || TUNING_ADAM_SGD

#ifdef TUNING_LOCAL_SEARCH
void TuningLocalSearch(void);
#endif // TUNING_LOCAL_SEARCH

#ifdef TUNING_ADAM_SGD
void TuningAdamSGD(void);
#endif // TUNING_ADAM_SGD

#endif // !TUNING_H