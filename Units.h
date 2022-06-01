//
// Created by SNMac on 2022/05/13.
//

#ifndef CAMP_PROJECT3_UNITS_H
#define CAMP_PROJECT3_UNITS_H

#include <stdint.h>
#include <stdbool.h>
#define BTBMAX 16
#define BHTMAX 16  // 2^GLHR
#define LHRMAX 4
#define CACHELINESIZE 64


/* Instruction */
typedef struct _INSTRUCTION {
    uint32_t address;
    uint8_t opcode;
    uint8_t rs;
    uint8_t rt;
    uint8_t rd;
    uint8_t shamt;
    uint16_t imm;
    uint8_t funct;
}INSTRUCTION;

/* Program Counter */
typedef struct _PROGRAM_COUNTER {
    bool valid;
    uint32_t PC;
}PROGRAM_COUNTER;

/* Pipelines units */
typedef struct _IFID {  // IF/ID pipeline
    bool valid;
    uint32_t instruction;
    uint32_t PCadd4;
}IFID;
typedef struct _IDEX {  // ID/EX pipeline
    bool valid;
    uint32_t PCadd8;
    uint8_t shamt;
    uint8_t funct;
    uint32_t Rrs;
    uint32_t Rrt;
    uint32_t extimm;
    uint32_t upperimm;
    uint8_t rs;
    uint8_t rt;
    uint8_t rd;
    bool Shift, ALUSrc, RegDst[2], MemWrite, MemRead, MemtoReg[2], RegWrite;
    char ALUOp;
}IDEX;
typedef struct _EXMEM {  // EX/MEM pipeline
    bool valid;
    uint32_t PCadd8;
    uint32_t ALUresult;
    uint32_t ForwardBMUX;
    uint32_t upperimm;
    uint8_t Writereg;
    uint8_t rt;
    bool MemWrite, MemRead, MemtoReg[2], RegWrite;
}EXMEM;
typedef struct _MEMWB {  // MEM/WB pipeline
    bool valid;
    uint32_t PCadd8;
    uint32_t Readdata;
    uint32_t ALUresult;
    uint32_t upperimm;
    uint8_t Writereg;
    bool MemtoReg[2], RegWrite, MemRead;
}MEMWB;

/* Control signals */
typedef struct _CONTROL_SIGNAL {  // Control signals
    bool ALUSrc, RegWrite, MemRead, MemWrite, SignZero, BEQ, BNE,
        Shift, IFFlush, Jump[2], RegDst[2], MemtoReg[2];
    char ALUOp;
}CONTROL_SIGNAL;
typedef struct _ALU_CONTROL_SIGNAL {  // ALU control signals
    bool ArthOvfl;
    char ALUSig;
}ALU_CONTROL_SIGNAL;
typedef struct _FORWARD_SIGNAL {  // Forward unit signals
    bool ForwardA[2], ForwardB[2], EXMEMupperimmA, EXMEMupperimmB;
}FORWARD_SIGNAL;
typedef struct _ID_FORWARD_SIGNAL {  // Branch forward unit signals
    bool IDForwardA[2], IDForwardB[2], ID_EXMEMupperimmA, ID_EXMEMupperimmB;
}ID_FORWARD_SIGNAL;
typedef struct _MEM_FORWARD_SIGNAL {
    bool MEMForward;
}MEM_FORWARD_SIGNAL;
typedef struct _HAZARD_DETECTION_SIGNAL {  // Hazard detection unit signals
    bool PCnotWrite, IFIDnotWrite, BTBnotWrite, ControlNOP;
}HAZARD_DETECTION_SIGNAL;

/* Branch predictor */
typedef struct _BRANCH_PREDICT {  // Branch prediction unit
    uint32_t instPC[2];  // [0] : now PC, [1] : previous PC
    bool Predict[2];  // [0] : now PC, [1] : previous PC
    bool AddressHit[2];  // [0] : now PC, [1] : previous PC
    int BTBindex[2];  // [0] : now PC, [1] : previous PC
    int BTBsize;
    int DPindex[2];
    int DPsize;
    int BHTindex[2];
    int LHRindex[2];
    int LHRsize;
    int IFBHTindex;
    uint32_t BTB[BTBMAX][3];  // Branch Target Buffer
    // [i][0] = Branch instruction PC, [i][1] = Branch Target, [i][2] = Frequency (How many times used)
    uint32_t DP[BTBMAX][3];  // Direction Predictor
    // [i][0] = Branch instruction PC, [i][1] = Prediction bits, [i][2] = Frequency (How many times used)
    uint8_t BHT[BHTMAX][2];  // Branch History table (2^GHRMAX size)
    // [i][0] = 0000 ~ 1111, [i][1] = Prediction bits
    uint32_t LHR[LHRMAX][3];  // Local History Register (4 branches, 4 bits)
    // [i][0] = Branch instruction PC, [i][1] = Branch history (4 bits), [i][2] = Frequency (How many times used)
    bool GLHR[4];  // Global / Local History Register (4 bits)
}BRANCH_PREDICT;

/* Cache unit */
typedef struct _CACHE {
    uint32_t Cache[][3];
}CACHE;

/* Data units */
void InstDecoder(INSTRUCTION *inst, uint32_t instruction);  // Instruction decoder
uint32_t InstMem(uint32_t Readaddr);  // Instruction memory
uint32_t* RegsRead(uint8_t Readreg1, uint8_t Readreg2);  // Registers (ID)
void RegsWrite(uint8_t Writereg, uint32_t Writedata, bool RegWrite);  // Registers (WB)
uint32_t DataMem(uint32_t Addr, uint32_t Writedata, bool MemRead, bool MemWrite);  // Data memory

