//
// Created by SNMac on 2022/06/01.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <time.h>

#include "main.h"
#include "Units.h"
#include "Stages.h"
#include "Debug.h"

#define DIRECTORY "/testbin/"
#define SAFE_FREE(p) {if(p!=NULL){free(p);p=NULL;}}
COUNTING counting;  // for result counting

uint32_t Memory[0x400000];
uint32_t R[32];

// from Units.c
extern PROGRAM_COUNTER PC;
extern IFID ifid[2];
extern IDEX idex[2];
extern EXMEM exmem[2];
extern MEMWB memwb[2];
extern HAZARD_DETECTION_SIGNAL hzrddetectSig;
extern BRANCH_PREDICT BranchPred;
extern CACHE Cache[4];

// from Debug.c
extern DEBUGID debugid[2];
extern DEBUGEX debugex[2];
extern DEBUGMEM debugmem[2];
extern DEBUGWB debugwb[2];

// This program reads *.bin files from testbin folder
// It must be located in upper directory of executable file
// for example)
// Location of executable file : User/Project/
// Location of testbin folder : User/

int main(int argc, char* argv[]) {
    while (1) {
        char* filename = malloc(sizeof(char) * PATH_MAX);
        memset(filename, 0, sizeof(&filename));
        strcat(filename, argv[0]);
        char Predictor = '0';
        int PredictionBit = 0;
        char Counter = '0';
        int CacheSet = 0;
        int CacheSize = 0;
        int CacheWrite = 0;

        FileSelect(&filename);
        Predictor = PredSelect();
        if (Predictor == '1' || Predictor == '2' || Predictor == '3') {
            PredictionBit = PBSelect();
            if (PredictionBit == '2') {
                Counter = CounterSelect();
            }
        }
        CacheSize = CacheSizeSelect();
        CacheSet = CacheSetSelect();
        CacheWrite = CacheWriteSelect();
        CacheSetting(&CacheSet, &CacheSize);

        clock_t start = clock();

        Firstinit(&PredictionBit);

        FILE* fp = fopen(filename, "rb");
        if (fp == NULL) {
            perror("ERROR");
            exit(EXIT_FAILURE);
        }
        int buffer;
        int index = 0;
        int ret;
        while (1) {
            ret = fread(&buffer, sizeof(buffer), 1, fp);
            if (ret != 1) {  // EOF or error
                break;
            }
            Memory[index] = __builtin_bswap32(buffer);  // Little endian to Big endian
            index++;
        }
        if (index == 0) {
            goto END;
        }

        for (int i = 0; i < index; i++) {
            printf("Memory [%d] : 0x%08x\n", i, Memory[i]);
        }

        switch (Predictor) {
            case '1' :
                OnelevelPredict(&PredictionBit, &Counter, &CacheSet, &CacheSize, &CacheWrite);
                break;

            case '2' :
                GsharePredict(&PredictionBit, &Counter, &CacheSet, &CacheSize, &CacheWrite);
                break;

            case '3' :
                LocalPredict(&PredictionBit, &Counter, &CacheSet, &CacheSize, &CacheWrite);
                break;

            case '4' :
                AlwaysTaken(&CacheSet, &CacheSize, &CacheWrite);
                break;

            case '5' :
                AlwaysnotTaken(&CacheSet, &CacheSize, &CacheWrite);
                break;

            case '6' :
                BTFNT(&CacheSet, &CacheSize, &CacheWrite);
                break;

            default :
                fprintf(stderr, "ERROR: main) char Predictor is wrong.\n");
                exit(EXIT_FAILURE);
        }

        END:
        printFinalresult(&Predictor, &PredictionBit, filename, &Counter, &CacheSet, &CacheSize, &CacheWrite);
        fclose(fp);
        free(filename);
        FreeCache(&CacheSet, &CacheSize);

        clock_t end = clock();
        printf("Execution time : %lf\n", (double)(end - start) / CLOCKS_PER_SEC);

        char retry;
        printf("\n\n");
        printf("Retry? (Y/N) : ");
        while (1) {
            scanf(" %c", &retry);
            getchar();
            if (retry == 'N') {
                return 0;
            }
            else if (retry == 'Y') {
                printf("\n\n");
                break;
            }
            else {
                printf("User inputted wrong character. Please try again.\n");
            }
        }
    }
}

