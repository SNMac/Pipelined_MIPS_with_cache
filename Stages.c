//
// Created by SNMac on 2022/06/01.
//

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "Stages.h"
#include "main.h"
#include "Units.h"
#include "Debug.h"

uint32_t MemtoRegMUX;

// from main.c
extern uint32_t Memory[0x400000];
extern uint32_t R[32];
extern COUNTING counting;

// from Units.c
extern PROGRAM_COUNTER PC;
extern IFID ifid[2];
extern IDEX idex[2];
extern EXMEM exmem[2];
extern MEMWB memwb[2];
extern CONTROL_SIGNAL ctrlSig;
extern ALU_CONTROL_SIGNAL ALUctrlSig;
extern BRANCH_PREDICT BranchPred;
extern FORWARD_SIGNAL fwrdSig;
extern ID_FORWARD_SIGNAL idfwrdSig;
extern MEM_FORWARD_SIGNAL memfwrdSig;
extern HAZARD_DETECTION_SIGNAL hzrddetectSig;

// from Debug.c
extern DEBUGIF debugif;
extern DEBUGID debugid[2];
extern DEBUGEX debugex[2];
extern DEBUGMEM debugmem[2];
extern DEBUGWB debugwb[2];

/*============================Stages============================*/

// Instruction Fetch (One Level Predictor)
void OnelevelIF(const int* Predictbit) {
    debugif.IFPC = PC.PC; debugid[0].valid = PC.valid;
    if (PC.PC == 0xffffffff) {
        ifid[0].valid = 0;
        PC.valid = 0;
        return;
    }
    // Fetch instruction
    uint32_t instruction = InstMem(PC.PC);

    // Check if PC is branch instruction
    CheckBranch(PC.PC, Predictbit);

    // Save data to pipeline
    ifid[0].instruction = instruction; ifid[0].PCadd4 = PC.PC + 4;

    // For visible state
    debugif.IFinst = instruction;
    debugif.Predict = BranchPred.Predict[0];
    debugif.AddressHit = BranchPred.AddressHit[0];
    return;
}
// Instruction Decode (One Level Predictor)
void OnelevelID(const int* Predictbit, const char* Counter) {
    idex[0].valid = ifid[1].valid;
    debugex[0].valid = ifid[1].valid;

    // Decode instruction
    INSTRUCTION inst;
    InstDecoder(&inst, ifid[1].instruction);

    // Set control signals
    CtrlUnit(inst.opcode, inst.funct);

    // Load-use, branch hazard detect
    bool Branch = ctrlSig.BEQ | ctrlSig.BNE;
    uint8_t IDEXWritereg = MUX_3(idex[1].rt, idex[1].rd, 31, idex[1].RegDst);
    HazardDetectUnit(inst.rs, inst.rt, idex[1].rt, IDEXWritereg, exmem[1].Writereg,
                     idex[1].MemRead, idex[1].RegWrite, exmem[1].MemRead, Branch, ctrlSig.Jump[1]);
    if (hzrddetectSig.ControlNOP) {  // adding nop
        memset(&ctrlSig, 0, sizeof(CONTROL_SIGNAL));
        Branch = 0;
    }

    // Avoid ID-WB hazard
    if(memwb[1].valid) {
        MemtoRegMUX = MUX_4(memwb[1].ALUresult, memwb[1].Readdata, memwb[1].PCadd8, memwb[1].upperimm, memwb[1].MemtoReg);
        RegsWrite(memwb[1].Writereg, MemtoRegMUX, memwb[1].RegWrite);
    }

    // Register fetch
    uint32_t* Regs_return = RegsRead(inst.rs, inst.rt);

    // Check branch forwarding
    IDForwardUnit(inst.rt, inst.rs, IDEXWritereg,exmem[1].Writereg, memwb[1].Writereg,
                  idex[1].RegWrite, exmem[1].RegWrite, memwb[1].RegWrite, idex[1].MemtoReg, exmem[1].MemtoReg);
    uint32_t IDForwardAupperimmMUX = MUX(exmem[1].ALUresult, exmem[1].upperimm, fwrdSig.EXMEMupperimmA);
    uint32_t IDForwardBupperimmMUX = MUX(exmem[1].ALUresult, exmem[1].upperimm, fwrdSig.EXMEMupperimmB);
    uint32_t IDForwardAMUX= MUX_4(Regs_return[0], MemtoRegMUX ,IDForwardAupperimmMUX, idex[1].upperimm, idfwrdSig.IDForwardA);
    uint32_t IDForwardBMUX= MUX_4(Regs_return[1], MemtoRegMUX ,IDForwardBupperimmMUX, idex[1].upperimm, idfwrdSig.IDForwardB);

    // Comparate two operands
    bool Equal = (IDForwardAMUX == IDForwardBMUX) ? 1 : 0;
    bool PCBranch = (ctrlSig.BNE & !Equal) | (ctrlSig.BEQ & Equal);

    // Extending immediate
    int32_t signimm = (int16_t) inst.imm;
    uint32_t zeroimm = (uint16_t) inst.imm;
    uint32_t extimm = MUX(signimm, zeroimm, ctrlSig.SignZero);

    // Address calculate
    uint32_t BranchAddr = ifid[1].PCadd4 + (signimm << 2);
    uint32_t JumpAddr = (ifid[1].PCadd4 & 0xf0000000) | (inst.address << 2);

    // Update branch result to BTB
    debugid[1].PB = BranchPred.DP[BranchPred.DPindex[1]][1];
    if (!hzrddetectSig.BTBnotWrite) {
        UpdateBranchBuffer(Branch, PCBranch, BranchAddr, Predictbit, Counter);
    }

    // Select PC address
    bool PCtarget = BranchPred.AddressHit[0] & BranchPred.Predict[0];
    bool BranchHit = !(BranchPred.Predict[1] ^ PCBranch);
    bool IDBrPC = !BranchPred.Predict[1] & PCBranch;
    uint32_t IFIDPCMUX = MUX(ifid[1].PCadd4, PC.PC + 4, BranchHit);
    uint32_t PCBranchMUX = MUX(IFIDPCMUX, BranchAddr, IDBrPC);
    uint32_t JumpMUX = MUX_3(PCBranchMUX, JumpAddr, IDForwardAMUX, ctrlSig.Jump);
    uint32_t PredictMUX = MUX(JumpMUX, BranchPred.BTB[BranchPred.BTBindex[0]][1], PCtarget);
    if (PC.valid & !(hzrddetectSig.PCnotWrite)) {  // PC write disabled
        PC.PC = PredictMUX;
    }

    if (!(ifid[1].valid)) {  // Pipeline is invalid
        return;
    }

    // Save data to pipeline
    idex[0].PCadd8 = ifid[1].PCadd4 + 4;
    idex[0].Rrs = Regs_return[0]; idex[0].Rrt = Regs_return[1];
    idex[0].extimm = extimm; idex[0].upperimm = zeroimm << 16;
    idex[0].funct = inst.funct; idex[0].shamt = inst.shamt;
    idex[0].rs = inst.rs; idex[0].rt = inst.rt; idex[0].rd = inst.rd;

    // Save control signals to pipeline
    idex[0].Shift = ctrlSig.Shift; idex[0].ALUSrc = ctrlSig.ALUSrc;
    idex[0].RegDst[0] = ctrlSig.RegDst[0]; idex[0].RegDst[1] = ctrlSig.RegDst[1];
    idex[0].MemWrite = ctrlSig.MemWrite; idex[0].MemRead = ctrlSig.MemRead;
    idex[0].MemtoReg[0] = ctrlSig.MemtoReg[0]; idex[0].MemtoReg[1] = ctrlSig.MemtoReg[1];
    idex[0].RegWrite = ctrlSig.RegWrite; idex[0].ALUOp = ctrlSig.ALUOp;

    // Flushing IF instruction
    if (ctrlSig.IFFlush) {
        ifid[0].instruction = 0;
    }

    // For visible state
    debugid[1].inst = inst;
    debugid[1].Branch = Branch; debugid[1].PCBranch = PCBranch;
    debugid[1].Predict = BranchPred.Predict[1];
    debugid[1].AddressHit = BranchPred.AddressHit[1];
    return;
}

