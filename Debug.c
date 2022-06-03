//
// Created by SNMac on 2022/06/01.
//

#include <stdio.h>
#include <string.h>

#include "Debug.h"
#include "Units.h"

DEBUGIF debugif;
DEBUGID debugid[2];
DEBUGEX debugex[2];
DEBUGMEM debugmem[2];
DEBUGWB debugwb[2];

// from main.c
extern uint32_t R[32];

// from Units.c
extern PROGRAM_COUNTER PC;
extern IFID ifid[2];
extern IDEX idex[2];
extern EXMEM exmem[2];
extern MEMWB memwb[2];
extern FORWARD_SIGNAL fwrdSig;
extern ID_FORWARD_SIGNAL idfwrdSig;
extern MEM_FORWARD_SIGNAL memfwrdSig;
extern HAZARD_DETECTION_SIGNAL hzrddetectSig;
extern BRANCH_PREDICT BranchPred;
extern CACHE Cache[4];


void printIF(int Predictor) {
    printf("\n<<<<<<<<<<<<<<<<<<<<<IF>>>>>>>>>>>>>>>>>>>>>\n");
    if (debugif.IFPC == 0xffffffff) {
        printf("End of program\n");
        printf("Wait for finishing other stage\n");
        printf("**********************************************\n");
        return;
    }

    // Print information of IF
    printf("PC : 0x%08x\n", debugif.IFPC);
    printf("Fetch instruction : 0x%08x\n", debugif.IFinst);

    if (Predictor == 1 || Predictor == 2 || Predictor == 3 || Predictor == 4) {
        if (debugif.AddressHit == 0) {
            printf("<BTB hasn't PC value.>\n");
        }
        else {
            if (Predictor == 4 || debugif.Predict == 1) {
                printf("<BTB has PC value. Predict that branch is taken.>\n");
            }
            else {
                printf("<BTB has PC value. Predict that branch is not taken.>\n");
            }
        }
    }
    else if (Predictor == 6) {
        if (debugif.AddressHit == 0) {
            printf("<BTB hasn't PC value.>\n");
        }
        else {
            if (debugif.Predict == 1) {
                printf("<BTB has PC value. Target address is backward.>\n"
                       "<Predict that branch is taken.>\n");
            }
            else {
                printf("<BTB has PC value. Target address is forward.>\n"
                       "<Predict that branch is not taken.>\n");
            }
        }
    }
    printf("**********************************************\n");

    // Hands over data to next debug stage
    debugid[0].IDPC = debugif.IFPC; debugid[0].IDinst = debugif.IFinst;
    return;
}