// Filename select
void FileSelect(char** name) {
    char* files[10];
    memset(files, 0, sizeof(files));

    ReadDirectory(files, name);

    int filenameSelector;
    while (1) {
        printf("\n");
        printf("Select filename : ");
        scanf(" %d", &filenameSelector);
        getchar();
        if (filenameSelector < 1 || files[filenameSelector - 1] == NULL) {
            printf("User inputted wrong number. Please try again.\n");
        }
        else {
            strcat(*name, files[filenameSelector - 1]);
            printf("%s\n", files[filenameSelector - 1]);
            break;
        }
    }
}

// Read designated directory
void ReadDirectory(char** files, char** directory) {
    int index = 0;
    int name_count;
    char* ext;
    struct dirent** name_list = NULL;

    char* upperdirect;
    printf("######################################################################\n");

    // Cut last directory to get execution file's upper directory
    upperdirect = strrchr(*directory, '/'); *upperdirect = '\0';
    upperdirect = strrchr(*directory, '/'); *upperdirect = '\0';

    strcat(*directory, DIRECTORY);  // access to testbin folder of upper directory
    printf("Read *.bin file from designated directory : \n%s\n", *directory);
    printf("======================================================================\n");


    name_count = scandir(*directory, &name_list, NULL, alphasort);  // Get files' name and sort them in alphabetical order
    for (int i = 0; i < name_count; i++) {
        ext = strchr(name_list[i]->d_name, '.');  // Get extension from file
        if (ext == NULL) {
            continue;
        }
        if (strcmp(ext, ".bin") == 0) {  // If file's name has .bin extension
            if (index != 0 && index % 4 != 0) {
                printf(", ");
            }
            files[index] = name_list[i]->d_name;  // Save it to array
            printf("%d : %s", index + 1, files[index]);
            index++;
            if (index % 4 == 0) {
                printf("\n");
            }
        }
        SAFE_FREE(name_list[i]);
    }
    printf("\n");
    printf("######################################################################\n");
    SAFE_FREE(name_list);
}

// Predictor select
char PredSelect(void) {
    char retVal;
    printf("\n");
    printf("#############################################################################\n");
    printf("1 : One-level Branch Predictor, 2 : Two-level Global History Branch Predictor\n");
    printf("            3 : Two-level Local History Branch Predictor                     \n");
    printf("4 : Always Taken Predictor,     5 : Always Not Taken Predictor               \n");
    printf("            6 : Backward Taken, Forward Not Taken Predictor                  \n");
    printf("#############################################################################\n");
    while (1) {
        printf("\n");
        printf("Select branch predictor : ");
        scanf(" %c", &retVal);
        getchar();
        if (retVal == '1' || retVal == '2' || retVal == '3' || retVal == '4' || retVal == '5' || retVal == '6') {
            return retVal;
        }
        else {
            printf("User inputted wrong number. Please try again.\n");
        }
    }
}

// Prediction bit select
int PBSelect(void) {
    int retVal;
    printf("\n");
    printf("##########################################\n");
    printf("1 : 1-bit prediction, 2 : 2-bit prediction\n");
    printf("##########################################\n");
    while (1) {
        printf("\n");
        printf("Select prediction bit : ");
        scanf(" %d", &retVal);
        getchar();
        if (retVal == 1 || retVal == 2) {
            return retVal;
        }
        else {
            printf("User inputted wrong number. Please try again.\n");
        }
    }
}