// Instruction Fetch (Gshare Predictor)
void GshareIF(const int* Predictbit) {
    debugif.IFPC = PC.PC; debugid[0].valid = PC.valid;
    if (PC.PC == 0xffffffff) {
        ifid[0].valid = 0;
        PC.valid = 0;
        return;
    }

    // Fetch instruction
    uint32_t instruction = InstMem(PC.PC);

    // Check if PC is branch instruction
    BranchPred.IFBHTindex = makeGHRindex(BranchPred.GLHR, PC.PC);
    GshareCheckBranch(PC.PC, Predictbit);

    // Save data to pipeline
    ifid[0].instruction = instruction; ifid[0].PCadd4 = PC.PC + 4;

    // For visible state
    debugif.IFinst = instruction;
    debugif.Predict = BranchPred.Predict[0];
    debugif.AddressHit = BranchPred.AddressHit[0];
    return;
}
// Instruction Decode (Gshare Predictor)
void GshareID(const int* Predictbit, const char* Counter) {
    idex[0].valid = ifid[1].valid;
    debugex[0].valid = ifid[1].valid;

    // Decode instruction
    INSTRUCTION inst;
    InstDecoder(&inst, ifid[1].instruction);

    // Set control signals
    CtrlUnit(inst.opcode, inst.funct);

    // Load-use, branch hazard detect
    bool Branch = ctrlSig.BEQ | ctrlSig.BNE;
    uint8_t IDEXWritereg = MUX_3(idex[1].rt, idex[1].rd, 31, idex[1].RegDst);
    HazardDetectUnit(inst.rs, inst.rt, idex[1].rt, IDEXWritereg, exmem[1].Writereg,
                     idex[1].MemRead, idex[1].RegWrite, exmem[1].MemRead, Branch, ctrlSig.Jump[1]);
    if (hzrddetectSig.ControlNOP) {  // adding nop
        memset(&ctrlSig, 0, sizeof(CONTROL_SIGNAL));
        Branch = 0;
    }

    // Avoid ID-WB hazard
    if(memwb[1].valid) {
        MemtoRegMUX = MUX_4(memwb[1].ALUresult, memwb[1].Readdata, memwb[1].PCadd8, memwb[1].upperimm, memwb[1].MemtoReg);
        RegsWrite(memwb[1].Writereg, MemtoRegMUX, memwb[1].RegWrite);
    }

    // Register fetch
    uint32_t* Regs_return = RegsRead(inst.rs, inst.rt);

    // Check branch forwarding
    IDForwardUnit(inst.rt, inst.rs, IDEXWritereg,exmem[1].Writereg, memwb[1].Writereg,
                  idex[1].RegWrite, exmem[1].RegWrite, memwb[1].RegWrite, idex[1].MemtoReg, exmem[1].MemtoReg);
    uint32_t IDForwardAupperimmMUX = MUX(exmem[1].ALUresult, exmem[1].upperimm, fwrdSig.EXMEMupperimmA);
    uint32_t IDForwardBupperimmMUX = MUX(exmem[1].ALUresult, exmem[1].upperimm, fwrdSig.EXMEMupperimmB);
    uint32_t IDForwardAMUX= MUX_4(Regs_return[0], MemtoRegMUX ,IDForwardAupperimmMUX, idex[1].upperimm, idfwrdSig.IDForwardA);
    uint32_t IDForwardBMUX= MUX_4(Regs_return[1], MemtoRegMUX ,IDForwardBupperimmMUX, idex[1].upperimm, idfwrdSig.IDForwardB);

    // Comparate two operands
    bool Equal = (IDForwardAMUX == IDForwardBMUX) ? 1 : 0;
    bool PCBranch = (ctrlSig.BNE & !Equal) | (ctrlSig.BEQ & Equal);

    // Extending immediate
    int32_t signimm = (int16_t) inst.imm;
    uint32_t zeroimm = (uint16_t) inst.imm;
    uint32_t extimm = MUX(signimm, zeroimm, ctrlSig.SignZero);

    // Address calculate
    uint32_t BranchAddr = ifid[1].PCadd4 + (signimm << 2);
    uint32_t JumpAddr = (ifid[1].PCadd4 & 0xf0000000) | (inst.address << 2);

    // Update branch result to BTB
    debugid[1].PB = BranchPred.BHT[BranchPred.BHTindex[1]][1];
    if (!hzrddetectSig.BTBnotWrite) {
        GshareUpdateBranchBuffer(Branch, PCBranch, BranchAddr, Predictbit, Counter);
    }


    // Select PC address
    bool PCtarget = BranchPred.AddressHit[0] & BranchPred.Predict[0];
    bool BranchHit = !(BranchPred.Predict[1] ^ PCBranch);
    bool IDBrPC = !BranchPred.Predict[1] & PCBranch;
    uint32_t IFIDPCMUX = MUX(ifid[1].PCadd4, PC.PC + 4, BranchHit);
    uint32_t PCBranchMUX = MUX(IFIDPCMUX, BranchAddr, IDBrPC);
    uint32_t JumpMUX = MUX_3(PCBranchMUX, JumpAddr, IDForwardAMUX, ctrlSig.Jump);
    uint32_t PredictMUX = MUX(JumpMUX, BranchPred.BTB[BranchPred.BTBindex[0]][1], PCtarget);
    if (PC.valid & !(hzrddetectSig.PCnotWrite)) {  // PC write disabled
        PC.PC = PredictMUX;
    }

    if (!(ifid[1].valid)) {  // Pipeline is invalid
        return;
    }

    // Save data to pipeline
    idex[0].PCadd8 = ifid[1].PCadd4 + 4;
    idex[0].Rrs = Regs_return[0]; idex[0].Rrt = Regs_return[1];
    idex[0].extimm = extimm; idex[0].upperimm = zeroimm << 16;
    idex[0].funct = inst.funct; idex[0].shamt = inst.shamt;
    idex[0].rs = inst.rs; idex[0].rt = inst.rt; idex[0].rd = inst.rd;

    // Save control signals to pipeline
    idex[0].Shift = ctrlSig.Shift; idex[0].ALUSrc = ctrlSig.ALUSrc;
    idex[0].RegDst[0] = ctrlSig.RegDst[0]; idex[0].RegDst[1] = ctrlSig.RegDst[1];
    idex[0].MemWrite = ctrlSig.MemWrite; idex[0].MemRead = ctrlSig.MemRead;
    idex[0].MemtoReg[0] = ctrlSig.MemtoReg[0]; idex[0].MemtoReg[1] = ctrlSig.MemtoReg[1];
    idex[0].RegWrite = ctrlSig.RegWrite; idex[0].ALUOp = ctrlSig.ALUOp;

    // Flushing IF instruction
    if (ctrlSig.IFFlush) {
        ifid[0].instruction = 0;
    }

    // For visible state
    debugid[1].inst = inst;
    debugid[1].Branch = Branch; debugid[1].PCBranch = PCBranch;
    debugid[1].Predict = BranchPred.Predict[1];
    debugid[1].AddressHit = BranchPred.AddressHit[1];
    return;
}

