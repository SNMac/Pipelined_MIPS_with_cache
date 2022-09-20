//
// Created by SNMac on 2022/06/01.
//

#ifndef CAMP_PROJECT4_MAIN_H
#define CAMP_PROJECT4_MAIN_H

typedef struct _COUNTING {
    unsigned long long cycle;  // Clock cycle count
    int RegOpcount;  // Register operation count
    int Memcount;  // Memory access instruction count
    int PredictHitCount;  // Branch prediction count
    int takenBranch;  // taken Branch count
    int nottakenBranch;  // not taken Branch count
    int cacheHITcount;
    int coldMISScount;
    int conflictMISScount;
    int stall;
}COUNTING;

void FileSelect(char** filename);
void ReadDirectory(char** files, char* filepath);
char PredSelect(void);
int PBSelect(void);
char CounterSelect(void);
int CacheSizeSelect(void);
int CacheWaySelect(void);
int CacheWriteSelect(void);
void CacheSetting(const int* Cacheway, const int* Cachesize);
void FreeCache(const int* Cacheway, const int* Cachesize);

void OnelevelPredict(const int* Predictbit, const char* Counter, const int* Cacheway, const int* Cachesize, const int* Cachewrite);
void GsharePredict(const int* Predictbit, const char* Counter, const int* Cacheway, const int* Cachesize, const int* Cachewrite);
void LocalPredict(const int* Predictbit, const char* Counter, const int* Cacheway, const int* Cachesize, const int* Cachewrite);
void AlwaysTaken(const int* Cacheway, const int* Cachesize, const int* Cachewrite);
void AlwaysnotTaken(const int* Cacheway, const int* Cachesize, const int* Cachewrite);
void BTFNT(const int* Cacheway, const int* Cachesize, const int* Cachewrite);

void Firstinit(const int* Predictbit);
void printnextPC(void);

void OnelevelPipelineHandsOver(void);
void GsharePipelineHandsOver(void);
void LocalPipelineHandsOver(void);
void AlwaysTakenPipelineHandsOver(void);
void BTFNTPipelineHandsOver(void);

void printFinalresult(const char* Predictor, const int* Predictbit, const char* filename, const char* Counter,
                      const int* Cacheway, const int* Cachesize, const int* Cachewrite);

#endif //CAMP_PROJECT4_MAIN_H