// Prediction counter select
char CounterSelect(void) {
    char retVal;
    printf("\n");
    printf("##############################################\n");
    printf("1 : Saturating Counter, 2 : Hysteresis Counter\n");
    printf("##############################################\n");
    while (1) {
        printf("\n");
        printf("Select counter : ");
        scanf(" %c", &retVal);
        getchar();
        if (retVal == '1' || retVal == '2') {
            return retVal;
        }
        else {
            printf("User inputted wrong number. Please try again.\n");
        }
    }
}

// Cache size select
int CacheSizeSelect(void) {
    int retVal;
    printf("\n");
    printf("############################################\n");
    printf("1 : 256 bytes, 2 : 512 bytes, 3 : 1024 bytes\n");
    printf("############################################\n");
    while (1) {
        printf("\n");
        printf("Select cache size : ");
        scanf(" %d", &retVal);
        getchar();
        switch (retVal) {
            case 1 :  // 256 bytes
                return 256;

            case 2 :  // 512 bytes
                return 512;

            case 3 :  // 1024 bytes
                return 1024;

            default :
                printf("User inputted wrong number. Please try again.\n");
        }
    }
}

// Cache set-associativity select
int CacheSetSelect(void) {
    int retVal;
    printf("\n");
    printf("#######################################\n");
    printf("1 : Direct-mapped, 2 : 2-way, 3 : 4-way\n");
    printf("#######################################\n");
    while (1) {
        printf("\n");
        printf("Select cache set-associativity : ");
        scanf(" %d", &retVal);
        getchar();
        switch (retVal) {
            case 1 :  // Direct-mapped
                return 1;

            case 2 :  // 2-way
                return 2;

            case 3 :  // 4-way
                return 4;

            default :
                printf("User inputted wrong number. Please try again.\n");
        }
    }
}

// Cache write policy select
int CacheWriteSelect(void) {
    int retVal;
    printf("\n");
    printf("#################################\n");
    printf("1 : Write-through, 2 : Write-back\n");
    printf("#################################\n");
    while (1) {
        printf("\n");
        printf("Select cache write policy : ");
        scanf(" %d", &retVal);
        getchar();
        switch (retVal) {
            case 1 :  // Write-through
                return 1;

            case 2 :  // Write-back
                return 2;

            default :
                printf("User inputted wrong number. Please try again.\n");
        }
    }
}

// Cache setting
void CacheSetting(const int* Cacheset, const int* Cachesize) {
    for (int way = 0; way < *Cacheset; way++) {
        Cache[way].Cache = (uint32_t***)malloc(sizeof(uint32_t**) * (*Cachesize / CACHELINESIZE));
        for (int i = 0; i < (*Cachesize / CACHELINESIZE); i++) {
            Cache[way].Cache[i] = (uint32_t**)malloc(sizeof(uint32_t*) * 5);
            for (int j = 0; j < 5; j++) {
                Cache[way].Cache[i][j] = (uint32_t*)malloc(sizeof(uint32_t) * 64);
            }
        }
    }
}

// Free malloc cache
void FreeCache(const int* Cacheset, const int* Cachesize) {
    for (int way = 0; way < *Cacheset; way++) {
        for (int i = 0; i < (*Cachesize / CACHELINESIZE); i++) {
            for (int j = 0; j < 5; j++) {
                free(Cache[way].Cache[i][j]);
            }
            free(Cache[way].Cache[i]);
        }
        free(Cache[way].Cache);
    }
}

// One-level predictor
void OnelevelPredict(const int* Predictbit, const char* Counter, const int* Cacheset, const int* Cachesize, const int* Cachewrite) {
    while(1) {
        if (!(ifid[0].valid | idex[0].valid | exmem[0].valid | memwb[0].valid)) {
            return;
        }

        OnelevelIF(Predictbit);
        OnelevelID(Predictbit, Counter);
        EX();
        MEM(Cacheset, Cachesize, Cachewrite);
        WB();
        printIF(1);
        printID(1, Predictbit, Counter);
        printEX();
        printMEM(Cachewrite);
        printWB();
        printnextPC();

        OnelevelPipelineHandsOver();
        DebugPipelineHandsOver();

        counting.cycle++;
        printf("\n================================ CC %d ================================\n", counting.cycle);
    }
}

