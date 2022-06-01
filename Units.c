//
// Created by SNMac on 2022/05/09.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "Units.h"
#include "main.h"

PROGRAM_COUNTER PC;
IFID ifid[2];
IDEX idex[2];
EXMEM exmem[2];
MEMWB memwb[2];
CONTROL_SIGNAL ctrlSig;
ALU_CONTROL_SIGNAL ALUctrlSig;
BRANCH_PREDICT BranchPred;
FORWARD_SIGNAL fwrdSig;
ID_FORWARD_SIGNAL idfwrdSig;
MEM_FORWARD_SIGNAL memfwrdSig;
HAZARD_DETECTION_SIGNAL hzrddetectSig;

// from main.c
extern uint32_t Memory[0x400000];
extern uint32_t R[32];
extern COUNTING counting;

/*============================Data units============================*/

// [Instruction decoder]
void InstDecoder(INSTRUCTION *inst, uint32_t instruction) {
    memset(inst, 0, sizeof(INSTRUCTION));
    inst->address = instruction & 0x3ffffff;
    inst->opcode = (instruction & 0xfc000000) >> 26;
    inst->rs = (instruction & 0x03e00000) >> 21;
    inst->rt = (instruction & 0x001f0000) >> 16;
    inst->rd = (instruction & 0x0000f800) >> 11;
    inst->shamt = (instruction & 0x000007c0) >> 6;
    inst->imm = instruction & 0x0000ffff;
    inst->funct = inst->imm & 0x003f;
}

// [Instruction memory]
uint32_t InstMem(uint32_t Readaddr) {
    uint32_t Instruction = Memory[Readaddr / 4];
    return Instruction;
}

// [Registers]
uint32_t* RegsRead(uint8_t Readreg1, uint8_t Readreg2) {
    static uint32_t Readdata[2];
    Readdata[0] = R[Readreg1];
    Readdata[1] = R[Readreg2];
    return Readdata;
}
void RegsWrite(uint8_t Writereg, uint32_t Writedata, bool RegWrite) {
    if (RegWrite) {  // RegWrite asserted
        if (Writereg == 0 && Writedata != 0) {
            fprintf(stderr, "WARNING: Cannot change value at $0\n");
            return;
        }
        R[Writereg] = Writedata;  // GPR write enabled
        return;
    }
}

// [Data memory]
uint32_t DataMem(uint32_t Addr, uint32_t Writedata, bool MemRead, bool MemWrite) {
    uint32_t Readdata = 0;
    if (MemRead) {  // MemRead asserted, MemWrite De-asserted
        if (Addr > 0x1000000) {  // loading outside of memory
            fprintf(stderr, "ERROR: Accessing outside of memory\n");
            exit(EXIT_FAILURE);
        }
        Readdata = Memory[Addr / 4];  // Memory read port return load value
        counting.Memcount++;
    }
    else if (MemWrite) {  // MemRead De-asserted, MemWrite asserted
        if (Addr > 0x1000000) {  // Writing outside of memory
            fprintf(stderr, "ERROR: Accessing outside of memory\n");
            exit(EXIT_FAILURE);
        }
        Memory[Addr / 4] = Writedata;  // Memory write enabled
        counting.Memcount++;
    }
    return Readdata;
}