void printID(int Predictor, const int* Predictbit, const char* Counter) {
    printf("\n<<<<<<<<<<<<<<<<<<<<<ID>>>>>>>>>>>>>>>>>>>>>\n");
    if (!(debugid[1].valid)) {  // Pipeline is invalid
        printf("IF/ID pipeline is invalid\n");
        printf("**********************************************\n");
        return;
    }

    // Print information of ID
    printf("Processing PC : 0x%08x\n", debugid[1].IDPC);
    printf("Processing instruction : 0x%08x\n", debugid[1].IDinst);
    memset(debugid[1].instprint, 0, sizeof(debugid[1].instprint));
    switch (debugid[1].inst.opcode) {
        case 0x0 :  // R-format
            printRformat();
            break;

        case 0x8 :  // addi
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : I   |   addi $%d, $%d, 0x%x", debugid[1].inst.rt, debugid[1].inst.rs, debugid[1].inst.imm);
            break;

        case 0x9 :  // addiu
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : I   |   addiu $%d, $%d, 0x%x", debugid[1].inst.rt, debugid[1].inst.rs, debugid[1].inst.imm);
            break;

        case 0xc :  // andi
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : I   |   andi $%d, $%d, 0x%x", debugid[1].inst.rt, debugid[1].inst.rs, debugid[1].inst.imm);
            break;

        case 0x4 :  // beq
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : I   |   beq $%d, $%d, 0x%x", debugid[1].inst.rt, debugid[1].inst.rs, debugid[1].inst.imm);
            break;

        case 0x5 :  // bne
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : I   |   bne $%d, $%d, 0x%x", debugid[1].inst.rt, debugid[1].inst.rs, debugid[1].inst.imm);
            break;

        case 0x2 :  // j
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : J   |   j 0x%08x", debugid[1].inst.address);
            break;

        case 0x3 :  // jal
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : J   |   jal 0x%08x", debugid[1].inst.address);
            break;

        case 0xf :  // lui
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : I   |   lui $%d, 0x%x", debugid[1].inst.rt, debugid[1].inst.imm);
            break;

        case 0x23 :  // lw
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : I   |   lw $%d, 0x%x($%d)", debugid[1].inst.rt, debugid[1].inst.imm, debugid[1].inst.rs);
            break;

        case 0xd :  // ori
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : I   |   ori $%d, $%d, 0x%x", debugid[1].inst.rt, debugid[1].inst.rs, debugid[1].inst.imm);
            break;

        case 0xa :  // slti
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : I   |   slti $%d, $%d, 0x%x", debugid[1].inst.rt, debugid[1].inst.rs, debugid[1].inst.imm);
            break;

        case 0xb :  // sltiu
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : I   |   sltiu $%d, $%d, 0x%x", debugid[1].inst.rt, debugid[1].inst.rs, debugid[1].inst.imm);
            break;

        case 0x2B :  // sw
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : I   |   sw $%d, 0x%x($%d)", debugid[1].inst.rt, debugid[1].inst.imm, debugid[1].inst.rs);
            break;

        default :
            break;
    }
    printf("%s\n", debugid[1].instprint);

    if (hzrddetectSig.ControlNOP) {
        if (hzrddetectSig.BTBnotWrite) {
            printf("!!Branch(jump) hazard detected. Adding NOP!!\n");
        }
        else {
            printf("!!Load-use hazard detected. Adding NOP!!\n");
        }
    }
    else {
        printIDforward();
        printUpdateBTB(&Predictor, Predictbit, Counter);
    }

    printf("**********************************************\n");

    // Hands over data to next debug stage
    debugex[0].ControlNOP = hzrddetectSig.ControlNOP;
    debugex[0].EXPC = debugid[1].IDPC; debugex[0].EXinst = debugid[1].IDinst;
    strcpy(debugex[0].instprint, debugid[1].instprint);
    return;
}

void printEX(void) {
    debugmem[0].ControlNOP = debugex[1].ControlNOP;

    printf("\n<<<<<<<<<<<<<<<<<<<<<EX>>>>>>>>>>>>>>>>>>>>>\n");
    if (!(debugex[1].valid)) {
        printf("ID/EX pipeline is invalid\n");
        printf("**********************************************\n");
        return;
    }

    // Print information of EX
    printf("Processing PC : 0x%08x\n", debugex[1].EXPC);
    printf("Processing instruction : 0x%08x\n", debugex[1].EXinst);
    if (debugex[1].ControlNOP) {
        printf("Control signals are NOP.\n");
        printf("**********************************************\n");
        return;
    }
    printf("%s\n", debugex[1].instprint);

    printEXforward();

    printf("ALU input1 : 0x%08x (%u)\nALU input2 : 0x%08x (%u)\n",
           debugex[1].ALUinput1, debugex[1].ALUinput1, debugex[1].ALUinput2, debugex[1].ALUinput2);
    printf("ALU result : 0x%08x (%u)\n", debugex[1].ALUresult, debugex[1].ALUresult);
    printf("**********************************************\n");

    // Hands over data to next debug stage
    debugmem[0].MEMPC = debugex[1].EXPC; debugmem[0].MEMinst = debugex[1].EXinst;
    strcpy(debugmem[0].instprint, debugex[1].instprint);
    return;
}

