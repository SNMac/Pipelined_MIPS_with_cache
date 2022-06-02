//
// Created by SNMac on 2022/06/01.
//

#ifndef CAMP_PROJECT4_STAGES_H
#define CAMP_PROJECT4_STAGES_H

#include <stdint.h>

/* Stages */
void OnelevelIF(const int* Predictbit);  // Instruction Fetch (One Level Predictor)
void OnelevelID(const int* Predictbit, const char* Counter);  // Instruction Decode (One Level Predictor)
void GshareIF(const int* Predictbit);  // Instruction Fetch (Gshare Predictor)
void GshareID(const int* Predictbit, const char* Counter);  // Instruction Decode (Gshare Predictor)
void LocalIF(const int* Predictbit);  // Instruction Fetch (Local Predictor)
void LocalID(const int* Predictbit, const char* Counter);  // Instruction Decode (Local Predictor)
void AlwaysTakenIF(void);  // Instruction Fetch (Always taken predictor)
void AlwaysTakenID(void);  // Instruction Decode (Always taken predictor)
void AlwaysnotTakenIF(void);  // Instruction Fetch (Always not taken predictor)
void AlwaysnotTakenID(void);  // Instruction Decode (Always not taken predictor)
void BTFNTIF(void);  // Instruction Fetch (Backward Taken, Forward Not Taken predictor)
void BTFNTID(void);  // Instruction Decode (Backward Taken, Forward Not Taken predictor)
void EX(void);  // EXecute
void MEM(const int* Cacheset, const int* Cachesize);  // MEMory access
void WB(void);  // Write Back

// make GHR index (2^GHRs)
uint8_t makeGHRindex(const bool GHR[], uint32_t PCvalue);

#endif //CAMP_PROJECT4_STAGES_H