// Instruction Fetch (Local Predictor)
void LocalIF(const int* Predictbit) {
    debugif.IFPC = PC.PC; debugid[0].valid = PC.valid;
    if (PC.PC == 0xffffffff) {
        ifid[0].valid = 0;
        PC.valid = 0;
        return;
    }

    // Fetch instruction
    uint32_t instruction = InstMem(PC.PC);

    // Check if PC is branch instruction
    if (LocalCheckLHR(PC.PC)) {
        LocalCheckBranch(PC.PC, Predictbit);
    }

    // Save data to pipeline
    ifid[0].instruction = instruction; ifid[0].PCadd4 = PC.PC + 4;

    // For visible state
    debugif.IFinst = instruction;
    debugif.Predict = BranchPred.Predict[0];
    debugif.AddressHit = BranchPred.AddressHit[0];
    return;
}
// Instruction Decode (Local Predictor)
void LocalID(const int* Predictbit, const char* Counter) {
    idex[0].valid = ifid[1].valid;
    debugex[0].valid = ifid[1].valid;

    // Decode instruction
    INSTRUCTION inst;
    InstDecoder(&inst, ifid[1].instruction);

    // Set control signals
    CtrlUnit(inst.opcode, inst.funct);

    // Load-use, branch hazard detect
    bool Branch = ctrlSig.BEQ | ctrlSig.BNE;
    uint8_t IDEXWritereg = MUX_3(idex[1].rt, idex[1].rd, 31, idex[1].RegDst);
    HazardDetectUnit(inst.rs, inst.rt, idex[1].rt, IDEXWritereg, exmem[1].Writereg,
                     idex[1].MemRead, idex[1].RegWrite, exmem[1].MemRead, Branch, ctrlSig.Jump[1]);
    if (hzrddetectSig.ControlNOP) {  // adding nop
        memset(&ctrlSig, 0, sizeof(CONTROL_SIGNAL));
        Branch = 0;
    }

    // Avoid ID-WB hazard
    if(memwb[1].valid) {
        MemtoRegMUX = MUX_4(memwb[1].ALUresult, memwb[1].Readdata, memwb[1].PCadd8, memwb[1].upperimm, memwb[1].MemtoReg);
        RegsWrite(memwb[1].Writereg, MemtoRegMUX, memwb[1].RegWrite);
    }

    // Register fetch
    uint32_t* Regs_return = RegsRead(inst.rs, inst.rt);

    // Check branch forwarding
    IDForwardUnit(inst.rt, inst.rs, IDEXWritereg,exmem[1].Writereg, memwb[1].Writereg,
                  idex[1].RegWrite, exmem[1].RegWrite, memwb[1].RegWrite, idex[1].MemtoReg, exmem[1].MemtoReg);
    uint32_t IDForwardAupperimmMUX = MUX(exmem[1].ALUresult, exmem[1].upperimm, fwrdSig.EXMEMupperimmA);
    uint32_t IDForwardBupperimmMUX = MUX(exmem[1].ALUresult, exmem[1].upperimm, fwrdSig.EXMEMupperimmB);
    uint32_t IDForwardAMUX= MUX_4(Regs_return[0], MemtoRegMUX ,IDForwardAupperimmMUX, idex[1].upperimm, idfwrdSig.IDForwardA);
    uint32_t IDForwardBMUX= MUX_4(Regs_return[1], MemtoRegMUX ,IDForwardBupperimmMUX, idex[1].upperimm, idfwrdSig.IDForwardB);

    // Comparate two operands
    bool Equal = (IDForwardAMUX == IDForwardBMUX) ? 1 : 0;
    bool PCBranch = (ctrlSig.BNE & !Equal) | (ctrlSig.BEQ & Equal);

    // Extending immediate
    int32_t signimm = (int16_t) inst.imm;
    uint32_t zeroimm = (uint16_t) inst.imm;
    uint32_t extimm = MUX(signimm, zeroimm, ctrlSig.SignZero);

    // Address calculate
    uint32_t BranchAddr = ifid[1].PCadd4 + (signimm << 2);
    uint32_t JumpAddr = (ifid[1].PCadd4 & 0xf0000000) | (inst.address << 2);

    // Update branch result to BTB
    debugid[1].PB = BranchPred.BHT[BranchPred.BHTindex[1]][1];
    if (!hzrddetectSig.BTBnotWrite) {
        LocalUpdateBranchBuffer(Branch, PCBranch, BranchAddr, Predictbit, Counter);
    }


    // Select PC address
    bool PCtarget = BranchPred.AddressHit[0] & BranchPred.Predict[0];
    bool BranchHit = !(BranchPred.Predict[1] ^ PCBranch);
    bool IDBrPC = !BranchPred.Predict[1] & PCBranch;
    uint32_t IFIDPCMUX = MUX(ifid[1].PCadd4, PC.PC + 4, BranchHit);
    uint32_t PCBranchMUX = MUX(IFIDPCMUX, BranchAddr, IDBrPC);
    uint32_t JumpMUX = MUX_3(PCBranchMUX, JumpAddr, IDForwardAMUX, ctrlSig.Jump);
    uint32_t PredictMUX = MUX(JumpMUX, BranchPred.BTB[BranchPred.BTBindex[0]][1], PCtarget);
    if (PC.valid & !(hzrddetectSig.PCnotWrite)) {  // PC write disabled
        PC.PC = PredictMUX;
    }

    if (!(ifid[1].valid)) {  // Pipeline is invalid
        return;
    }

    // Save data to pipeline
    idex[0].PCadd8 = ifid[1].PCadd4 + 4;
    idex[0].Rrs = Regs_return[0]; idex[0].Rrt = Regs_return[1];
    idex[0].extimm = extimm; idex[0].upperimm = zeroimm << 16;
    idex[0].funct = inst.funct; idex[0].shamt = inst.shamt;
    idex[0].rs = inst.rs; idex[0].rt = inst.rt; idex[0].rd = inst.rd;

    // Save control signals to pipeline
    idex[0].Shift = ctrlSig.Shift; idex[0].ALUSrc = ctrlSig.ALUSrc;
    idex[0].RegDst[0] = ctrlSig.RegDst[0]; idex[0].RegDst[1] = ctrlSig.RegDst[1];
    idex[0].MemWrite = ctrlSig.MemWrite; idex[0].MemRead = ctrlSig.MemRead;
    idex[0].MemtoReg[0] = ctrlSig.MemtoReg[0]; idex[0].MemtoReg[1] = ctrlSig.MemtoReg[1];
    idex[0].RegWrite = ctrlSig.RegWrite; idex[0].ALUOp = ctrlSig.ALUOp;

    // Flushing IF instruction
    if (ctrlSig.IFFlush) {
        ifid[0].instruction = 0;
    }

    // For visible state
    debugid[1].inst = inst;
    debugid[1].Branch = Branch; debugid[1].PCBranch = PCBranch;
    debugid[1].Predict = BranchPred.Predict[1];
    debugid[1].AddressHit = BranchPred.AddressHit[1];
    return;
}

