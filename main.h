//
// Created by SNMac on 2022/06/01.
//

#ifndef CAMP_PROJECT4_MAIN_H
#define CAMP_PROJECT4_MAIN_H

typedef struct _COUNTING {
    int cycle;  // Clock cycle count
    int RegOpcount;  // Register operation count
    int Memcount;  // Memory access instruction count
    int PredictHitCount;  // Branch prediction count
    int takenBranch;  // taken Branch count
    int nottakenBranch;  // not taken Branch count
    int cacheHITcount;
    int coldMISScount;
    int conflictMISScount;
    int cacheMISScount;
    int stall;
}COUNTING;

void FileSelect(char** name);
void ReadDirectory(char** files, char** directory);
char PredSelect(void);
int PBSelect(void);
char CounterSelect(void);
int CacheSetSelect(void);
int CacheSizeSelect(void);
void CacheSetting(const int* Cacheset, const int* Cachesize);
void FreeCache(const int* Cacheset, const int* Cachesize);

void OnelevelPredict(const int* Predictbit, const char* Counter, const int* Cacheset, const int* Cachesize);
void GsharePredict(const int* Predictbit, const char* Counter, const int* Cacheset, const int* Cachesize);
void LocalPredict(const int* Predictbit, const char* Counter, const int* Cacheset, const int* Cachesize);
void AlwaysTaken(const int* Cacheset, const int* Cachesize);
void AlwaysnotTaken(const int* Cacheset, const int* Cachesize);
void BTFNT(const int* Cacheset, const int* Cachesize);

void Firstinit(const int* Predictbit);
void printnextPC(void);

void OnelevelPipelineHandsOver(void);
void GsharePipelineHandsOver(void);
void LocalPipelineHandsOver(void);
void AlwaysTakenPipelineHandsOver(void);
void BTFNTPipelineHandsOver(void);

void printFinalresult(const char* Predictor, const int* Predictbit, const char* filename, const char* Counter);

#endif //CAMP_PROJECT4_MAIN_H