// Gshare predictor execute
void GsharePredict(const int* Predictbit, const char* Counter, const int* Cacheset, const int* Cachesize, const int* Cachewrite) {
    while(1) {
        if (!(ifid[0].valid | idex[0].valid | exmem[0].valid | memwb[0].valid)) {
            return;
        }
        GshareIF(Predictbit);
        GshareID(Predictbit, Counter);
        EX();
        MEM(Cacheset, Cachesize, Cachewrite);
        WB();
        printIF(2);
        printID(2, Predictbit, Counter);
        printEX();
        printMEM(Cachewrite);
        printWB();
        printnextPC();

        GsharePipelineHandsOver();
        DebugPipelineHandsOver();

        counting.cycle++;
        printf("\n================================ CC %d ================================\n", counting.cycle);
    }
}

// Local predictor execute
void LocalPredict(const int* Predictbit, const char* Counter, const int* Cacheset, const int* Cachesize, const int* Cachewrite) {
    while(1) {
        if (!(ifid[0].valid | idex[0].valid | exmem[0].valid | memwb[0].valid)) {
            return;
        }
        LocalIF(Predictbit);
        LocalID(Predictbit, Counter);
        EX();
        MEM(Cacheset, Cachesize, Cachewrite);
        WB();
        printIF(3);
        printID(3, Predictbit, Counter);
        printEX();
        printMEM(Cachewrite);
        printWB();
        printnextPC();

        LocalPipelineHandsOver();
        DebugPipelineHandsOver();

        counting.cycle++;
        printf("\n================================ CC %d ================================\n", counting.cycle);
    }
}

// Always taken predictor execute
void AlwaysTaken(const int* Cacheset, const int* Cachesize, const int* Cachewrite) {
    while(1) {
        if (!(ifid[0].valid | idex[0].valid | exmem[0].valid | memwb[0].valid)) {
            return;
        }

        AlwaysTakenIF();
        AlwaysTakenID();
        EX();
        MEM(Cacheset, Cachesize, Cachewrite);
        WB();
        printIF(4);
        printID(4, 0, 0);
        printEX();
        printMEM(Cachewrite);
        printWB();
        printnextPC();

        AlwaysTakenPipelineHandsOver();
        DebugPipelineHandsOver();

        counting.cycle++;
        printf("\n================================ CC %d ================================\n", counting.cycle);
    }
}

// Always not taken predictor execute
void AlwaysnotTaken(const int* Cacheset, const int* Cachesize, const int* Cachewrite) {
    while(1) {
        if (!(ifid[0].valid | idex[0].valid | exmem[0].valid | memwb[0].valid)) {
            return;
        }

        AlwaysnotTakenIF();
        AlwaysnotTakenID();
        EX();
        MEM(Cacheset, Cachesize, Cachewrite);
        WB();
        printIF(5);
        printID(5, 0, 0);
        printEX();
        printMEM(Cachewrite);
        printWB();
        printnextPC();

        OnelevelPipelineHandsOver();
        DebugPipelineHandsOver();

        counting.cycle++;
        printf("\n================================ CC %d ================================\n", counting.cycle);
    }
}

void BTFNT(const int* Cacheset, const int* Cachesize, const int* Cachewrite) {
    while(1) {
        if (!(ifid[0].valid | idex[0].valid | exmem[0].valid | memwb[0].valid)) {
            return;
        }

        BTFNTIF();
        BTFNTID();
        EX();
        MEM(Cacheset, Cachesize, Cachewrite);
        WB();
        printIF(6);
        printID(6, 0, 0);
        printEX();
        printMEM(Cachewrite);
        printWB();
        printnextPC();

        BTFNTPipelineHandsOver();
        DebugPipelineHandsOver();

        counting.cycle++;
        printf("\n================================ CC %d ================================\n", counting.cycle);
    }
}