// Instruction Fetch (Always taken predictor)
void AlwaysTakenIF(void) {
    debugif.IFPC = PC.PC; debugid[0].valid = PC.valid;
    if (PC.PC == 0xffffffff) {
        ifid[0].valid = 0;
        PC.valid = 0;
        return;
    }
    // Fetch instruction
    uint32_t instruction = InstMem(PC.PC);

    // Check if PC is branch instruction
    AlwaysTakenCheckBranch(PC.PC);

    // Save data to pipeline
    ifid[0].instruction = instruction; ifid[0].PCadd4 = PC.PC + 4;

    // For visible state
    debugif.IFinst = instruction;
    debugif.AddressHit = BranchPred.AddressHit[0];
    return;
}
// Instruction Decode (Always taken)
void AlwaysTakenID(void) {
    idex[0].valid = ifid[1].valid;
    debugex[0].valid = ifid[1].valid;

    // Decode instruction
    INSTRUCTION inst;
    InstDecoder(&inst, ifid[1].instruction);

    // Set control signals
    CtrlUnit(inst.opcode, inst.funct);

    // Load-use, branch hazard detect
    bool Branch = ctrlSig.BEQ | ctrlSig.BNE;
    uint8_t IDEXWritereg = MUX_3(idex[1].rt, idex[1].rd, 31, idex[1].RegDst);
    HazardDetectUnit(inst.rs, inst.rt, idex[1].rt, IDEXWritereg, exmem[1].Writereg,
                     idex[1].MemRead, idex[1].RegWrite, exmem[1].MemRead, Branch, ctrlSig.Jump[1]);
    if (hzrddetectSig.ControlNOP) {  // adding nop
        memset(&ctrlSig, 0, sizeof(CONTROL_SIGNAL));
        Branch = 0;
    }

    // Avoid ID-WB hazard
    if(memwb[1].valid) {
        MemtoRegMUX = MUX_4(memwb[1].ALUresult, memwb[1].Readdata, memwb[1].PCadd8, memwb[1].upperimm, memwb[1].MemtoReg);
        RegsWrite(memwb[1].Writereg, MemtoRegMUX, memwb[1].RegWrite);
    }

    // Register fetch
    uint32_t* Regs_return = RegsRead(inst.rs, inst.rt);

    // Check branch forwarding
    IDForwardUnit(inst.rt, inst.rs, IDEXWritereg,exmem[1].Writereg, memwb[1].Writereg,
                  idex[1].RegWrite, exmem[1].RegWrite, memwb[1].RegWrite, idex[1].MemtoReg, exmem[1].MemtoReg);
    uint32_t IDForwardAupperimmMUX = MUX(exmem[1].ALUresult, exmem[1].upperimm, fwrdSig.EXMEMupperimmA);
    uint32_t IDForwardBupperimmMUX = MUX(exmem[1].ALUresult, exmem[1].upperimm, fwrdSig.EXMEMupperimmB);
    uint32_t IDForwardAMUX= MUX_4(Regs_return[0], MemtoRegMUX ,IDForwardAupperimmMUX, idex[1].upperimm, idfwrdSig.IDForwardA);
    uint32_t IDForwardBMUX= MUX_4(Regs_return[1], MemtoRegMUX ,IDForwardBupperimmMUX, idex[1].upperimm, idfwrdSig.IDForwardB);

    // Comparate two operands
    bool Equal = (IDForwardAMUX == IDForwardBMUX) ? 1 : 0;
    bool PCBranch = (ctrlSig.BNE & !Equal) | (ctrlSig.BEQ & Equal);

    // Extending immediate
    int32_t signimm = (int16_t) inst.imm;
    uint32_t zeroimm = (uint16_t) inst.imm;
    uint32_t extimm = MUX(signimm, zeroimm, ctrlSig.SignZero);

    // Address calculate
    uint32_t BranchAddr = ifid[1].PCadd4 + (signimm << 2);
    uint32_t JumpAddr = (ifid[1].PCadd4 & 0xf0000000) | (inst.address << 2);

    // Update branch result to BTB
    if (!hzrddetectSig.BTBnotWrite) {
        AlwaysTakenUpdateBranchBuffer(Branch, PCBranch, BranchAddr);
    }

    // Select PC address
    bool PCtarget = BranchPred.AddressHit[0];
    bool BranchHit = !(BranchPred.AddressHit[1] ^ PCBranch);
    bool IDBrPC = !BranchPred.AddressHit[1] & PCBranch;
    uint32_t IFIDPCMUX = MUX(ifid[1].PCadd4, PC.PC + 4, BranchHit);
    uint32_t PCBranchMUX = MUX(IFIDPCMUX, BranchAddr, IDBrPC);
    uint32_t JumpMUX = MUX_3(PCBranchMUX, JumpAddr, IDForwardAMUX, ctrlSig.Jump);
    uint32_t PredictMUX = MUX(JumpMUX, BranchPred.BTB[BranchPred.BTBindex[0]][1], PCtarget);
    if (PC.valid & !(hzrddetectSig.PCnotWrite)) {  // PC write disabled
        PC.PC = PredictMUX;
    }

    if (!(ifid[1].valid)) {  // Pipeline is invalid
        return;
    }

    // Save data to pipeline
    idex[0].PCadd8 = ifid[1].PCadd4 + 4;
    idex[0].Rrs = Regs_return[0]; idex[0].Rrt = Regs_return[1];
    idex[0].extimm = extimm; idex[0].upperimm = zeroimm << 16;
    idex[0].funct = inst.funct; idex[0].shamt = inst.shamt;
    idex[0].rs = inst.rs; idex[0].rt = inst.rt; idex[0].rd = inst.rd;

    // Save control signals to pipeline
    idex[0].Shift = ctrlSig.Shift; idex[0].ALUSrc = ctrlSig.ALUSrc;
    idex[0].RegDst[0] = ctrlSig.RegDst[0]; idex[0].RegDst[1] = ctrlSig.RegDst[1];
    idex[0].MemWrite = ctrlSig.MemWrite; idex[0].MemRead = ctrlSig.MemRead;
    idex[0].MemtoReg[0] = ctrlSig.MemtoReg[0]; idex[0].MemtoReg[1] = ctrlSig.MemtoReg[1];
    idex[0].RegWrite = ctrlSig.RegWrite; idex[0].ALUOp = ctrlSig.ALUOp;

    // Flushing IF instruction
    if (ctrlSig.IFFlush) {
        ifid[0].instruction = 0;
    }

    // For visible state
    debugid[1].inst = inst;
    debugid[1].Branch = Branch; debugid[1].PCBranch = PCBranch;
    debugid[1].AddressHit = BranchPred.AddressHit[1];
    return;
}