void printMEM(const int* Cachewrite) {
    debugwb[0].ControlNOP = debugmem[1].ControlNOP;
    printf("\n<<<<<<<<<<<<<<<<<<<<<MEM>>>>>>>>>>>>>>>>>>>>>\n");
    if (!(debugmem[1].valid)) {
        printf("EX/MEM pipeline is invalid\n");
        printf("**********************************************\n");
        return;
    }

    // Print information of MEM
    printf("Processing PC : 0x%08x\n", debugmem[1].MEMPC);
    printf("Processing instruction : 0x%08x\n", debugmem[1].MEMinst);
    if (debugmem[1].ControlNOP) {
        printf("Control signals are NOP.\n");
        printf("**********************************************\n");
        return;
    }
    printf("%s\n", debugmem[1].instprint);

    if (memfwrdSig.MEMForward) {
        printf("<Memory Write Data forwarded from MEM/WB pipeline>\n");
    }

    if (debugmem[1].MemRead) {  // lw
        printf("Memory address : 0x%08x\n", debugmem[1].Addr);
        if (debugmem[1].CacheHIT) {
            printf("<Cache HIT>\n");
            printf("%d-set Cache[0x%x][0x%x][0x%x] load -> 0x%x (%d)\n", Cache->way, Cache->index, Cache->tag, Cache->offset,
                                                                        debugmem[1].Readdata, debugmem[1].Readdata);
        } else {
            if (debugmem[1].ColdorConflictMISS) {
                printf("<Cache conflict MISS, Updating cache>\n");
            } else {
                printf("<Cache Cold MISS, Updating cache>\n");
            }
            printf("Memory[0x%08x] load -> 0x%x (%d)\n", debugmem[1].Addr, debugmem[1].Readdata, debugmem[1].Readdata);

            if (debugmem[1].replaceCache) {
                printf("Replace Least Recently Used data to new data.\n");
                if (debugmem[1].dirtyline) {
                    printf("Memory coherence hazard occured.\n");
                    printf("Update memory to cache data.\n");
                    printf("%d-set Cache[0x%x][0x%x][0 ~ 63] -> Memory[0x%08x ~ ff]\n", Cache->way, Cache->index, Cache->tag, debugmem[1].CacheoldAddr);
                }
            }
            printf("%d-set Cache[0x%x][0x%x][0 ~ 63] <- Memory[0x%08x ~ ff]\n", Cache->way, Cache->index, Cache->tag, debugmem[1].CachenowAddr);
        }
    } else if (debugmem[1].MemWrite) {  // sw
        printf("Memory address : 0x%08x\n", debugmem[1].Addr);
        if (debugmem[1].CacheHIT) {  // Cache HIT
            printf("<Cache HIT>\n");
            switch (*Cachewrite) {
                case 1 :
                    // Write-through policy
                    // No write allocate
                    printf("%d-set Cache[0x%x][0x%x][0x%x] store <- 0x%x (%d)\n", Cache->way, Cache->index, Cache->tag, Cache->offset,
                                                                                debugmem[1].Writedata, debugmem[1].Writedata);
                    printf("Memory[0x%08x] <- store 0x%x (%d)\n", debugmem[1].Addr, debugmem[1].Writedata, debugmem[1].Writedata);
                    break;

                case 2 :
                    // Write-back policy
                    // Write allocate
                    printf("%d-set Cache[0x%x][0x%x][0x%x] store <- 0x%x (%d)\n", Cache->way, Cache->index, Cache->tag, Cache->offset,
                                                                                debugmem[1].Writedata, debugmem[1].Writedata);
                    break;
            }
        } else {  // Cache MISS
            if (debugmem[1].ColdorConflictMISS) {  // Conflict MISS
                switch (*Cachewrite) {
                    case 1 :
                        // Write-through policy
                        // No write allocate
                        printf("<Cache conflict MISS>\n");
                        printf("Memory[0x%08x] <- store 0x%x (%d)\n", debugmem[1].Addr, debugmem[1].Writedata, debugmem[1].Writedata);
                        break;

                    case 2 :
                        // Write-back policy
                        // Write allocate
                        printf("<Cache conflict MISS, Updating cache>\n");
                        if (debugmem[1].replaceCache) {
                            printf("Replace Least Recently Used data to new data.\n");
                            if (debugmem[1].dirtyline) {
                                printf("Memory coherence hazard occured.\n");
                                printf("Update memory to cache data.\n");
                                printf("%d-set Cache[0x%x][0x%x][00 ~ 3f] -> Memory[0x%08x ~ ff]\n", Cache->way, Cache->index, Cache->tag, debugmem[1].CacheoldAddr);
                            }
                        }
                        printf("%d-set Cache[0x%x][0x%x][00 ~ 3f] <- Memory[0x%08x ~ ff]\n", Cache->way, Cache->index, Cache->tag, debugmem[1].CachenowAddr);
                        printf("%d-set Cache[0x%x][0x%x][0x%x] store <- 0x%x (%d)\n", Cache->way, Cache->index, Cache->tag, Cache->offset,
                                                                                    debugmem[1].Writedata, debugmem[1].Writedata);
                        break;
                }
            } else {  // Cold MISS
                switch (*Cachewrite) {
                    case 1 :
                        // Write-through policy
                        // No write allocate
                        printf("<Cache cold MISS>\n");
                        printf("Memory[0x%08x] <- store 0x%x (%d)\n", debugmem[1].Addr, debugmem[1].Writedata, debugmem[1].Writedata);
                        break;

                    case 2 :
                        // Write-back policy
                        // Write allocate
                        printf("<Cache cold MISS, Updating cache>\n");
                        if (debugmem[1].replaceCache) {
                            printf("Replace Least Recently Used data to new data.\n");
                            if (debugmem[1].dirtyline) {
                                printf("Memory coherence hazard occured.\n");
                                printf("Update memory to cache data.\n");
                                printf("%d-set Cache[0x%x][0x%x][00 ~ 3f] -> Memory[0x%08x ~ ff]\n", Cache->way, Cache->index, Cache->tag, debugmem[1].CacheoldAddr);
                            }
                        }
                        printf("%d-set Cache[0x%x][0x%x][00 ~ 3f] <- Memory[0x%08x ~ ff]\n", Cache->way, Cache->index, Cache->tag, debugmem[1].CachenowAddr);
                        printf("%d-set Cache[0x%x][0x%x][0x%x] store <- 0x%x (%d)\n", Cache->way, Cache->index, Cache->tag, Cache->offset,
                                                                                    debugmem[1].Writedata, debugmem[1].Writedata);
                        break;
                }

            }
        }
    } else {
        printf("There is no memory access\n");
    }
    printf("**********************************************\n");

    // Hands over data to next debug stage
    debugwb[0].WBPC = debugmem[1].MEMPC; debugwb[0].WBinst = debugmem[1].MEMinst;
    strcpy(debugwb[0].instprint, debugmem[1].instprint);
    return;
}