void Firstinit(const int* Predictbit) {
    memset(&PC, 0, sizeof(PROGRAM_COUNTER));
    memset(&Memory, 0, sizeof(Memory));
    memset(&R, 0, sizeof(R));
    memset(&BranchPred, 0, sizeof(BRANCH_PREDICT));
    memset(&counting, 0, sizeof(COUNTING));
    switch (*Predictbit) {
        case 0 :  // Always taken or not taken
            break;

        case 1 :  // One-bit predictor
            for (int i = 0; i < BTBMAX; i++) {
                BranchPred.DP[i][1] = 0;  // Initialize Direction Predictor to 1
            }
            for (int i = 0; i < BHTMAX; i++) {
                BranchPred.BHT[i][0] = i;
                BranchPred.BHT[i][1] = 0;  // Initialize Branch History Table to 1
            }
            break;

        case 2 :  // Two-bit predictor
            for (int i = 0; i < BTBMAX; i++) {
                BranchPred.DP[i][1] = 1;  // Initialize Direction Predictor to 1
            }
            for (int i = 0; i < BHTMAX; i++) {
                BranchPred.BHT[i][0] = i;
                BranchPred.BHT[i][1] = 1;  // Initialize Branch History Table to 1
            }
            break;

        default :
            fprintf(stderr, "ERROR: Firstinit) const char* Predictbit is wrong.\n");
            exit(EXIT_FAILURE);
    }
    PC.valid = 1;
    ifid[0].valid = 1;
    R[31] = 0xFFFFFFFF;  // $ra = 0xFFFFFFFF
    R[29] = 0x1000000;  // $sp = 0x1000000
    return;
}

void printnextPC(void) {
    printf("\n");
    printf("##########################\n");
    if (PC.valid & !hzrddetectSig.PCnotWrite) {
        printf("## Next PC = 0x%08x ##\n", PC.PC);
    }
    else {
        printf("## !!! No PC update !!! ##\n");
    }
    printf("##########################\n");
    return;
}

void OnelevelPipelineHandsOver(void) {
    if (!hzrddetectSig.IFIDnotWrite){
        ifid[1] = ifid[0];  // IF/ID pipeline hands data to ID
    }

    if (!hzrddetectSig.BTBnotWrite) {
        BranchPred.Predict[1] = BranchPred.Predict[0];
        BranchPred.AddressHit[1] = BranchPred.AddressHit[0];
        BranchPred.BTBindex[1] = BranchPred.BTBindex[0];
        BranchPred.DPindex[1] = BranchPred.DPindex[0];
        BranchPred.instPC[1] = BranchPred.instPC[0];
    }

    idex[1] = idex[0];  // ID/EX pipeline hands data to EX
    exmem[1] = exmem[0];  // EX/MEM pipeline hands data to MEM
    memwb[1] = memwb[0];  // MEM/WB pipeline hands data to WB
    return;
}

void GsharePipelineHandsOver(void) {
    if (!hzrddetectSig.IFIDnotWrite){
        ifid[1] = ifid[0];  // IF/ID pipeline hands data to ID
    }

    if (!hzrddetectSig.BTBnotWrite) {
        BranchPred.Predict[1] = BranchPred.Predict[0];
        BranchPred.AddressHit[1] = BranchPred.AddressHit[0];
        BranchPred.BTBindex[1] = BranchPred.BTBindex[0];
        BranchPred.BHTindex[1] = BranchPred.BHTindex[0];
        BranchPred.instPC[1] = BranchPred.instPC[0];
    }

    idex[1] = idex[0];  // ID/EX pipeline hands data to EX
    exmem[1] = exmem[0];  // EX/MEM pipeline hands data to MEM
    memwb[1] = memwb[0];  // MEM/WB pipeline hands data to WB
    return;
}