// Instruction Fetch (Always not taken)
void AlwaysnotTakenIF(void) {
    debugif.IFPC = PC.PC; debugid[0].valid = PC.valid;
    if (PC.PC == 0xffffffff) {
        ifid[0].valid = 0;
        PC.valid = 0;
        return;
    }
    // Fetch instruction
    uint32_t instruction = InstMem(PC.PC);

    // Predict branch is always not taken
    BranchPred.Predict[0] = 0;

    // Save data to pipeline
    ifid[0].instruction = instruction; ifid[0].PCadd4 = PC.PC + 4;

    // For visible state
    debugif.IFinst = instruction;
    debugif.Predict = BranchPred.Predict[0];
    return;
}
// Instruction Decode (Always not taken)
void AlwaysnotTakenID() {
    idex[0].valid = ifid[1].valid;
    debugex[0].valid = ifid[1].valid;

    // Decode instruction
    INSTRUCTION inst;
    InstDecoder(&inst, ifid[1].instruction);

    // Set control signals
    CtrlUnit(inst.opcode, inst.funct);

    // Load-use, branch hazard detect
    bool Branch = ctrlSig.BEQ | ctrlSig.BNE;
    uint8_t IDEXWritereg = MUX_3(idex[1].rt, idex[1].rd, 31, idex[1].RegDst);
    HazardDetectUnit(inst.rs, inst.rt, idex[1].rt, IDEXWritereg, exmem[1].Writereg,
                     idex[1].MemRead, idex[1].RegWrite, exmem[1].MemRead, Branch, ctrlSig.Jump[1]);
    if (hzrddetectSig.ControlNOP) {  // adding nop
        memset(&ctrlSig, 0, sizeof(CONTROL_SIGNAL));
        Branch = 0;
    }

    // Avoid ID-WB hazard
    if(memwb[1].valid) {
        MemtoRegMUX = MUX_4(memwb[1].ALUresult, memwb[1].Readdata, memwb[1].PCadd8, memwb[1].upperimm, memwb[1].MemtoReg);
        RegsWrite(memwb[1].Writereg, MemtoRegMUX, memwb[1].RegWrite);
    }

    // Register fetch
    uint32_t* Regs_return = RegsRead(inst.rs, inst.rt);

    // Check branch forwarding
    IDForwardUnit(inst.rt, inst.rs, IDEXWritereg,exmem[1].Writereg, memwb[1].Writereg,
                  idex[1].RegWrite, exmem[1].RegWrite, memwb[1].RegWrite, idex[1].MemtoReg, exmem[1].MemtoReg);
    uint32_t IDForwardAupperimmMUX = MUX(exmem[1].ALUresult, exmem[1].upperimm, fwrdSig.EXMEMupperimmA);
    uint32_t IDForwardBupperimmMUX = MUX(exmem[1].ALUresult, exmem[1].upperimm, fwrdSig.EXMEMupperimmB);
    uint32_t IDForwardAMUX= MUX_4(Regs_return[0], MemtoRegMUX ,IDForwardAupperimmMUX, idex[1].upperimm, idfwrdSig.IDForwardA);
    uint32_t IDForwardBMUX= MUX_4(Regs_return[1], MemtoRegMUX ,IDForwardBupperimmMUX, idex[1].upperimm, idfwrdSig.IDForwardB);

    // Comparate two operands
    bool Equal = (IDForwardAMUX == IDForwardBMUX) ? 1 : 0;
    bool PCBranch = (ctrlSig.BNE & !Equal) | (ctrlSig.BEQ & Equal);

    // Extending immediate
    int32_t signimm = (int16_t) inst.imm;
    uint32_t zeroimm = (uint16_t) inst.imm;
    uint32_t extimm = MUX(signimm, zeroimm, ctrlSig.SignZero);

    // Address calculate
    uint32_t BranchAddr = ifid[1].PCadd4 + (signimm << 2);
    uint32_t JumpAddr = (ifid[1].PCadd4 & 0xf0000000) | (inst.address << 2);

    // Select PC address
    uint32_t PCBranchMUX = MUX(PC.PC + 4, BranchAddr, PCBranch);
    uint32_t JumpMUX = MUX_3(PCBranchMUX, JumpAddr, IDForwardAMUX, ctrlSig.Jump);
    if (PC.valid & !(hzrddetectSig.PCnotWrite)) {  // PC write disabled
        PC.PC = JumpMUX;
    }

    BranchAlwaysnotTaken(Branch, PCBranch);

    if (!(ifid[1].valid)) {  // Pipeline is invalid
        return;
    }

    // Save data to pipeline
    idex[0].PCadd8 = ifid[1].PCadd4 + 4;
    idex[0].Rrs = Regs_return[0]; idex[0].Rrt = Regs_return[1];
    idex[0].extimm = extimm; idex[0].upperimm = zeroimm << 16;
    idex[0].funct = inst.funct; idex[0].shamt = inst.shamt;
    idex[0].rs = inst.rs; idex[0].rt = inst.rt; idex[0].rd = inst.rd;

    // Save control signals to pipeline
    idex[0].Shift = ctrlSig.Shift; idex[0].ALUSrc = ctrlSig.ALUSrc;
    idex[0].RegDst[0] = ctrlSig.RegDst[0]; idex[0].RegDst[1] = ctrlSig.RegDst[1];
    idex[0].MemWrite = ctrlSig.MemWrite; idex[0].MemRead = ctrlSig.MemRead;
    idex[0].MemtoReg[0] = ctrlSig.MemtoReg[0]; idex[0].MemtoReg[1] = ctrlSig.MemtoReg[1];
    idex[0].RegWrite = ctrlSig.RegWrite; idex[0].ALUOp = ctrlSig.ALUOp;

    // Flushing IF instruction
    if (ctrlSig.IFFlush) {
        ifid[0].instruction = 0;
    }

    // For visible state
    debugid[1].inst = inst;
    debugid[1].Branch = Branch; debugid[1].PCBranch = PCBranch;
    debugid[1].Predict = BranchPred.Predict[1];

    return;
}