void printWB(void) {
    printf("\n<<<<<<<<<<<<<<<<<<<<<WB>>>>>>>>>>>>>>>>>>>>>\n");
    if (!(debugwb[1].valid)) {
        printf("MEM/WB pipeline is invalid\n");
        printf("**********************************************\n");
        return;
    }

    printf("Processing PC : 0x%08x\n", debugwb[1].WBPC);
    printf("Processing instruction : 0x%08x\n", debugwb[1].WBinst);
    if (debugwb[1].ControlNOP) {
        printf("Control signals are NOP.\n");
        printf("**********************************************\n");
        return;
    }
    printf("%s\n", debugwb[1].instprint);

    if (debugwb[1].RegWrite) {
        printf("R[%d] = 0x%x (%d)\n", debugwb[1].Writereg, R[debugwb[1].Writereg], R[debugwb[1].Writereg]);
    } else {
        printf("There is no register write\n");
    }

    printf("**********************************************\n");
    return;
}



void printRformat(void) {
    switch (debugid[1].inst.funct) {
        case 0x20 :  // add
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : R   |   add $%d, $%d, $%d", debugid[1].inst.rd, debugid[1].inst.rs, debugid[1].inst.rt);
            break;

        case 0x21 :  // addu
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : R   |   addu $%d, $%d, $%d", debugid[1].inst.rd, debugid[1].inst.rs, debugid[1].inst.rt);
            break;

        case 0x24 :  // and
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : R   |   and $%d, $%d, $%d", debugid[1].inst.rd, debugid[1].inst.rs, debugid[1].inst.rt);
            break;

        case 0x08 :  // jr
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : R   |   jr $%d", debugid[1].inst.rs);
            break;

        case 0x9 :  // jalr
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : R   |   jalr $%d", debugid[1].inst.rs);
            break;

        case 0x27 :  // nor
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : R   |   nor $%d, $%d, $%d", debugid[1].inst.rd, debugid[1].inst.rs, debugid[1].inst.rt);
            break;

        case 0x25 :  // or
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : R   |   or $%d, $%d, $%d", debugid[1].inst.rd, debugid[1].inst.rs, debugid[1].inst.rt);
            break;

        case 0x2a :  // slt
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : R   |   slt $%d, $%d, $%d", debugid[1].inst.rd, debugid[1].inst.rs, debugid[1].inst.rt);
            break;

        case 0x2b :  // sltu
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : R   |   add $%d, $%d, $%d", debugid[1].inst.rd, debugid[1].inst.rs, debugid[1].inst.rt);
            printf("format : R   |   sltu $%d, $%d, $%d", debugid[1].inst.rd, debugid[1].inst.rs, debugid[1].inst.rt);
            break;

        case 0x00 :  // sll
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : R   |   sll $%d, $%d, %d", debugid[1].inst.rd, debugid[1].inst.rs, debugid[1].inst.rt);
            break;

        case 0x02 :  // srl
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : R   |   srl $%d, $%d, %d", debugid[1].inst.rd, debugid[1].inst.rs, debugid[1].inst.rt);
            break;

        case 0x22 :  // sub
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : R   |   sub $%d, $%d, $%d", debugid[1].inst.rd, debugid[1].inst.rs, debugid[1].inst.rt);
            break;

        case 0x23 :  // subu
            snprintf(debugid[1].instprint, sizeof(debugid[1].instprint),
                     "format : R   |   subu $%d, $%d, $%d", debugid[1].inst.rd, debugid[1].inst.rs, debugid[1].inst.rt);
            break;

        default:
            break;
    }
    return;
}