// [One-level branch predictor]
void CheckBranch(uint32_t PCvalue, const char* Predictbit) {  // Check if PC is branch instruction
    BranchPred.instPC[0] = PCvalue;
    if (BranchPred.BTBsize == 0 && BranchPred.DPsize == 0) {  // BTB, DP is empty
        BranchPred.AddressHit[0] = 0;  // Branch not predicted
        BranchPred.Predict[0] = 0;  // Predict branch not taken
        return;
    }
    else if (BranchPred.BTBsize != BranchPred.DPsize) {
        fprintf(stderr, "ERROR: Branch predictor is wrong\n");
        exit(EXIT_FAILURE);
    }

    // Find PC in BTB
    for (BranchPred.BTBindex[0] = 0; BranchPred.BTBindex[0] < BTBMAX; BranchPred.BTBindex[0]++) {
        if (BranchPred.BTB[BranchPred.BTBindex[0]][0] == PCvalue) {  // PC found in BTB
            BranchPred.AddressHit[0] = 1;  // Branch predicted
            BranchPred.BTB[BranchPred.BTBindex[0]][2]++;
            break;  // Send out predicted PC
        }
    }

    // PC not found in BTB
    if (BranchPred.BTBindex[0] == BTBMAX) {
        BranchPred.AddressHit[0] = 0;  // Branch not predicted
        BranchPred.Predict[0] = 0;  // Predict branch not taken
        return;
    }

    // Find PC in DP
    for (BranchPred.DPindex[0] = 0; BranchPred.DPindex[0] < BTBMAX; BranchPred.DPindex[0]++) {
        if (BranchPred.DP[BranchPred.DPindex[0]][0] == PCvalue) {
            // PC found in BTB
            switch (*Predictbit) {
                case '1' :  // One-bit predictor
                    if (BranchPred.DP[BranchPred.DPindex[0]][1] == 1)  {
                        BranchPred.Predict[0] = 1;  // Predict branch taken
                    }
                    else {
                        BranchPred.Predict[0] = 0;  // Predict branch not taken
                    }
                    break;

                case '2' :  // Two-bit predictor
                    if (BranchPred.DP[BranchPred.DPindex[0]][1] == 2 || BranchPred.DP[BranchPred.DPindex[0]][1] == 3)  {
                        BranchPred.Predict[0] = 1;  // Predict branch taken
                    }
                    else {
                        BranchPred.Predict[0] = 0;  // Predict branch not taken
                    }
                    break;

                default :
                    fprintf(stderr, "ERROR: Wrong prediction bit select number\n");
                    exit(EXIT_FAILURE);
            }
            BranchPred.DP[BranchPred.DPindex[0]][2]++;
            return;
        }
    }
    return;
}
void UpdateBranchBuffer(bool Branch, bool PCBranch, uint32_t BranchAddr, const char* Predictbit, const char* Counter) {
    if (Branch) {  // beq, bne
        if (BranchPred.AddressHit[1]) {  // PC found in BTB
            if (PCBranch) {  // Branch taken
                PBtaken(BranchPred.DP[BranchPred.DPindex[1]][1], Predictbit, Counter);
                counting.takenBranch++;
                if (BranchPred.Predict[1]) {  // Predicted branch taken, HIT
                    counting.PredictHitCount++;
                }
                else {  // Predicted branch not taken
                    ctrlSig.IFFlush = 1;
                }
            }
            else {  // Branch not taken
                PBnottaken(BranchPred.DP[BranchPred.DPindex[1]][1], Predictbit, Counter);
                counting.nottakenBranch++;
                if (BranchPred.Predict[1]) {  // Predicted branch taken
                    ctrlSig.IFFlush = 1;
                }
                else {  // Predicted branch not taken, HIT
                    counting.PredictHitCount++;
                }
            }
        }

        else {  // PC not found in BTB, PC = PC + 4
            BranchBufferWrite(BranchPred.instPC[1], BranchAddr, Predictbit);
            if (PCBranch) {  // Branch taken
                PBtaken(BranchPred.DP[BranchPred.DPindex[1]][1], Predictbit, Counter);
                counting.takenBranch++;
                ctrlSig.IFFlush = 1;
            }
            else {  // Branch not taken
                counting.nottakenBranch++;
            }
        }
    }
}
void BranchBufferWrite(uint32_t WritePC, uint32_t Address, const char* Predictbit) {  // Write PC and BranchAddr to BTB
    if (BranchPred.BTBsize >= BTBMAX) {  // BTB has no space
        uint32_t BTBmin = BranchPred.BTB[0][2];
        int BTBminindex;
        for (BranchPred.BTBindex[1] = 0; BranchPred.BTBindex[1] < BTBMAX; BranchPred.BTBindex[1]++) {
            if (BTBmin > BranchPred.BTB[BranchPred.BTBindex[1]][2]) {
                BTBmin = BranchPred.BTB[BranchPred.BTBindex[1]][2];
                BTBminindex = BranchPred.BTBindex[1];
            }
        }
        // Substitute low frequency used branch
        BranchPred.BTB[BTBminindex][0] = WritePC;
        BranchPred.BTB[BTBminindex][1] = Address;
        BranchPred.BTB[BTBminindex][2] = 0;
        BranchPred.BTBindex[1] = BTBminindex;
    }
    if (BranchPred.DPsize >= BTBMAX) {  // DP has no space
        uint32_t DPmin = BranchPred.DP[0][2];
        int DPminindex;
        for (BranchPred.DPindex[1] = 0; BranchPred.DPindex[1] < BTBMAX; BranchPred.DPindex[1]++) {
            if (DPmin > BranchPred.DP[BranchPred.DPindex[1]][2]) {
                DPmin = BranchPred.DP[BranchPred.DPindex[1]][2];
                DPminindex = BranchPred.DPindex[1];
            }
        }
        // Substitute low frequency used branch
        BranchPred.DP[DPminindex][0] = WritePC;

        if (*Predictbit == '2') {  // Two-bit predictor
            BranchPred.DP[DPminindex][1] = 1;
        }
        else {  // One-bit predictor
            BranchPred.DP[DPminindex][1] = 0;
        }
        BranchPred.DP[DPminindex][2] = 0;
        BranchPred.DPindex[1] = DPminindex;
        return;
    }
    BranchPred.BTB[BranchPred.BTBsize][0] = WritePC;
    BranchPred.BTB[BranchPred.BTBsize][1] = Address;
    BranchPred.DP[BranchPred.DPsize][0] = WritePC;
    BranchPred.BTBindex[1] = BranchPred.BTBsize;
    BranchPred.DPindex[1] = BranchPred.DPsize;
    BranchPred.BTBsize++;
    BranchPred.DPsize++;
    return;
}
void PBtaken(uint8_t Predbit, const char* Predictbit, const char* Counter) {  // Update prediction bit to taken
    switch (*Predictbit) {
        case '1' :  // One-bit predictor
            BranchPred.DP[BranchPred.DPindex[1]][1] =  1;  // PB = 1
            break;

        case '2' :  // Two-bit predictor
            if (*Counter == '1') {  // Saturating Counter
                if (Predbit < 3) {  // PB == 00 or 01 or 10
                    BranchPred.DP[BranchPred.DPindex[1]][1]++;  // PB++
                }
            }
            else {  // Hysteresis Counter
                if (Predbit == 1 || Predbit == 2 || Predbit == 3) {  // PB == 01 or 10 or 11
                    BranchPred.DP[BranchPred.DPindex[1]][1] =  3;  // PB = 11
                }
                else if (Predbit == 0) {  // PB == 00
                    BranchPred.DP[BranchPred.DPindex[1]][1] = 1;  // PB = 01
                }
                else {
                    fprintf(stderr, "ERROR: Prediction bit is wrong\n");
                    exit(EXIT_FAILURE);
                }
            }
            break;

        default :
            fprintf(stderr, "ERROR: Wrong prediction bit select number\n");
            exit(EXIT_FAILURE);
    }
    return;
}
void PBnottaken(uint8_t Predbit, const char* Predictbit, const char* Counter) {  // Update prediction bit to not taken
    switch (*Predictbit) {
        case '1' :  // One-bit predictor
            BranchPred.DP[BranchPred.DPindex[1]][1] =  0;  // PB = 0
            break;

        case '2' :  // Two-bit predictor
            if (*Counter == '1') {  // Saturating Counter
                if (Predbit > 0) {  // PB == 11 or 10 or 01
                    BranchPred.DP[BranchPred.DPindex[1]][1]--;  // PB--
                }
            }
            else {  // Hysteresis Counter
                if (Predbit == 0 || Predbit == 1 || Predbit == 2) {  // PB == 00 or 01 or 10
                    BranchPred.DP[BranchPred.DPindex[1]][1] =  0;  // PB = 00
                }
                else if (Predbit == 3) {  // PB == 00
                    BranchPred.DP[BranchPred.DPindex[1]][1] = 2;  // PB = 10
                }
                else {
                    fprintf(stderr, "ERROR: Prediction bit is wrong\n");
                    exit(EXIT_FAILURE);
                }
            }
            break;

        default :
            fprintf(stderr, "ERROR: Wrong prediction bit select number\n");
            exit(EXIT_FAILURE);
    }
    return;
}

