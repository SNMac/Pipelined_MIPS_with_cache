//
// Created by SNMac on 2022/05/13.
//

#ifndef CAMP_PROJECT3_DEBUG_H
#define CAMP_PROJECT3_DEBUG_H

#include <stdbool.h>
#include <stdint.h>

#include "Units.h"

/* Visible states */
typedef struct _DEBUGIF {
    uint32_t IFPC;
    uint32_t IFinst;
    bool Predict, AddressHit;
}DEBUGIF;

typedef struct _DEBUGID {
    bool valid;
    uint32_t IDPC;
    uint32_t IDinst;
    INSTRUCTION inst;
    bool Branch, PCBranch;
    bool Predict, AddressHit;
    uint8_t PB;
    char instprint[100];
}DEBUGID;

typedef struct _DEBUGEX {
    bool valid;
    bool ControlNOP;
    uint32_t EXPC;
    uint32_t EXinst;
    uint32_t ALUinput1;
    uint32_t ALUinput2;
    uint32_t ALUresult;
    char instprint[100];
}DEBUGEX;

typedef struct _DEBUGMEM {
    bool valid;
    bool ControlNOP;
    bool MemRead;
    bool MemWrite;
    uint32_t MEMPC;
    uint32_t MEMinst;
    uint32_t Addr;
    uint32_t Writedata;
    char instprint[100];
}DEBUGMEM;

typedef struct _DEBUGWB {
    bool valid;
    bool ControlNOP;
    bool RegWrite;
    bool Writereg;
    uint32_t WBPC;
    uint32_t WBinst;
    char instprint[100];
}DEBUGWB;

void printIF(int Predictor);
void printID(int Predictor, const char* Predictbit, const char* Counter);
void printEX(void);
void printMEM(const char* Cacheset, const char* Cachesize);
void printWB(void);
void printRformat(void);
void printIDforward(void);
void printEXforward(void);
void printUpdateBTB(const int* Predictor, const char* Predictbit, const char* Counter);
void printPBtaken(const int* Predictor, const char* Predictbit, const char* Counter);
void printPBnottaken(const int* Predictor, const char* Predictbit, const char* Counter);
void DebugPipelineHandsOver(void);

#endif //CAMP_PROJECT3_DEBUG_H