void printIDforward(void) {
    if (idfwrdSig.IDForwardA[1] == 1 && idfwrdSig.IDForwardA[0] == 1) {
        printf("<Register Read data1 forwarded from ID/EX pipeline upperimm>\n");
    }
    else if (idfwrdSig.IDForwardA[1] == 1 && idfwrdSig.IDForwardA[0] == 0) {
        if (idfwrdSig.ID_EXMEMupperimmA) {
            printf("<Register Read data1 forwarded from EX/MEM pipeline upperimm>\n");
            return;
        }
        printf("<Register Read data1 forwarded from EX/MEM pipeline>\n");
    }
    else if (idfwrdSig.IDForwardA[1] == 0 && idfwrdSig.IDForwardA[0] == 1) {
        printf("<Register Read data1 forwarded from MEM/WB pipeline>\n");
    }

    if (idfwrdSig.IDForwardB[1] == 1 && idfwrdSig.IDForwardB[0] == 1) {
        printf("<Register Read data2 forwarded from ID/EX pipeline>\n");
    }
    else if (idfwrdSig.IDForwardB[1] == 1 && idfwrdSig.IDForwardB[0] == 0) {
        if (idfwrdSig.ID_EXMEMupperimmB) {
            printf("<Register Read data2 forwarded from EX/MEM pipeline upperimm>\n");
            return;
        }
        printf("<Register Read data2 forwarded from EX/MEM pipeline>\n");
    }
    else if (idfwrdSig.IDForwardB[1] == 0 && idfwrdSig.IDForwardB[0] == 1) {
        printf("<Register Read data2 forwarded from MEM/WB pipeline>\n");
    }
    return;
}