// [Gshare branch predictor]
void GshareCheckBranch(uint32_t PCvalue, const char* Predictbit) {
    BranchPred.instPC[0] = PCvalue;
    if (BranchPred.BTBsize == 0) {  // BTB is empty
        BranchPred.AddressHit[0] = 0;  // Branch not predicted
        BranchPred.Predict[0] = 0;  // Predict branch not taken
        return;
    }

    // Find PC in BTB
    for (BranchPred.BTBindex[0] = 0; BranchPred.BTBindex[0] < BTBMAX; BranchPred.BTBindex[0]++) {
        if (BranchPred.BTB[BranchPred.BTBindex[0]][0] == PCvalue) {  // PC found in BTB
            BranchPred.AddressHit[0] = 1;  // Branch predicted
            BranchPred.BTB[BranchPred.BTBindex[0]][2]++;  // Frequency + 1
            break;  // Send out predicted PC
        }
    }

    // PC not found in BTB
    if (BranchPred.BTBindex[0] == BTBMAX) {
        BranchPred.AddressHit[0] = 0;  // Branch not predicted
        BranchPred.Predict[0] = 0;  // Predict branch not taken
        return;
    }

    // Find GLHR in BHT
    for (BranchPred.BHTindex[0] = 0; BranchPred.BHTindex[0] < BHTMAX; BranchPred.BHTindex[0]++) {
        if (BranchPred.BHT[BranchPred.BHTindex[0]][0] == BranchPred.IFBHTindex) {
            // IFBHT index found in BHT
            switch (*Predictbit) {
                case '1' :  // One-bit predictor
                    if (BranchPred.BHT[BranchPred.BHTindex[0]][1] == 1)  {
                        BranchPred.Predict[0] = 1;  // Predict branch taken
                    }
                    else {
                        BranchPred.Predict[0] = 0;  // Predict branch not taken
                    }
                    break;

                case '2' :  // Two-bit predictor
                    if (BranchPred.BHT[BranchPred.BHTindex[0]][1] == 2 || BranchPred.BHT[BranchPred.BHTindex[0]][1] == 3)  {
                        BranchPred.Predict[0] = 1;  // Predict branch taken
                    }
                    else {
                        BranchPred.Predict[0] = 0;  // Predict branch not taken
                    }
                    break;

                default :
                    fprintf(stderr, "ERROR: Wrong prediction bit select number\n");
                    exit(EXIT_FAILURE);
            }
            return;
        }
    }
    return;
}
void GshareUpdateBranchBuffer(bool Branch, bool PCBranch, uint32_t BranchAddr, const char* Predictbit, const char* Counter) {
    if (Branch) {  // beq, bne
        BranchPred.GLHR[3] = BranchPred.GLHR[2];
        BranchPred.GLHR[2] = BranchPred.GLHR[1];
        BranchPred.GLHR[1] = BranchPred.GLHR[0];
        if (BranchPred.AddressHit[1]) {  // PC found in BTB
            if (PCBranch) {  // Branch taken
                BranchPred.GLHR[0] = 1;
                GsharePBtaken(BranchPred.BHT[BranchPred.BHTindex[1]][1], Predictbit, Counter);
                counting.takenBranch++;
                if (BranchPred.Predict[1]) {  // Predicted branch taken, HIT
                    counting.PredictHitCount++;
                }
                else {  // Predicted branch not taken
                    ctrlSig.IFFlush = 1;
                }
            }
            else {  // Branch not taken
                BranchPred.GLHR[0] = 0;
                GsharePBnottaken(BranchPred.BHT[BranchPred.BHTindex[1]][1], Predictbit, Counter);
                counting.nottakenBranch++;
                if (BranchPred.Predict[1]) {  // Predicted branch taken
                    ctrlSig.IFFlush = 1;
                }
                else {  // Predict branch not taken, HIT
                    counting.PredictHitCount++;
                }
            }
        }

        else {  // PC not found in BTB, PC = PC + 4
            GshareBranchBufferWrite(BranchPred.instPC[1], BranchAddr);
            if (PCBranch) {  // Branch taken
                BranchPred.GLHR[0] = 1;
                GsharePBtaken(BranchPred.BHT[BranchPred.BHTindex[1]][1], Predictbit, Counter);
                counting.takenBranch++;
                ctrlSig.IFFlush = 1;
            }
            else {  // Branch not taken
                BranchPred.GLHR[0] = 0;
                counting.nottakenBranch++;
            }
        }
    }
}
void GshareBranchBufferWrite(uint32_t WritePC, uint32_t Address) {
    if (BranchPred.BTBsize >= BTBMAX) {  // BTB has no space
        uint32_t BTBmin = BranchPred.BTB[0][2];
        int BTBminindex;
        for (BranchPred.BTBindex[1] = 0; BranchPred.BTBindex[1] < BTBMAX; BranchPred.BTBindex[1]++) {
            if (BTBmin > BranchPred.BTB[BranchPred.BTBindex[1]][2]) {
                BTBmin = BranchPred.BTB[BranchPred.BTBindex[1]][2];
                BTBminindex = BranchPred.BTBindex[1];
            }
        }
        // Substitute low frequency used branch
        BranchPred.BTB[BTBminindex][0] = WritePC;
        BranchPred.BTB[BTBminindex][1] = Address;
        BranchPred.BTB[BTBminindex][2] = 0;
        BranchPred.BTBindex[1] = BTBminindex;
    }
    BranchPred.BTB[BranchPred.BTBsize][0] = WritePC;
    BranchPred.BTB[BranchPred.BTBsize][1] = Address;
    BranchPred.BTBindex[1] = BranchPred.BTBsize;
    BranchPred.BTBsize++;
    return;
}
void GsharePBtaken(uint8_t Predbit, const char* Predictbit, const char* Counter) {
    switch (*Predictbit) {
        case '1' :  // One-bit predictor
            BranchPred.BHT[BranchPred.BHTindex[1]][1] =  1;  // PB = 1
            break;

        case '2' :  // Two-bit predictor
            if (*Counter == '1') {  // Saturating Counter
                if (Predbit < 3) {  // PB == 00 or 01 or 10
                    BranchPred.BHT[BranchPred.BHTindex[1]][1]++;  // PB++
                }
            }
            else {  // Hysteresis Counter
                if (Predbit == 1 || Predbit == 2 || Predbit == 3) {  // PB == 01 or 10 or 11
                    BranchPred.BHT[BranchPred.BHTindex[1]][1] =  3;  // PB = 11
                }
                else if (Predbit == 0) {  // PB == 00
                    BranchPred.BHT[BranchPred.BHTindex[1]][1] = 1;  // PB = 01
                }
                else {
                    fprintf(stderr, "ERROR: Prediction bit is wrong\n");
                    exit(EXIT_FAILURE);
                }
            }
            break;

        default :
            fprintf(stderr, "ERROR: Wrong prediction bit select number\n");
            exit(EXIT_FAILURE);
    }
    return;
}
void GsharePBnottaken(uint8_t Predbit, const char* Predictbit, const char* Counter) {
    switch (*Predictbit) {
        case '1' :  // One-bit predictor
            BranchPred.BHT[BranchPred.BHTindex[1]][1] =  0;  // PB = 0
            break;

        case '2' :  // Two-bit predictor
            if (*Counter == '1') {  // Saturating Counter
                if (Predbit > 0) {  // PB == 11 or 10 or 01
                    BranchPred.BHT[BranchPred.BHTindex[1]][1]--;  // PB--
                }
            }
            else {  // Hysteresis Counter
                if (Predbit == 0 || Predbit == 1 || Predbit == 2) {  // PB == 00 or 01 or 10
                    BranchPred.BHT[BranchPred.BHTindex[1]][1] = 0;  // PB = 00
                }
                else if (Predbit == 3) {  // PB == 00
                    BranchPred.BHT[BranchPred.BHTindex[1]][1] = 2;  // PB = 10
                }
                else {
                    fprintf(stderr, "ERROR: Prediction bit is wrong\n");
                    exit(EXIT_FAILURE);
                }
            }
            break;

        default :
            fprintf(stderr, "ERROR: Wrong prediction bit select number\n");
            exit(EXIT_FAILURE);
    }
    return;
}

