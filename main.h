//
// Created by SNMac on 2022/05/09.
//

#ifndef CAMP_PROJECT3_MAIN_H
#define CAMP_PROJECT3_MAIN_H

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
char PBSelect(void);
char CounterSelect(void);
char CacheSetSelect(void);
char CacheSizeSelect(void);

void OnelevelPredict(const char* Predictbit, const char* Counter, const char* Cacheset, const char* Cachesize);
void GsharePredict(const char* Predictbit, const char* Counter, const char* Cacheset, const char* Cachesize);
void LocalPredict(const char* Predictbit, const char* Counter, const char* Cacheset, const char* Cachesize);
void AlwaysTaken(const char* Cacheset, const char* Cachesize);
void AlwaysnotTaken(const char* Cacheset, const char* Cachesize);
void BTFNT(const char* Cacheset, const char* Cachesize);

void Firstinit(const char* Predictbit);
void printnextPC(void);

void OnelevelPipelineHandsOver(void);
void GsharePipelineHandsOver(void);
void LocalPipelineHandsOver(void);
void AlwaysTakenPipelineHandsOver(void);
void BTFNTPipelineHandsOver(void);

void printFinalresult(const char* Predictor, const char* Predictbit,
                      const char* filename, const char* Counter);

#endif //CAMP_PROJECT3_MAIN_H