void printEXforward(void) {
    if (fwrdSig.ForwardA[1] == 1 && fwrdSig.ForwardA[0] == 0) {
        if (fwrdSig.EXMEMupperimmA) {
            printf("<ALU input1 forwarded from EX/MEM pipeline upperimm>\n");
            return;
        }
        printf("<ALU input1 forwarded from EX/MEM pipeline>\n");
    }
    else if (fwrdSig.ForwardA[1] == 0 && fwrdSig.ForwardA[0] == 1) {
        printf("<ALU input1 forwarded from MEM/WB pipeline>\n");
    }

    if (fwrdSig.ForwardB[1] == 1 && fwrdSig.ForwardB[0] == 0) {
        if (fwrdSig.EXMEMupperimmB) {
            printf("<ALU input2 forwarded from EX/MEM pipeline upperimm>\n");
            return;
        }
        printf("<ALU input2 forwarded from EX/MEM pipeline>\n");
    }
    else if (fwrdSig.ForwardB[1] == 0 && fwrdSig.ForwardB[0] == 1) {
        printf("<ALU input2 forwarded from MEM/WB pipeline>\n");
    }
    return;
}

void printUpdateBTB(const int* Predictor, const int* Predictbit, const char* Counter) {
    if (debugid[1].Branch) {
        if (debugid[1].AddressHit) {
            if (debugid[1].PCBranch) {
                printf("Branch taken, ");
                if (*Predictor == 4) {
                    printf("predicted branch taken.\nBranch prediction HIT.\n");
                    return;
                }

                if (debugid[1].Predict) {
                    printf("predicted branch taken.\nBranch prediction HIT.\n");
                }
                else {
                    printf("predicted branch not taken.\nBranch prediction not HIT. Flushing IF instruction.\n");
                }

                if (*Predictor == 1 || *Predictor == 2 || *Predictor == 3) {
                    if (*Predictor == 2 || *Predictor == 3) {
                        printf("<Update GLHR[0] to 1>\n");
                    }
                    printPBtaken(Predictor, Predictbit, Counter);
                }
            }

            else {
                printf("Branch not taken, ");
                if (*Predictor == 4) {
                    printf("predicted branch taken.\nBranch prediction not HIT. Flushing IF instruction.\n");
                    return;
                }

                if (debugid[1].Predict) {
                    printf("predicted branch taken.\nBranch prediction not HIT. Flushing IF instruction.\n");
                }
                else {
                    printf("predicted branch not taken.\nBranch prediction HIT.\n");
                }

                if (*Predictor == 1 || *Predictor == 2 || *Predictor == 3) {
                    if (*Predictor == 2 || *Predictor == 3) {
                        printf("<Update GLHR[0] to 0>\n");
                    }
                    printPBnottaken(Predictor, Predictbit, Counter);
                }
            }
        }
        else {
            if (*Predictor != 5) {
                printf("## Write PC to BTB ##\n");
                printf("Branch PC : 0x%08x\n", BranchPred.BTB[BranchPred.BTBindex[1]][0]);
                printf("Predicted target : 0x%08x\n", BranchPred.BTB[BranchPred.BTBindex[1]][1]);
            }

            if (debugid[1].PCBranch) {
                if (*Predictor == 1 || *Predictor == 2 || *Predictor == 3) {
                    if (*Predictor == 2) {
                        printf("<Update GLHR[0] to 1>\n");
                    }
                    printPBtaken(Predictor, Predictbit, Counter);
                }
                else if (*Predictor == 5) {
                    printf("Branch taken, predicted branch not taken.\nBranch prediction not HIT.\n");
                    printf("FLushing IF instruction.\n");
                    return;
                }
                printf("Instruction is taken branch. FLushing IF instruction.\n");
            }
            else {
                if (*Predictor == 2 || *Predictor == 3) {
                    printf("<Update GLHR[0] to 0>\n");
                }
                else if (*Predictor == 5) {
                    printf("Branch not taken, predicted branch not taken.\nBranch prediction HIT.\n");
                    return;
                }
                printf("Instruction is not taken branch.\n");
            }
        }

        if (*Predictor == 2 || *Predictor == 3) {
            printf ("GLHR = %d%d%d%d\n", BranchPred.GLHR[3], BranchPred.GLHR[2], BranchPred.GLHR[1], BranchPred.GLHR[0]);
        }
    }
    return;
}