// [Local branch predictor]
bool LocalCheckLHR(uint32_t PCvalue) {
    BranchPred.instPC[0] = PCvalue;
    if (BranchPred.LHRsize == 0) {  // LHR is empty
        BranchPred.AddressHit[0] = 0;  // Branch not predicted
        BranchPred.Predict[0] = 0;  // Predict branch not taken
        return 0;
    }

    // Find PC in LHR
    for (BranchPred.LHRindex[0] = 0; BranchPred.LHRindex[0] < LHRMAX; BranchPred.LHRindex[0]++) {
        if (BranchPred.LHR[BranchPred.LHRindex[0]][0] == PCvalue) {
            BranchPred.LHR[BranchPred.LHRindex[0]][2]++;
            return 1;
        }
    }
    // PC not found in LHR
    BranchPred.AddressHit[0] = 0;  // Branch not predicted
    BranchPred.Predict[0] = 0;  // Predict branch not taken
    return 0;
}
void LocalCheckBranch(uint32_t PCvalue, const char* Predictbit) {
    if (BranchPred.BTBsize == 0) {  // BTB is empty
        BranchPred.AddressHit[0] = 0;  // Branch not predicted
        BranchPred.Predict[0] = 0;  // Predict branch not taken
        return;
    }

    // Find PC in BTB
    for (BranchPred.BTBindex[0] = 0; BranchPred.BTBindex[0] < BTBMAX; BranchPred.BTBindex[0]++) {
        if (BranchPred.BTB[BranchPred.BTBindex[0]][0] == PCvalue) {  // PC found in BTB
            BranchPred.AddressHit[0] = 1;  // Branch predicted
            BranchPred.BTB[BranchPred.BTBindex[0]][2]++;
            break;  // Send out predicted PC
        }
    }

    // PC not found in BTB
    if (BranchPred.BTBindex[0] == BTBMAX) {
        BranchPred.AddressHit[0] = 0;  // Branch not predicted
        BranchPred.Predict[0] = 0;  // Predict branch not taken
        return;
    }

    // Find LHR in BHT
    for (BranchPred.BHTindex[0] = 0; BranchPred.BHTindex[0] < BHTMAX; BranchPred.BHTindex[0]++) {
        if (BranchPred.BHT[BranchPred.BHTindex[0]][0] == BranchPred.LHR[BranchPred.LHRindex[0]][1]) {
            // IFBHT index found in BHT
            switch (*Predictbit) {
                case '1' :  // One-bit predictor
                    if (BranchPred.BHT[BranchPred.BHTindex[0]][1] == 1)  {
                        BranchPred.Predict[0] = 1;  // Predict branch taken
                    }
                    else {
                        BranchPred.Predict[0] = 0;  // Predict branch not taken
                    }
                    break;

                case '2' :  // Two-bit predictor
                    if (BranchPred.BHT[BranchPred.BHTindex[0]][1] == 2 || BranchPred.BHT[BranchPred.BHTindex[0]][1] == 3)  {
                        BranchPred.Predict[0] = 1;  // Predict branch taken
                    }
                    else {
                        BranchPred.Predict[0] = 0;  // Predict branch not taken
                    }
                    break;

                default :
                    fprintf(stderr, "ERROR: Wrong prediction bit select number\n");
                    exit(EXIT_FAILURE);
            }
            return;
        }
    }
    return;
}
void LocalUpdateBranchBuffer(bool Branch, bool PCBranch, uint32_t BranchAddr, const char* Predictbit, const char* Counter) {
    if (Branch) {  // beq, bne
        for (int j = 3; j >= 0; j--) {
            BranchPred.GLHR[j] = BranchPred.LHR[BranchPred.LHRindex[1]][1] >> j & 1;
        }
        BranchPred.GLHR[3] = BranchPred.GLHR[2];
        BranchPred.GLHR[2] = BranchPred.GLHR[1];
        BranchPred.GLHR[1] = BranchPred.GLHR[0];
        if (BranchPred.AddressHit[1]) {  // PC found in BTB
            if (PCBranch) {  // Branch taken
                BranchPred.GLHR[0] = 1;
                LocalPBtaken(BranchPred.BHT[BranchPred.BHTindex[1]][1], Predictbit, Counter);
                counting.takenBranch++;
                if (BranchPred.Predict[1]) {  // Predicted branch taken, HIT
                    counting.PredictHitCount++;
                }
                else {  // Predicted branch not taken
                    ctrlSig.IFFlush = 1;
                }
            }
            else {  // Branch not taken
                BranchPred.GLHR[0] = 0;
                LocalPBnottaken(BranchPred.BHT[BranchPred.BHTindex[1]][1], Predictbit, Counter);
                counting.nottakenBranch++;
                if (BranchPred.Predict[1]) {  // Predicted branch taken
                    ctrlSig.IFFlush = 1;
                }
                else {  // Predict branch not taken, HIT
                    counting.PredictHitCount++;
                }
            }
        }

        else {  // PC not found in BTB, PC = PC + 4
            LocalBranchBufferWrite(BranchPred.instPC[1], BranchAddr);
            if (PCBranch) {  // Branch taken
                BranchPred.GLHR[0] = 1;
                LocalPBtaken(BranchPred.BHT[BranchPred.BHTindex[1]][1], Predictbit, Counter);
                counting.takenBranch++;
                ctrlSig.IFFlush = 1;
            }
            else {  // Branch not taken
                BranchPred.GLHR[0] = 0;
                counting.nottakenBranch++;
            }
        }
        BranchPred.LHR[BranchPred.LHRindex[1]][1] = 8 * BranchPred.GLHR[3] + 4 * BranchPred.GLHR[2]
                                                    + 2 * BranchPred.GLHR[1] + BranchPred.GLHR[0];
    }
}
void LocalBranchBufferWrite(uint32_t WritePC, uint32_t Address) {
    if (BranchPred.BTBsize >= BTBMAX) {  // BTB has no space
        uint32_t BTBmin = BranchPred.BTB[0][2];
        int BTBminindex;
        for (BranchPred.BTBindex[1] = 0; BranchPred.BTBindex[1] < BTBMAX; BranchPred.BTBindex[1]++) {
            if (BTBmin > BranchPred.BTB[BranchPred.BTBindex[1]][2]) {
                BTBmin = BranchPred.BTB[BranchPred.BTBindex[1]][2];
                BTBminindex = BranchPred.BTBindex[1];
            }
        }
        // Substitute low frequency used branch
        BranchPred.BTB[BTBminindex][0] = WritePC;
        BranchPred.BTB[BTBminindex][1] = Address;
        BranchPred.BTB[BTBminindex][2] = 0;
        BranchPred.BTBindex[1] = BTBminindex;
    }
    if (BranchPred.LHRsize >= LHRMAX) {  // LHR has no space
        uint32_t LHRmin = BranchPred.LHR[0][2];
        int LHRminindex;
        for (BranchPred.LHRindex[1] = 0; BranchPred.LHRindex[1] < LHRMAX; BranchPred.LHRindex[1]++) {
            if (LHRmin > BranchPred.LHR[BranchPred.LHRindex[1]][2]) {
                LHRmin = BranchPred.LHR[BranchPred.LHRindex[1]][2];
                LHRminindex = BranchPred.LHRindex[1];
            }
        }
        // Substitute low frequency used branch
        BranchPred.LHR[LHRminindex][0] = WritePC;
        BranchPred.LHR[LHRminindex][1] = 0;
        BranchPred.LHR[LHRminindex][2] = 0;
        BranchPred.LHRindex[1] = LHRminindex;
        return;
    }
    BranchPred.BTB[BranchPred.BTBsize][0] = WritePC;
    BranchPred.BTB[BranchPred.BTBsize][1] = Address;
    BranchPred.LHR[BranchPred.LHRsize][0] = WritePC;
    BranchPred.BTBindex[1] = BranchPred.BTBsize;
    BranchPred.LHRindex[1] = BranchPred.LHRsize;
    BranchPred.BTBsize++;
    BranchPred.LHRsize++;
    return;
}
void LocalPBtaken(uint8_t Predbit, const char* Predictbit, const char* Counter) {
    switch (*Predictbit) {
        case '1' :  // One-bit predictor
            BranchPred.BHT[BranchPred.BHTindex[1]][1] =  1;  // PB = 1
            break;

        case '2' :  // Two-bit predictor
            if (*Counter == '1') {  // Saturating Counter
                if (Predbit < 3) {  // PB == 00 or 01 or 10
                    BranchPred.BHT[BranchPred.BHTindex[1]][1]++;  // PB++
                }
            }
            else {  // Hysteresis Counter
                if (Predbit == 1 || Predbit == 2 || Predbit == 3) {  // PB == 01 or 10 or 11
                    BranchPred.BHT[BranchPred.BHTindex[1]][1] =  3;  // PB = 11
                }
                else if (Predbit == 0) {  // PB == 00
                    BranchPred.BHT[BranchPred.BHTindex[1]][1] = 1;  // PB = 01
                }
                else {
                    fprintf(stderr, "ERROR: Prediction bit is wrong\n");
                    exit(EXIT_FAILURE);
                }
            }
            break;

        default :
            fprintf(stderr, "ERROR: Wrong prediction bit select number\n");
            exit(EXIT_FAILURE);
    }
    return;
}
void LocalPBnottaken(uint8_t Predbit, const char* Predictbit, const char* Counter) {
    switch (*Predictbit) {
        case '1' :  // One-bit predictor
            BranchPred.BHT[BranchPred.BHTindex[1]][1] =  0;  // PB = 0
            break;

        case '2' :  // Two-bit predictor
            if (*Counter == '1') {  // Saturating Counter
                if (Predbit > 0) {  // PB == 11 or 10 or 01
                    BranchPred.BHT[BranchPred.BHTindex[1]][1]--;  // PB--
                }
            }
            else {  // Hysteresis Counter
                if (Predbit == 0 || Predbit == 1 || Predbit == 2) {  // PB == 00 or 01 or 10
                    BranchPred.BHT[BranchPred.BHTindex[1]][1] = 0;  // PB = 00
                }
                else if (Predbit == 3) {  // PB == 00
                    BranchPred.BHT[BranchPred.BHTindex[1]][1] = 2;  // PB = 10
                }
                else {
                    fprintf(stderr, "ERROR: Prediction bit is wrong\n");
                    exit(EXIT_FAILURE);
                }
            }
            break;

        default :
            fprintf(stderr, "ERROR: Wrong prediction bit select number\n");
            exit(EXIT_FAILURE);
    }
    return;
}