// Instruction Fetch (Backward Taken, Forward Not Taken predictor)
void BTFNTIF(void) {
    debugif.IFPC = PC.PC; debugid[0].valid = PC.valid;
    if (PC.PC == 0xffffffff) {
        ifid[0].valid = 0;
        PC.valid = 0;
        return;
    }
    // Fetch instruction
    uint32_t instruction = InstMem(PC.PC);

    // Check if PC is branch instruction
    BTFNTCheckBranch(PC.PC);

    // Save data to pipeline
    ifid[0].instruction = instruction; ifid[0].PCadd4 = PC.PC + 4;

    // For visible state
    debugif.IFinst = instruction;
    debugif.AddressHit = BranchPred.AddressHit[0];
    debugif.Predict = BranchPred.Predict[0];
    return;
}
// Instruction Decode (Backward Taken, Forward Not Taken predictor)
void BTFNTID(void) {
    idex[0].valid = ifid[1].valid;
    debugex[0].valid = ifid[1].valid;

    // Decode instruction
    INSTRUCTION inst;
    InstDecoder(&inst, ifid[1].instruction);

    // Set control signals
    CtrlUnit(inst.opcode, inst.funct);

    // Load-use, branch hazard detect
    bool Branch = ctrlSig.BEQ | ctrlSig.BNE;
    uint8_t IDEXWritereg = MUX_3(idex[1].rt, idex[1].rd, 31, idex[1].RegDst);
    HazardDetectUnit(inst.rs, inst.rt, idex[1].rt, IDEXWritereg, exmem[1].Writereg,
                     idex[1].MemRead, idex[1].RegWrite, exmem[1].MemRead, Branch, ctrlSig.Jump[1]);
    if (hzrddetectSig.ControlNOP) {  // adding nop
        memset(&ctrlSig, 0, sizeof(CONTROL_SIGNAL));
        Branch = 0;
    }

    // Avoid ID-WB hazard
    if(memwb[1].valid) {
        MemtoRegMUX = MUX_4(memwb[1].ALUresult, memwb[1].Readdata, memwb[1].PCadd8, memwb[1].upperimm, memwb[1].MemtoReg);
        RegsWrite(memwb[1].Writereg, MemtoRegMUX, memwb[1].RegWrite);
    }

    // Register fetch
    uint32_t* Regs_return = RegsRead(inst.rs, inst.rt);

    // Check branch forwarding
    IDForwardUnit(inst.rt, inst.rs, IDEXWritereg,exmem[1].Writereg, memwb[1].Writereg,
                  idex[1].RegWrite, exmem[1].RegWrite, memwb[1].RegWrite, idex[1].MemtoReg, exmem[1].MemtoReg);
    uint32_t IDForwardAupperimmMUX = MUX(exmem[1].ALUresult, exmem[1].upperimm, fwrdSig.EXMEMupperimmA);
    uint32_t IDForwardBupperimmMUX = MUX(exmem[1].ALUresult, exmem[1].upperimm, fwrdSig.EXMEMupperimmB);
    uint32_t IDForwardAMUX= MUX_4(Regs_return[0], MemtoRegMUX ,IDForwardAupperimmMUX, idex[1].upperimm, idfwrdSig.IDForwardA);
    uint32_t IDForwardBMUX= MUX_4(Regs_return[1], MemtoRegMUX ,IDForwardBupperimmMUX, idex[1].upperimm, idfwrdSig.IDForwardB);

    // Comparate two operands
    bool Equal = (IDForwardAMUX == IDForwardBMUX) ? 1 : 0;
    bool PCBranch = (ctrlSig.BNE & !Equal) | (ctrlSig.BEQ & Equal);

    // Extending immediate
    int32_t signimm = (int16_t) inst.imm;
    uint32_t zeroimm = (uint16_t) inst.imm;
    uint32_t extimm = MUX(signimm, zeroimm, ctrlSig.SignZero);

    // Address calculate
    uint32_t BranchAddr = ifid[1].PCadd4 + (signimm << 2);
    uint32_t JumpAddr = (ifid[1].PCadd4 & 0xf0000000) | (inst.address << 2);

    // Update branch result to BTB
    if(!hzrddetectSig.BTBnotWrite) {
        BTFNTUpdateBranchBuffer(Branch, PCBranch, BranchAddr);
    }

    // Select PC address
    bool PCtarget = BranchPred.AddressHit[0] & BranchPred.Predict[0];
    bool BranchHit = !(BranchPred.Predict[1] ^ PCBranch);
    bool IDBrPC = !BranchPred.Predict[1] & PCBranch;
    uint32_t IFIDPCMUX = MUX(ifid[1].PCadd4, PC.PC + 4, BranchHit);
    uint32_t PCBranchMUX = MUX(IFIDPCMUX, BranchAddr, IDBrPC);
    uint32_t JumpMUX = MUX_3(PCBranchMUX, JumpAddr, IDForwardAMUX, ctrlSig.Jump);
    uint32_t PredictMUX = MUX(JumpMUX, BranchPred.BTB[BranchPred.BTBindex[0]][1], PCtarget);
    if (PC.valid & !(hzrddetectSig.PCnotWrite)) {  // PC write disabled
        PC.PC = PredictMUX;
    }

    if (!(ifid[1].valid)) {  // Pipeline is invalid
        return;
    }

    // Save data to pipeline
    idex[0].PCadd8 = ifid[1].PCadd4 + 4;
    idex[0].Rrs = Regs_return[0]; idex[0].Rrt = Regs_return[1];
    idex[0].extimm = extimm; idex[0].upperimm = zeroimm << 16;
    idex[0].funct = inst.funct; idex[0].shamt = inst.shamt;
    idex[0].rs = inst.rs; idex[0].rt = inst.rt; idex[0].rd = inst.rd;

    // Save control signals to pipeline
    idex[0].Shift = ctrlSig.Shift; idex[0].ALUSrc = ctrlSig.ALUSrc;
    idex[0].RegDst[0] = ctrlSig.RegDst[0]; idex[0].RegDst[1] = ctrlSig.RegDst[1];
    idex[0].MemWrite = ctrlSig.MemWrite; idex[0].MemRead = ctrlSig.MemRead;
    idex[0].MemtoReg[0] = ctrlSig.MemtoReg[0]; idex[0].MemtoReg[1] = ctrlSig.MemtoReg[1];
    idex[0].RegWrite = ctrlSig.RegWrite; idex[0].ALUOp = ctrlSig.ALUOp;

    // Flushing IF instruction
    if (ctrlSig.IFFlush) {
        ifid[0].instruction = 0;
    }

    // For visible state
    debugid[1].inst = inst;
    debugid[1].Branch = Branch; debugid[1].PCBranch = PCBranch;
    debugid[1].AddressHit = BranchPred.AddressHit[1];
    debugid[1].Predict = BranchPred.Predict[1];
    return;
}