void printPBtaken(const int* Predictor, const int* Predictbit, const char* Counter) {
    switch (*Predictbit) {
        case 1 :
            printf("<Update PB to 1>\n");
            break;

        case 2 :
            if (*Counter == '1') {
                if (debugid[1].PB == 3 || debugid[1].PB == 2) {
                    printf("<Update PB to 3>\n");
                }
                else if (debugid[1].PB == 1) {
                    printf("<Update PB to 2>\n");
                }
                else {
                    printf("<Update PB to 1>\n");
                }
            }
            else if (*Counter == '2') {
                if (debugid[1].PB == 1 || debugid[1].PB == 2 || debugid[1].PB == 3) {
                    printf("<Update PB to 3>\n");
                }
                else {
                    printf("<Update PB to 1>\n");
                }
            }
            break;
    }
    switch (*Predictor) {
        case 1 :
            printf("Branch PC : 0x%08x\n", BranchPred.DP[BranchPred.DPindex[1]][0]);
            printf("Prediction bits : %d\n", BranchPred.DP[BranchPred.DPindex[1]][1]);
            break;

        case 2 :
            printf("Index : ");
            for (int j = 3; j >= 0; j--) {
                printf("%d", BranchPred.BHT[BranchPred.BHTindex[1]][0] >> j & 1);
            }
            printf("\nPrediction bits : %d\n", BranchPred.BHT[BranchPred.BHTindex[1]][1]);
            break;

        case 3 :
            printf("Index : ");
            for (int j = 3; j >= 0; j--) {
                printf("%d", BranchPred.BHT[BranchPred.BHTindex[1]][0] >> j & 1);
            }
            printf("\nPrediction bits : %d\n", BranchPred.BHT[BranchPred.BHTindex[1]][1]);
            break;
    }
    return;
}

void printPBnottaken(const int* Predictor, const int* Predictbit, const char* Counter) {
    switch (*Predictbit) {
        case 1 :
            printf("<Update PB to 0>\n");
            break;
        case 2 :
            if (*Counter == '1') {
                if (debugid[1].PB == 0 || debugid[1].PB == 1) {
                    printf("<Update PB to 0>\n");
                }
                else if (debugid[1].PB == 2) {
                    printf("<Update PB to 1>\n");
                }
                else {
                    printf("<Update PB to 2>\n");
                }
            }
            else if (*Counter == '2') {
                if (debugid[1].PB == 0 || debugid[1].PB == 1 || debugid[1].PB == 2) {
                    printf("<Update PB to 0>\n");
                }
                else {
                    printf("<Update PB to 2>\n");
                }
            }
            break;
    }
    switch (*Predictor) {
        case 1 :
            printf("Branch PC : 0x%08x\n", BranchPred.DP[BranchPred.DPindex[1]][0]);
            printf("Prediction bits : %d\n", BranchPred.DP[BranchPred.DPindex[1]][1]);
            break;

        case 2 :
            printf("Index : ");
            for (int j = 3; j >= 0; j--) {
                printf("%d", BranchPred.BHT[BranchPred.BHTindex[1]][0] >> j & 1);
            }
            printf("\nPrediction bits : %d\n", BranchPred.BHT[BranchPred.BHTindex[1]][1]);
            break;

        case 3 :
            printf("Index : ");
            for (int j = 3; j >= 0; j--) {
                printf("%d", BranchPred.BHT[BranchPred.BHTindex[1]][0] >> j & 1);
            }
            printf("\nPrediction bits : %d\n", BranchPred.BHT[BranchPred.BHTindex[1]][1]);
            break;
    }
    return;
}

void DebugPipelineHandsOver(void) {
    if (!hzrddetectSig.IFIDnotWrite){
        debugid[1] = debugid[0];
    }
    debugex[1] = debugex[0];
    debugmem[1] = debugmem[0];
    debugwb[1] = debugwb[0];
    return;
}