// [Always taken predictor]
void AlwaysTakenCheckBranch(uint32_t PCvalue) {
    BranchPred.instPC[0] = PCvalue;
    if (BranchPred.BTBsize == 0) {  // BTB is empty
        BranchPred.AddressHit[0] = 0;  // Branch not predicted
        return;
    }

    // Find PC in BTB
    for (BranchPred.BTBindex[0] = 0; BranchPred.BTBindex[0] < BTBMAX; BranchPred.BTBindex[0]++) {
        if (BranchPred.BTB[BranchPred.BTBindex[0]][0] == PCvalue) {  // PC found in BTB
            BranchPred.AddressHit[0] = 1;  // Branch taken
            BranchPred.BTB[BranchPred.BTBindex[0]][2]++;
            break;  // Send out predicted PC
        }
    }

    // PC not found in BTB
    if (BranchPred.BTBindex[0] == BTBMAX) {
        BranchPred.AddressHit[0] = 0;  // Branch not predicted
        return;
    }
    return;
}

void AlwaysTakenUpdateBranchBuffer(bool Branch, bool PCBranch, uint32_t BranchAddr) {
    if (Branch) {  // beq, bne
        if (BranchPred.AddressHit[1]) {  // Predicted branch taken
            if (PCBranch) {  // Branch taken, HIT
                counting.takenBranch++;
                counting.PredictHitCount++;
            }
            else {  // Branch not taken
                ctrlSig.IFFlush = 1;
                counting.nottakenBranch++;
            }
        }

        else {  // PC not found in BTB, PC = PC + 4
            AlwaysTakenBranchBufferWrite(BranchPred.instPC[1], BranchAddr);
            if (PCBranch) {  // Branch taken
                counting.takenBranch++;
                ctrlSig.IFFlush = 1;
            }
            else {  // Branch not taken
                counting.nottakenBranch++;
            }
        }
    }
}
void AlwaysTakenBranchBufferWrite(uint32_t WritePC, uint32_t Address) {
    if (BranchPred.BTBsize >= BTBMAX) {  // BTB has no space
        uint32_t BTBmin = BranchPred.BTB[0][2];
        int BTBminindex;
        for (BranchPred.BTBindex[1] = 0; BranchPred.BTBindex[1] < BTBMAX; BranchPred.BTBindex[1]++) {
            if (BTBmin > BranchPred.BTB[BranchPred.BTBindex[1]][2]) {
                BTBmin = BranchPred.BTB[BranchPred.BTBindex[1]][2];
                BTBminindex = BranchPred.BTBindex[1];
            }
        }
        // Substitute low frequency used branch
        BranchPred.BTB[BTBminindex][0] = WritePC;
        BranchPred.BTB[BTBminindex][1] = Address;
        BranchPred.BTB[BTBminindex][2] = 0;
        BranchPred.BTBindex[1] = BTBminindex;
    }
    BranchPred.BTB[BranchPred.BTBsize][0] = WritePC;
    BranchPred.BTB[BranchPred.BTBsize][1] = Address;
    BranchPred.BTBindex[1] = BranchPred.BTBsize;
    BranchPred.BTBsize++;
    return;
}

// [Always not taken predictor]
void BranchAlwaysnotTaken(bool Branch, bool PCBranch) {
    if (Branch) {  // beq, bne
        if (PCBranch) {  // Branch taken
            ctrlSig.IFFlush = 1;
            counting.takenBranch++;
        }
        else {  // Branch not taken, HIT
            counting.PredictHitCount++;
            counting.nottakenBranch++;
        }
    }
}

// [Backward Taken, Forward Not Taken]
void BTFNTCheckBranch(uint32_t PCvalue) {
    BranchPred.instPC[0] = PCvalue;
    if (BranchPred.BTBsize == 0) {  // BTB is empty
        BranchPred.AddressHit[0] = 0;  // Branch not predicted
        BranchPred.Predict[0] = 0;  // Branch not taken
        return;
    }

    // Find PC in BTB
    for (BranchPred.BTBindex[0] = 0; BranchPred.BTBindex[0] < BTBMAX; BranchPred.BTBindex[0]++) {
        if (BranchPred.BTB[BranchPred.BTBindex[0]][0] == PCvalue) {  // PC found in BTB
            BranchPred.AddressHit[0] = 1;  // Branch predicted
            if (BranchPred.BTB[BranchPred.BTBindex[0]][1] < PCvalue) {  // Branch target direction is backward
                BranchPred.Predict[0] = 1;  // Branch taken
            }
            else {  // Branch target direction is forward
                BranchPred.Predict[0] = 0;  // Branch not taken
            }
            BranchPred.BTB[BranchPred.BTBindex[0]][2]++;
            break;  // Send out predicted PC
        }
    }

    // PC not found in BTB
    if (BranchPred.BTBindex[0] == BTBMAX) {
        BranchPred.AddressHit[0] = 0;  // Branch not predicted
        BranchPred.Predict[0] = 0;  // Branch taken
        return;
    }
    return;
}
void BTFNTUpdateBranchBuffer(bool Branch, bool PCBranch, uint32_t BranchAddr) {
    if (Branch) {  // beq, bne
        if (BranchPred.AddressHit[1]) {  // PC found in BTB
            if (PCBranch) {  // Branch taken
                counting.takenBranch++;
                if (BranchPred.Predict[1]) {  // Predicted branch taken, HIT
                    counting.PredictHitCount++;
                }
                else {  // Predicted branch not taken
                    ctrlSig.IFFlush = 1;
                }
            }
            else {  // Branch not taken
                counting.nottakenBranch++;
                if (BranchPred.Predict[1]) {  // Predicted branch taken
                    ctrlSig.IFFlush = 1;
                }
                else {  // Predicted branch not taken, HIT
                    counting.PredictHitCount++;
                }
            }
        }

        else {  // PC not found in BTB, PC = PC + 4
            BTFNTBranchBufferWrite(BranchPred.instPC[1], BranchAddr);
            if (PCBranch) {  // Branch taken
                counting.takenBranch++;
                ctrlSig.IFFlush = 1;
            }
            else {  // Branch not taken
                counting.nottakenBranch++;
            }
        }
    }
}
void BTFNTBranchBufferWrite(uint32_t WritePC, uint32_t Address) {
    if (BranchPred.BTBsize >= BTBMAX) {  // BTB has no space
        uint32_t BTBmin = BranchPred.BTB[0][2];
        int BTBminindex;
        for (BranchPred.BTBindex[1] = 0; BranchPred.BTBindex[1] < BTBMAX; BranchPred.BTBindex[1]++) {
            if (BTBmin > BranchPred.BTB[BranchPred.BTBindex[1]][2]) {
                BTBmin = BranchPred.BTB[BranchPred.BTBindex[1]][2];
                BTBminindex = BranchPred.BTBindex[1];
            }
        }
        // Substitute low frequency used branch
        BranchPred.BTB[BTBminindex][0] = WritePC;
        BranchPred.BTB[BTBminindex][1] = Address;
        BranchPred.BTB[BTBminindex][2] = 0;
        BranchPred.BTBindex[1] = BTBminindex;
    }
    BranchPred.BTB[BranchPred.BTBsize][0] = WritePC;
    BranchPred.BTB[BranchPred.BTBsize][1] = Address;
    BranchPred.BTBindex[1] = BranchPred.BTBsize;
    BranchPred.BTBsize++;
    return;
}