// Execute
void EX(void) {
    exmem[0].valid = idex[1].valid;
    debugmem[0].valid = idex[1].valid;
    if (!(idex[1].valid)) {
        return;
    }

    // Set ALU operation
    ALUCtrlUnit(idex[1].funct, idex[1].ALUOp);

    // Forwarding
    ForwardUnit(idex[1].rt, idex[1].rs, exmem[1].Writereg, memwb[1].Writereg,
                exmem[1].RegWrite, memwb[1].RegWrite, exmem[1].MemtoReg);
    uint32_t ForwardAupperimmMUX = MUX(exmem[1].ALUresult, exmem[1].upperimm, fwrdSig.EXMEMupperimmA);
    uint32_t ForwardBupperimmMUX = MUX(exmem[1].ALUresult, exmem[1].upperimm, fwrdSig.EXMEMupperimmB);
    uint32_t ForwardAMUX = MUX_3(idex[1].Rrs, MemtoRegMUX, ForwardAupperimmMUX, fwrdSig.ForwardA);
    uint32_t ForwardBMUX = MUX_3(idex[1].Rrt, MemtoRegMUX, ForwardBupperimmMUX, fwrdSig.ForwardB);

    // Execute operation
    uint32_t ALUinput1 = MUX(ForwardAMUX, idex[1].shamt, idex[1].Shift);
    uint32_t ALUinput2 = MUX(ForwardBMUX, idex[1].extimm, idex[1].ALUSrc);
    uint32_t ALUresult = ALU(ALUinput1, ALUinput2, ALUctrlSig.ALUSig);

    // Select write register
    uint8_t Writereg = MUX_3(idex[1].rt, idex[1].rd, 31, idex[1].RegDst);

    // Save data to pipeline
    exmem[0].PCadd8 = idex[1].PCadd8; exmem[0].upperimm = idex[1].upperimm;
    exmem[0].ForwardBMUX = ForwardBMUX; exmem[0].ALUresult = ALUresult;
    exmem[0].Writereg = Writereg; exmem[0].rt = idex[1].rt;

    // Save control signals to pipeline
    exmem[0].MemWrite = idex[1].MemWrite; exmem[0].MemRead = idex[1].MemRead;
    exmem[0].MemtoReg[0] = idex[1].MemtoReg[0]; exmem[0].MemtoReg[1] = idex[1].MemtoReg[1];
    exmem[0].RegWrite = idex[1].RegWrite;

    // For visible state
    debugex[1].ALUinput1 = ALUinput1; debugex[1].ALUinput2 = ALUinput2;
    debugex[1].ALUresult = ALUresult;
    return;
}