void LocalPipelineHandsOver(void) {
    if (!hzrddetectSig.IFIDnotWrite){
        ifid[1] = ifid[0];  // IF/ID pipeline hands data to ID
    }

    if (!hzrddetectSig.BTBnotWrite) {
        BranchPred.Predict[1] = BranchPred.Predict[0];
        BranchPred.AddressHit[1] = BranchPred.AddressHit[0];
        BranchPred.BTBindex[1] = BranchPred.BTBindex[0];
        BranchPred.LHRindex[1] = BranchPred.LHRindex[0];
        BranchPred.BHTindex[1] = BranchPred.BHTindex[0];
        BranchPred.instPC[1] = BranchPred.instPC[0];
    }

    idex[1] = idex[0];  // ID/EX pipeline hands data to EX
    exmem[1] = exmem[0];  // EX/MEM pipeline hands data to MEM
    memwb[1] = memwb[0];  // MEM/WB pipeline hands data to WB
    return;
}

void AlwaysTakenPipelineHandsOver(void) {
    if (!hzrddetectSig.IFIDnotWrite){
        ifid[1] = ifid[0];  // IF/ID pipeline hands data to ID
    }

    if (!hzrddetectSig.BTBnotWrite) {
        BranchPred.AddressHit[1] = BranchPred.AddressHit[0];
        BranchPred.BTBindex[1] = BranchPred.BTBindex[0];
        BranchPred.instPC[1] = BranchPred.instPC[0];
    }

    idex[1] = idex[0];  // ID/EX pipeline hands data to EX
    exmem[1] = exmem[0];  // EX/MEM pipeline hands data to MEM
    memwb[1] = memwb[0];  // MEM/WB pipeline hands data to WB
    return;
}

void BTFNTPipelineHandsOver(void) {
    if (!hzrddetectSig.IFIDnotWrite){
        ifid[1] = ifid[0];  // IF/ID pipeline hands data to ID
    }

    if (!hzrddetectSig.BTBnotWrite) {
        BranchPred.Predict[1] = BranchPred.Predict[0];
        BranchPred.AddressHit[1] = BranchPred.AddressHit[0];
        BranchPred.BTBindex[1] = BranchPred.BTBindex[0];
        BranchPred.instPC[1] = BranchPred.instPC[0];
    }

    idex[1] = idex[0];  // ID/EX pipeline hands data to EX
    exmem[1] = exmem[0];  // EX/MEM pipeline hands data to MEM
    memwb[1] = memwb[0];  // MEM/WB pipeline hands data to WB
    return;
}