// [ALU]
uint32_t ALU(uint32_t input1, uint32_t input2, char ALUSig) {
    uint32_t ALUresult = 0;
    switch (ALUSig) {
        case '+' :  // add
            ALUresult = input1 + input2;
            break;

        case '-' :  // subtract
            ALUresult = input1 - input2;
            break;

        case '&' :  // AND
            ALUresult = input1 & input2;
            break;

        case '|' :  // OR
            ALUresult = input1 | input2;
            break;

        case '~' :  // NOR
            ALUresult = ~(input1 | input2);
            break;
        
        case '<' :  // set less than signed
            if ( (input1 & 0x80000000) == (input2 & 0x80000000) ) {  // same sign
                if (input1 < input2) {
                    ALUresult = 1;
                }
                else {
                    ALUresult = 0;
                }
            }
            else if ( ((input1 & 0x80000000) == 0x80000000) && ((input2 & 0x80000000) == 0x00000000) ) {  // input1 < 0. input2 > 0
                ALUresult = 1;
            }
            else {
                ALUresult = 0;
            }
            break;

        case '>' :  // set less than unsigned
            if ((input1 & 0x7fffffff) < (input2 & 0x7fffffff)) {
                ALUresult = 1;
            }
            else {
                ALUresult = 0;
            }
            break;

        case '{' :  // sll
            ALUresult = input2 << input1;
            break;

        case '}' :  // srl
            ALUresult = input2 >> input1;
            break;

        case '\0' :  // nop
            break;

        default :
            fprintf(stderr, "ERROR: Instruction has wrong opcode or funct.\n");
            exit(EXIT_FAILURE);

    }
    if ( ((input1 & 0x80000000) == (input2 & 0x80000000)) && ((ALUresult & 0x80000000) != (input1 & 0x80000000)) ) {
        // same sign input, different sign output
        ALUctrlSig.ArthOvfl = 1;
    }
    return ALUresult;
}

// [MUX with 2 input]
uint32_t MUX(uint32_t input1, uint32_t input2, bool signal) {
    if (signal) {
        return input2;
    }
    else {
        return input1;
    }
}

// [MUX with 3 input]
uint32_t MUX_3(uint32_t input1, uint32_t input2, uint32_t input3, const bool signal[]) {
    if (signal[1] == 0 && signal[0] == 0) {
        return input1;
    }
    else if (signal[1] == 0 && signal[0] == 1) {
        return input2;
    }
    else {
        return input3;
    }
}

// [MUX with 4 input]
uint32_t MUX_4(uint32_t input1, uint32_t input2, uint32_t input3, uint32_t input4, const bool signal[]) {
    if (signal[1] == 0 && signal[0] == 0) {
        return input1;
    }
    else if (signal[1] == 0 && signal[0] == 1) {
        return input2;
    }
    else if (signal[1] == 1 && signal[0] == 0){
        return input3;
    }
    else {
        return input4;
    }
}

/*============================Control units============================*/