// Memory Access
void MEM(const int* Cacheset, const int* Cachesize) {
    memwb[0].valid = exmem[1].valid;
    debugwb[0].valid = exmem[1].valid;
    if (!(exmem[1].valid)) {
        return;
    }

    MEMForwardUnit(exmem[1].rt, memwb[1].Writereg, exmem[1].MemWrite, memwb[1].MemRead);
    uint32_t MemWriteDataMUX = MUX(exmem[1].ForwardBMUX, MemtoRegMUX, memfwrdSig.MEMForward);

    // Cache access
    CheckCache(exmem[1].ALUresult);

    // Memory access
    // TODO
    //  add if to memory access
    counting.cycle += 999;
    uint32_t Readdata = DataMem(exmem[1].ALUresult, MemWriteDataMUX,exmem[1].MemRead, exmem[1].MemWrite);

    // Save data to pipeline
    memwb[0].PCadd8 = exmem[1].PCadd8; memwb[0].ALUresult = exmem[1].ALUresult;
    memwb[0].upperimm = exmem[1].upperimm; memwb[0].Writereg = exmem[1].Writereg;
    memwb[0].Readdata = Readdata;

    // Save control signals to pipeline
    memwb[0].MemtoReg[0] = exmem[1].MemtoReg[0]; memwb[0].MemtoReg[1] = exmem[1].MemtoReg[1];
    memwb[0].RegWrite = exmem[1].RegWrite; memwb[0].MemRead = exmem[1].MemRead;

    // For visible state
    debugmem[1].MemRead = exmem[1].MemRead; debugmem[1].MemWrite = exmem[1].MemWrite;
    debugmem[1].Addr = exmem[1].ALUresult; debugmem[1].Writedata = MemWriteDataMUX;
    return;
}

// Write Back
void WB(void) {
    if (!(memwb[1].valid)) {
        return;
    }
    // For visible state
    if (memwb[1].RegWrite) {
        counting.RegOpcount++;
    }
    debugwb[1].RegWrite = memwb[1].RegWrite; debugwb[1].Writereg = memwb[1].Writereg;
    return;
}

uint8_t makeGHRindex(const bool GHR[], uint32_t PCvalue) {
    uint8_t poweredGHR = 8 * GHR[3] + 4 * GHR[2] + 2 * GHR[1] + GHR[0];
    uint8_t XORPC = (PCvalue >> 2) & 0x0000000f;
    return poweredGHR ^ XORPC;
}