void printFinalresult(const char* Predictor, const int* Predictbit, const char* filename, const char* Counter,
                      const int* Cacheset, const int* Cachesize, const int* Cachewrite) {
    printf("\n\n");
    printf("===============================================================\n");
    printf("===============================================================\n");
    printf("<<<<<<<<<<<<<<<<<<<<<<<<End of execution>>>>>>>>>>>>>>>>>>>>>>>>\n");

    // Print Registers
    printf("\n\n");
    printf("################ Registers #################\n");
    for (int i = 0; i < 32; i += 2) {
        printf("R[%d] : %d   |   R[%d] : %d\n", i, R[i], i + 1, R[i + 1]);
    }
    printf("############################################\n");

    // Print selected predictor
    switch (*Predictor) {
        case '1' :  // One-level predictor
            // Print DP
            printf("\n\n");
            printf("################### Direction Predictor ###################\n");
            printf("##   Branch PCs   ## Prediction bits ## Frequency of use ##\n");
            for (int i = 0; i < BranchPred.DPsize; i++) {
                printf("##    0x%08x  ##        %d        ## %16d ##\n", BranchPred.DP[i][0], BranchPred.DP[i][1], BranchPred.DP[i][2]);
            }
            printf("###########################################################\n");
            // Print BTB
            printf("\n\n");
            printf("################# Branch Target Buffer #################\n");
            printf("## Branch PCs ## Predicted target ## Frequency of use ##\n");
            for (int i = 0; i < BranchPred.BTBsize; i++) {
                printf("## 0x%08x ##    0x%08x    ## %16d ##\n", BranchPred.BTB[i][0], BranchPred.BTB[i][1], BranchPred.BTB[i][2]);
            }
            printf("########################################################\n");
            break;

        case '2' :  // Gshare predictor
            // Print BHT
            printf("\n\n");
            printf("###### Branch History Table #######\n");
            printf("##   Index  ##  Prediction bits  ##\n");
            for (int i = 0; i < BHTMAX; i++) {
                printf("##    ");
                for (int j = 3; j >= 0; j--) {
                    printf("%d", BranchPred.BHT[i][0] >> j & 1);
                }
                printf("  ##         %d         ##\n", BranchPred.BHT[i][1]);
            }
            printf("###################################\n");
            // Print BTB
            printf("\n\n");
            printf("################# Branch Target Buffer #################\n");
            printf("## Branch PCs ## Predicted target ## Frequency of use ##\n");
            for (int i = 0; i < BranchPred.BTBsize; i++) {
                printf("## 0x%08x ##    0x%08x    ## %16d ##\n", BranchPred.BTB[i][0], BranchPred.BTB[i][1], BranchPred.BTB[i][2]);
            }
            printf("########################################################\n");
            break;

        case '3' :
            // Print LHR
            printf("\n\n");
            printf("##################### Local History Table ######################\n");
            printf("##  Branch PCs  ##  Local Branch History  ## Frequency of use ##\n");
            for (int i = 0; i < BranchPred.LHRsize; i++) {
                printf("##  0x%08x  ##          ", BranchPred.LHR[i][0]);
                for (int j = 3; j >= 0; j--) {
                    printf("%d", BranchPred.LHR[i][1] >> j & 1);
                }
                printf("          ##       %10d ##\n", BranchPred.LHR[i][2]);
            }
            printf("################################################################\n");
            // Print BHT
            printf("\n\n");
            printf("###### Branch History Table #######\n");
            printf("##   Index  ##  Prediction bits  ##\n");
            for (int i = 0; i < BHTMAX; i++) {
                printf("##    ");
                for (int j = 3; j >= 0; j--) {
                    printf("%d", BranchPred.BHT[i][0] >> j & 1);
                }
                printf("  ##         %d         ##\n", BranchPred.BHT[i][1]);
            }
            printf("###################################\n");
            // Print BTB
            printf("\n\n");
            printf("################# Branch Target Buffer #################\n");
            printf("## Branch PCs ## Predicted target ## Frequency of use ##\n");
            for (int i = 0; i < BranchPred.BTBsize; i++) {
                printf("## 0x%08x ##    0x%08x    ## %16d ##\n", BranchPred.BTB[i][0], BranchPred.BTB[i][1], BranchPred.BTB[i][2]);
            }
            printf("########################################################\n");
            break;
            break;

        case '4' :
            // Print BTB
            printf("\n\n");
            printf("################# Branch Target Buffer #################\n");
            printf("## Branch PCs ## Predicted target ## Frequency of use ##\n");
            for (int i = 0; i < BranchPred.BTBsize; i++) {
                printf("## 0x%08x ##    0x%08x    ## %16d ##\n", BranchPred.BTB[i][0], BranchPred.BTB[i][1], BranchPred.BTB[i][2]);
            }
            printf("########################################################\n");
            break;
            break;

        case '5' :
            // Print nothing
            break;

        case '6' :
            // Print BTB
            printf("\n\n");
            printf("################# Branch Target Buffer #################\n");
            printf("## Branch PCs ## Predicted target ## Frequency of use ##\n");
            for (int i = 0; i < BranchPred.BTBsize; i++) {
                printf("## 0x%08x ##    0x%08x    ## %16d ##\n", BranchPred.BTB[i][0], BranchPred.BTB[i][1], BranchPred.BTB[i][2]);
            }
            printf("########################################################\n");
            break;

        default :
            fprintf(stderr, "ERROR: printFinalresult) const char* Predictor is wrong.\n");
            exit(EXIT_FAILURE);
    }

    // Print cache
    for (int way = 0; way < *Cacheset; way++) {
        printf("\n\n");
        printf("###################### %d-way ######################\n", way);
        printf("##                  index table                  ##\n");
        printf("## Valid ##    Tag    ## Shift register ## Dirty ##\n");

        for (int index = 0; index < *Cachesize / CACHELINESIZE; index++) {
                printf("##   %d   ## 0x%07x ##      ", Cache[way].Cache[index][0][0], Cache[way].Cache[index][1][0]);
                for (int j = 2; j >= 0; j--) {
                    printf("%d", Cache[way].Cache[index][3][0] >> j & 1);
                }
                printf("       ##   %d   ##\n", Cache[way].Cache[index][4][0]);
        }
        printf("###################################################\n");
        printf("#######              data table             #######\n");
        printf("#######    Tag    ##          Data          #######\n");
        for (int index = 0; index < *Cachesize / CACHELINESIZE; index++) {
            printf("####### 0x%07x ## ", Cache[way].Cache[index][1][0]);
            for (int data = 0; data < 64; data += 4) {
                printf("  0x%02x : 0x%02x%02x%02x%02x    #######\n", data, Cache[way].Cache[index][2][data], Cache[way].Cache[index][2][data + 1],
                                            Cache[way].Cache[index][2][data + 2], Cache[way].Cache[index][2][data + 3]);
                if (data == 60) {
                    printf("###################################################\n");
                } else {
                    printf("#######           ## ");
                }
            }
        }
    }
    printf("###################################################\n");


    // Print executed file name
    printf("\n\n");
    printf("File directory : %s\n", filename);

    // Calculate branch HIT rate
    double BranchHITrate = 0;
    if ((counting.takenBranch + counting.nottakenBranch) != 0) {
        BranchHITrate = (double)counting.PredictHitCount / (double)(counting.takenBranch + counting.nottakenBranch) * 100;
    }

    // Calculate cache HIT rate
    double CacheHITrate = 0;
    if ((counting.cacheHITcount + counting.coldMISScount + counting.conflictMISScount) != 0) {
        CacheHITrate = (double)counting.cacheHITcount / (double)(counting.cacheHITcount + counting.coldMISScount + counting.conflictMISScount) * 100;
    }

    // Print summary
    switch (*Predictor) {
        case '1' :
            printf("Branch Predictor : One-level\n");
            break;

        case '2' :
            printf("Branch Predictor : Gshare\n");
            break;

        case '3' :
            printf("Branch Predictor : Local\n");
            break;

        case '4' :
            printf("Branch Predictor : Always taken\n");
            break;

        case '5' :
            printf("Branch Predictor : Always not-taken\n");
            break;

        case '6' :
            printf("Branch Predictor : Backward Taken, Forward Not Taken\n");
            break;
    }
    switch (*Predictbit) {
        case 0 :
            break;

        case 1 :
            printf("Prediction Bit : 1-bit\n");
            break;

        case 2 :
            printf("Prediction Bit : 2-bit\n");
            break;
    }
    switch (*Counter) {
        case '0' :
            break;

        case '1' :
            printf("Counter : Saturating\n");
            break;

        case '2' :
            printf("Counter : Hysteresis\n");
            break;
    }
    printf("\n");
    printf("Final return value R[2] = %d\n", R[2]);
    printf("# of clock cycles : %d\n", counting.cycle);
    printf("# of register operations : %d\n", counting.RegOpcount);
    printf("# of taken branches : %d\n", counting.takenBranch);
    printf("# of not taken branches : %d\n", counting.nottakenBranch);
    printf("# of total branches : %d", counting.takenBranch + counting.nottakenBranch);
    printf("# of branch prediction HIT : %d\n", counting.PredictHitCount);
    printf("HIT rate of branch prediction : %.2lf %%\n", BranchHITrate);
    printf("# of stalling : %d\n", counting.stall);
    printf("# of memory operation instructions : %d\n", counting.Memcount);
    printf("# of cache HIT : %d\n", counting.cacheHITcount);
    printf("# of cache MISS : %d\n", counting.coldMISScount + counting.conflictMISScount);
    printf("HIT rate of cache access : %.2lf %%\n", CacheHITrate);
    return;
}