// (Control unit)
void CtrlUnit(uint8_t opcode, uint8_t funct) {
    memset(&ctrlSig, 0, sizeof(CONTROL_SIGNAL));
    switch (opcode) {  // considered don't care as 0 or not written
        case 0x0 :  // R-format
            ctrlSig.RegDst[1] = 0; ctrlSig.RegDst[0] = 1;  // Write register = rd
            ctrlSig.ALUSrc = 0;  // ALU input2 = rt
            ctrlSig.MemtoReg[1] = 0; ctrlSig.MemtoReg[0] = 0;  // Write data = ALU result
            ctrlSig.RegWrite = 1;  // GPR write enabled
            ctrlSig.MemRead = 0;  // Memory read disabled
            ctrlSig.MemWrite = 0;  // Memory write enabled
            ctrlSig.BNE = 0;  // opcode != bne
            ctrlSig.BEQ = 0;  // opcode != beq
            ctrlSig.ALUOp = '0';  // select operation according to funct
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            if (funct == 0x08) {  // jr
                ctrlSig.RegWrite = 0;
                ctrlSig.Jump[1] = 1; ctrlSig.Jump[0] = 0;
            }
            else if (funct == 0x9) {  // jalr
                ctrlSig.MemtoReg[1] = 1; ctrlSig.MemtoReg[0] = 0;  // Write data = PC + 8
                ctrlSig.Jump[1] = 1; ctrlSig.Jump[0] = 0;
            }
            else if (funct == 0x00 || funct == 0x02) {  // sll, srl
                ctrlSig.Shift = 1;  // ALU input1 = shamt
            }
            break;

        case 0x8 :  // addi
            ctrlSig.RegDst[1] = 0; ctrlSig.RegDst[0] = 0;
            ctrlSig.ALUSrc = 1;
            ctrlSig.MemtoReg[1] = 0; ctrlSig.MemtoReg[0] = 0;
            ctrlSig.RegWrite = 1;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 0;
            ctrlSig.SignZero = 0;
            ctrlSig.BNE = 0;
            ctrlSig.BEQ = 0;
            ctrlSig.ALUOp = '+';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            break;

        case 0x9 :  // addiu
            ctrlSig.RegDst[1] = 0; ctrlSig.RegDst[0] = 0;
            ctrlSig.ALUSrc = 1;
            ctrlSig.MemtoReg[1] = 0; ctrlSig.MemtoReg[0] = 0;
            ctrlSig.RegWrite = 1;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 0;
            ctrlSig.SignZero = 0;
            ctrlSig.BNE = 0;
            ctrlSig.BEQ = 0;
            ctrlSig.ALUOp = '+';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            break;

        case 0xc :  // andi
            ctrlSig.RegDst[1] = 0; ctrlSig.RegDst[0] = 0;
            ctrlSig.ALUSrc = 1;
            ctrlSig.MemtoReg[1] = 0; ctrlSig.MemtoReg[0] = 0;
            ctrlSig.RegWrite = 1;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 0;
            ctrlSig.SignZero = 1;
            ctrlSig.BNE = 0;
            ctrlSig.BEQ = 0;
            ctrlSig.ALUOp = '&';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            break;

        case 0x4 :  // beq
            ctrlSig.ALUSrc = 0;
            ctrlSig.RegWrite = 0;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 0;
            ctrlSig.BNE = 0;
            ctrlSig.BEQ = 1;
            ctrlSig.ALUOp = 'B';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            break;

        case 0x5 :  // bne
            ctrlSig.ALUSrc = 0;
            ctrlSig.RegWrite = 0;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 0;
            ctrlSig.BNE = 1;
            ctrlSig.BEQ = 0;
            ctrlSig.ALUOp = 'B';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            break;

        case 0x2 :  // j
            ctrlSig.RegWrite = 0;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 0;
            ctrlSig.BNE = 0;
            ctrlSig.BEQ = 0;
            ctrlSig.ALUOp = '+';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 1;
            break;

        case 0x3 :  // jal
            ctrlSig.RegWrite = 1;
            ctrlSig.RegDst[1] = 1; ctrlSig.RegDst[0] = 0;
            ctrlSig.MemtoReg[1] = 1; ctrlSig.MemtoReg[0] = 0;
            ctrlSig.ALUOp = '+';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 1;
            break;
        
        case 0xf :  // lui
            ctrlSig.RegDst[1] = 0; ctrlSig.RegDst[0] = 0;
            ctrlSig.ALUSrc = 1;
            ctrlSig.MemtoReg[1] = 1; ctrlSig.MemtoReg[0] = 1;
            ctrlSig.RegWrite = 1;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 0;
            ctrlSig.BNE = 0;
            ctrlSig.BEQ = 0;
            ctrlSig.ALUOp = '+';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            break;

        case 0x23 :  // lw
            ctrlSig.RegDst[1] = 0; ctrlSig.RegDst[0] = 0;
            ctrlSig.ALUSrc = 1;
            ctrlSig.MemtoReg[1] = 0; ctrlSig.MemtoReg[0] = 1;
            ctrlSig.RegWrite = 1;
            ctrlSig.MemRead = 1;
            ctrlSig.MemWrite = 0;
            ctrlSig.SignZero = 0;
            ctrlSig.BNE = 0;
            ctrlSig.BEQ = 0;
            ctrlSig.ALUOp = '+';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            break;

        case 0xd :  // ori
            ctrlSig.RegDst[1] = 0; ctrlSig.RegDst[0] = 0;
            ctrlSig.ALUSrc = 1;
            ctrlSig.MemtoReg[1] = 0; ctrlSig.MemtoReg[0] = 0;
            ctrlSig.RegWrite = 1;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 0;
            ctrlSig.SignZero = 1;
            ctrlSig.BNE = 0;
            ctrlSig.BEQ = 0;
            ctrlSig.ALUOp = '|';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            break;

        case 0xa :  // slti
            ctrlSig.RegDst[1] = 0; ctrlSig.RegDst[0] = 0;
            ctrlSig.ALUSrc = 1;
            ctrlSig.MemtoReg[1] = 0; ctrlSig.MemtoReg[0] = 0;
            ctrlSig.RegWrite = 1;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 0;
            ctrlSig.SignZero = 0;
            ctrlSig.BNE = 0;
            ctrlSig.BEQ = 0;
            ctrlSig.ALUOp = '<';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            break;

        case 0xb :  // sltiu
            ctrlSig.RegDst[1] = 0; ctrlSig.RegDst[0] = 0;
            ctrlSig.ALUSrc = 1;
            ctrlSig.MemtoReg[1] = 0; ctrlSig.MemtoReg[0] = 0;
            ctrlSig.RegWrite = 1;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 0;
            ctrlSig.SignZero = 0;
            ctrlSig.BNE = 0;
            ctrlSig.BEQ = 0;
            ctrlSig.ALUOp = '>';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            break;

        case 0x2B :  // sw
            ctrlSig.ALUSrc = 1;
            ctrlSig.RegWrite = 0;
            ctrlSig.MemRead = 0;
            ctrlSig.MemWrite = 1;
            ctrlSig.SignZero = 0;
            ctrlSig.BNE = 0;
            ctrlSig.BEQ = 0;
            ctrlSig.ALUOp = '+';
            ctrlSig.Jump[1] = 0; ctrlSig.Jump[0] = 0;
            break;

        default :
            fprintf(stderr, "ERROR: Instruction has wrong opcode.\n");
            exit(EXIT_FAILURE);
    }
    return;
}

// (ALU control unit)
void ALUCtrlUnit(uint8_t funct, char ALUOp) {
    memset(&ALUctrlSig, 0, sizeof(ALU_CONTROL_SIGNAL));
    switch (ALUOp) {  // 0, +, &, B, |, <, \0
        case '0' :  // select operation according to funct
            ALUctrlSig.ALUSig = Rformat(funct);
            return;

        case '+' :
            ALUctrlSig.ALUSig = '+';
            return;

        case '&' :
            ALUctrlSig.ALUSig = '&';
            return;

        case 'B' :  // select bcond generation function
            ALUctrlSig.ALUSig = '-';
            return;

        case '|' :
            ALUctrlSig.ALUSig = '|';
            return;

        case '<' :
            ALUctrlSig.ALUSig = '<';
            return;

        case '\0' :  // nop
            return;

        default :
            fprintf(stderr, "ERROR: Instruction has wrong funct.\n");
            exit(EXIT_FAILURE);
    }
}

// (Fowarding Unit)
void ForwardUnit(uint8_t IDEXrt, uint8_t IDEXrs, uint8_t EXMEMWritereg, uint8_t MEMWBWritereg,
                 bool EXMEMRegWrite, bool MEMWBRegWrite, const bool EXMEMMemtoReg[]) {
    memset(&fwrdSig, 0, sizeof(FORWARD_SIGNAL));
    // EX hazard
    if (EXMEMRegWrite && (EXMEMWritereg != 0) && (EXMEMWritereg == IDEXrs)) {
        fwrdSig.ForwardA[1] = 1; fwrdSig.ForwardA[0] = 0;  // ForwardA = 10
        if (EXMEMMemtoReg[1] & EXMEMMemtoReg[0]) {
            fwrdSig.EXMEMupperimmA = 1;
        }
    }
    if (EXMEMRegWrite && (EXMEMWritereg != 0) && (EXMEMWritereg == IDEXrt)) {
        fwrdSig.ForwardB[1] = 1; fwrdSig.ForwardB[0] = 0;  // ForwardB = 10
        if (EXMEMMemtoReg[1] & EXMEMMemtoReg[0]) {
            fwrdSig.EXMEMupperimmB = 1;
        }
    }

    // MEM hazard
    if (MEMWBRegWrite && (MEMWBWritereg != 0) &&
        !( EXMEMRegWrite && (EXMEMWritereg != 0) && (EXMEMWritereg == IDEXrs) ) &&
        (MEMWBWritereg == IDEXrs)) {
        fwrdSig.ForwardA[1] = 0; fwrdSig.ForwardA[0] = 1;  // ForwardA = 01
    }
    if (MEMWBRegWrite && (MEMWBWritereg != 0) &&
        !( EXMEMRegWrite && (EXMEMWritereg != 0) && (EXMEMWritereg == IDEXrt) ) &&
        (MEMWBWritereg == IDEXrt)) {
        fwrdSig.ForwardB[1] = 0; fwrdSig.ForwardB[0] = 1;  // ForwardB = 01
    }
    return;
}