/* Branch Predictor */
// [One-level branch predictor]
void CheckBranch(uint32_t PCvalue, const char* Predictbit);  // Check branch in IF stage
void UpdateBranchBuffer(bool Branch, bool PCBranch, uint32_t BranchAddr, const char* Predictbit, const char* Counter);  // Update BTB
void BranchBufferWrite(uint32_t WritePC, uint32_t Address, const char* Predictbit);  // Write BranchAddr to BTB
void PBtaken(uint8_t Predbit, const char* Predictbit, const char* Counter);  // Update Prediction bit to taken
void PBnottaken(uint8_t Predbit, const char* Predictbit, const char* Counter);  // Update Prediction bit to not taken
// [Gshare branch predictor]
void GshareCheckBranch(uint32_t PCvalue, const char* Predictbit);  // Check branch in IF stage
void GshareUpdateBranchBuffer(bool Branch, bool PCBranch, uint32_t BranchAddr, const char* Predictbit, const char* Counter);  // Update BTB
void GshareBranchBufferWrite(uint32_t WritePC, uint32_t Address);  // Write BranchAddr to BTB
void GsharePBtaken(uint8_t Predbit, const char* Predictbit, const char* Counter);  // Update Prediction bit to taken
void GsharePBnottaken(uint8_t Predbit, const char* Predictbit, const char* Counter);  // Update Prediction bit to not taken
// [Local branch predictor]
bool LocalCheckLHR(uint32_t PCvalue);
void LocalCheckBranch(uint32_t PCvalue, const char* Predictbit);  // Check branch in IF stage
void LocalUpdateBranchBuffer(bool Branch, bool PCBranch, uint32_t BranchAddr, const char* Predictbit, const char* Counter);
void LocalBranchBufferWrite(uint32_t WritePC, uint32_t Address);  // Write BranchAddr to BTB
void LocalPBtaken(uint8_t Predbit, const char* Predictbit, const char* Counter);  // Update Prediction bit to taken
void LocalPBnottaken(uint8_t Predbit, const char* Predictbit, const char* Counter);  // Update Prediction bit to not taken
// [Always taken predictor]
void AlwaysTakenCheckBranch(uint32_t PCvalue);  // Check branch in IF stage
void AlwaysTakenUpdateBranchBuffer(bool Branch, bool PCBranch, uint32_t BranchAddr);  // Update BTB
void AlwaysTakenBranchBufferWrite(uint32_t WritePC, uint32_t Address);  // Write BranchAddr to BTB
// [Always not taken predictor]
void BranchAlwaysnotTaken(bool Branch, bool PCBranch);  // Check branch in ID stage
// [Backward Taken, Forward Not Taken predictor]
void BTFNTCheckBranch(uint32_t PCvalue);  // Check branch in IF stage
void BTFNTUpdateBranchBuffer(bool Branch, bool PCBranch, uint32_t BranchAddr);  // Update BTB
void BTFNTBranchBufferWrite(uint32_t WritePC, uint32_t Address);  // Write BranchAddr to BTB

/* Cache memory */
bool CheckCache();

uint32_t ALU(uint32_t input1, uint32_t input2, char ALUSig);  // ALU
uint32_t MUX(uint32_t input1, uint32_t input2, bool signal);  // signal == 0) input1, 1) input2
uint32_t MUX_3(uint32_t input1, uint32_t input2, uint32_t input3, const bool signal[]);  // signal == 0) input1, 1) input2, 2) input3
uint32_t MUX_4(uint32_t input1, uint32_t input2, uint32_t input3, uint32_t input4, const bool signal[]);  // signal == 0) input1, 1) input2, 2) input3, 3) input4

/* Control units */
void CtrlUnit(uint8_t opcode, uint8_t funct);  // Control unit
void ALUCtrlUnit(uint8_t funct, char ALUOp);  // ALU control unit
void ForwardUnit(uint8_t IDEXrt, uint8_t IDEXrs, uint8_t EXMEMWritereg, uint8_t MEMWBWritereg,
                 bool EXMEMRegWrite, bool MEMWBRegWrite, const bool EXMEMMemtoReg[]);  // Forward unit (EX, MEM hazard)
void IDForwardUnit(uint8_t IFIDrt, uint8_t IFIDrs, uint8_t IDEXWritereg, uint8_t EXMEMWritereg, uint8_t MEMWBWritereg,
                   bool IDEXRegWrite, bool EXMEMRegWrite, bool MEMWBRegWrite, const bool IDEXMemtoReg[], const bool EXMEMMemtoReg[]);  // Branch Forward unit (ID hazard by beq, bne)
void MEMForwardUnit(uint8_t EXMEMrt, uint8_t MEMWBWritereg, bool EXMEMMemWrite, bool MEMWBMemRead);
void HazardDetectUnit(uint8_t IFIDrs, uint8_t IFIDrt, uint8_t IDEXrt, uint8_t IDEXWritereg, uint8_t EXMEMWritereg,
                    bool IDEXMemRead, bool IDEXRegWrite, bool EXMEMMemRead, bool Branch, bool Jump);  // Hazard detection unit

/* Select ALU operation */
char Rformat(uint8_t funct);  // select ALU operation by funct (R-format)

/* Overflow exception */
void OverflowException();  // Overflow exception

#endif //CAMP_PROJECT3_UNITS_H