void IDForwardUnit(uint8_t IFIDrt, uint8_t IFIDrs, uint8_t IDEXWritereg, uint8_t EXMEMWritereg, uint8_t MEMWBWritereg,
                   bool IDEXRegWrite, bool EXMEMRegWrite, bool MEMWBRegWrite, const bool IDEXMemtoReg[], const bool EXMEMMemtoReg[]) {
    memset(&idfwrdSig, 0, sizeof(ID_FORWARD_SIGNAL));
    // ID hazard
    if (IDEXRegWrite && (IDEXWritereg != 0) && (IDEXWritereg == IFIDrs) && (IDEXMemtoReg[1] & IDEXMemtoReg[0])) {
        idfwrdSig.IDForwardA[1] = 1; idfwrdSig.IDForwardA[0] = 1;  // IDForwardA = 11
    }
    if (IDEXRegWrite && (IDEXWritereg != 0) && (IDEXWritereg == IFIDrt) && (IDEXMemtoReg[1] & IDEXMemtoReg[0])) {
        idfwrdSig.IDForwardB[1] = 1; idfwrdSig.IDForwardB[0] = 1;  // IDForwardB = 11
    }

    // EX hazard
    if (EXMEMRegWrite && (EXMEMWritereg != 0) &&
        !( IDEXRegWrite && (IDEXWritereg != 0) && (IDEXWritereg == IFIDrs) ) &&
        (EXMEMWritereg == IFIDrs)) {
        idfwrdSig.IDForwardA[1] = 1; idfwrdSig.IDForwardA[0] = 0;  // IDForwardA = 10
        if (EXMEMMemtoReg[1] & EXMEMMemtoReg[0]) {
            idfwrdSig.ID_EXMEMupperimmA = 1;
        }
    }
    if (EXMEMRegWrite && (EXMEMWritereg != 0) &&
        !( IDEXRegWrite && (IDEXWritereg != 0) && (IDEXWritereg == IFIDrt) ) &&
        (EXMEMWritereg == IFIDrt)) {
        idfwrdSig.IDForwardB[1] = 1; idfwrdSig.IDForwardB[0] = 0;  // IDForwardB = 10
        if (EXMEMMemtoReg[1] & EXMEMMemtoReg[0]) {
            idfwrdSig.ID_EXMEMupperimmB = 1;
        }
    }

    // MEM hazard
    if (MEMWBRegWrite && (MEMWBWritereg != 0) &&
        !( EXMEMRegWrite && (EXMEMWritereg != 0) &&
        !( IDEXRegWrite && (IDEXWritereg != 0) && (IDEXWritereg == IFIDrs) ) && (EXMEMWritereg == IFIDrs) )
        && (MEMWBWritereg == IFIDrs)) {
        idfwrdSig.IDForwardA[1] = 0; idfwrdSig.IDForwardA[0] = 1;  // IDForwardA = 01

    }
    if (MEMWBRegWrite && (MEMWBWritereg != 0) &&
        !( EXMEMRegWrite && (EXMEMWritereg != 0) &&
           !( IDEXRegWrite && (IDEXWritereg != 0) && (IDEXWritereg == IFIDrt) ) && (EXMEMWritereg == IFIDrt) )
        && (MEMWBWritereg == IFIDrt)) {
        idfwrdSig.IDForwardB[1] = 0; idfwrdSig.IDForwardB[0] = 1;  // IDForwardB = 01
    }
}

void MEMForwardUnit(uint8_t EXMEMrt, uint8_t MEMWBWritereg, bool EXMEMMemWrite, bool MEMWBMemRead) {
    memset(&memfwrdSig, 0, sizeof(MEM_FORWARD_SIGNAL));
    if (MEMWBMemRead && EXMEMMemWrite && (MEMWBWritereg != 0) && (MEMWBWritereg == EXMEMrt)) {
        memfwrdSig.MEMForward = 1;
    }
}

// (Hazard detection unit)
void HazardDetectUnit(uint8_t IFIDrs, uint8_t IFIDrt, uint8_t IDEXrt, uint8_t IDEXWritereg, uint8_t EXMEMWritereg,
                    bool IDEXMemRead, bool IDEXRegWrite, bool EXMEMMemRead, bool Branch, bool Jump) {
    memset(&hzrddetectSig, 0, sizeof(HAZARD_DETECTION_SIGNAL));
    // Load-use hazard
    if (IDEXMemRead && ((IDEXrt == IFIDrs) || (IDEXrt == IFIDrt))) {
        hzrddetectSig.PCnotWrite = 1;
        hzrddetectSig.IFIDnotWrite = 1;
        hzrddetectSig.ControlNOP = 1;
    }
    // IDEX branch(jump) hazard
    if (IDEXRegWrite && (Branch | Jump) && ((IDEXWritereg == IFIDrs) || (IDEXWritereg == IFIDrt))) {
        hzrddetectSig.PCnotWrite = 1;
        hzrddetectSig.IFIDnotWrite = 1;
        hzrddetectSig.BTBnotWrite = 1;
        hzrddetectSig.ControlNOP = 1;
    }
    // EXMEM branch(jump) hazard (for lw instruction)
    if (EXMEMMemRead && (Branch | Jump) && ((EXMEMWritereg == IFIDrs) || (EXMEMWritereg == IFIDrt))) {
        hzrddetectSig.PCnotWrite = 1;
        hzrddetectSig.IFIDnotWrite = 1;
        hzrddetectSig.BTBnotWrite = 1;
        hzrddetectSig.ControlNOP = 1;
    }
    if (hzrddetectSig.ControlNOP == 1){
        counting.stall++;
    }
    return;
}

/*============================Select ALU operation============================*/

char Rformat(uint8_t funct) {  // select ALU operation by funct (R-format)
    switch (funct) {
        case 0x20 :  // add
            return '+';

        case 0x21 :  // addu
            return '+';

        case 0x24 :  // and
            return '&';

        case 0x08 :  // jr
            return '+';
        
        case 0x9 :  // jalr
            return '+';

        case 0x27 :  // nor
            return '~';

        case 0x25 :  // or
            return '|';

        case 0x2a :  // slt
            return '<';

        case 0x2b :  // sltu
            return '>';

        case 0x00 :  // sll
            return '{';

        case 0x02 :  // srl
            return '}';

        case 0x22 :  // sub
            return '-';

        case 0x23 :  // subu
            return '-';

        default:
            fprintf(stderr, "ERROR: Instruction has wrong funct.\n");
            exit(EXIT_FAILURE);
    }
}

/*============================Exception============================*/

void OverflowException() {  // check overflow in signed instruction
    fprintf(stderr, "ERROR: Arithmetic overflow occured.\n");
    exit(EXIT_FAILURE);
}
