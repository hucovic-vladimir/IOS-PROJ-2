/**
 * @file proj2.c
 * @author Vladimír Hucovič, FIT VUT
 * @date 18.04.2022
 * IOS, projekt 2 - Building H2O
 */

#include <stdio.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <string.h>

#define ERR_MEMORY 1
#define ERR_FORK 2
#define ERR_SEM 3

typedef struct{
    int NO;
    int NH;
    int TI;
    int TB;
} argsStruct;

struct sharedMemoryStruct{
    // shared counters for actions (A) and molecules (noM)
    int actionCount;
    int moleculeCount;

    // shared counters for remaining atoms
    int remainingHydrogen;
    int remainingOxygen;

    // these two semaphores guard access to a single molecule. only 1 oxygen and 2 hydrogen are allowed in
    sem_t* oxygenBarrier;
    sem_t* hydrogenBarrier;

    // oxygen uses this semaphore to inform hydrogens that the molecule is done
    sem_t* moleculeBarrier;

    // stops oxygen from finishing the molecule before both hydrogens can report that they are creating
    sem_t* hydrogenCreating;

    // these semaphores signal when there is enough oxygen and hydrogen to produce a molecule
    sem_t* oxygenReady;
    sem_t* hydrogenReady;

    // this semaphore guards access to the output file
    sem_t* writeSem;

    // this semaphore is here to ensure that the main process exits only after all child processes have exited
    sem_t* exitSem;
};

// global shared memory and output file
struct sharedMemoryStruct* shm;
FILE *f;

// function prototypes
void handleErrors(int errCode);
int parseArgs(int argc, char** argv, argsStruct* ar);
void oxygen(int idO, argsStruct *ar);
void hydrogen(int idH, argsStruct *ar);
void initializeSharedMemory();
void initializeSemaphores();
void destroySemaphores();
void freeMemory();


// handles errors with memory allocation and forking
void handleErrors(int errCode){
    if(errCode == ERR_FORK){
        fprintf(stderr, "Error: failed to create oxygen or hydrogen. Exiting..\n");
        freeMemory();
    }
    if(errCode == ERR_SEM){
        fprintf(stderr, "Error: failed to initialize semaphores. Exiting..\n");
    }
    if(errCode == ERR_MEMORY){
        fprintf(stderr, "Error: failed to allocate memory. Exiting..\n");
    }
    exit(1);
}

// if arguments are in correct format, fill ar with argument values
int parseArgs(int argc, char** argv, argsStruct* ar){
    if(argc > 5 || argc < 5){
        fprintf(stderr, "Error: Program needs exactly 4 arguments.\nUsage: ./proj2 <NO> <NH> <TI> <TB>\n");
        return 0;
    }

    // pointer to a string that stores non-numeric characters in args (parsing fails if it is not empty)
    char** endPtr = malloc(sizeof(char*));
    if(!endPtr) handleErrors(ERR_MEMORY);

    *endPtr = "";

    // stores the return value of strtol
    int argument;

    // arguments are stored in "ar" struct variables, or an error occurs and 0 is returned
    for (int i = 1; i < argc; ++i) {
        argument = strtol(argv[i], endPtr, 10);
        if(strcmp(*endPtr, "")){
            fprintf(stderr, "Error: Program needs 4 integer arguments.\nUsage: ./proj2 <NO> <NH> <TI> <TB>\n");
            free(endPtr);
            return 0;
        }
        else{
            switch (i) {
                case 1:
                    if(argument < 0){
                        fprintf(stderr, "Error: <NO> must be an integer value greater or equal to 0\n");
                        free(endPtr);
                        return 0;
                    }
                    ar->NO = argument;
                    break;
                case 2:
                    if(argument < 0){
                        fprintf(stderr, "Error: <NH> must be an integer value greater or equal to 0\n");
                        free(endPtr);
                        return 0;
                    }
                    ar->NH = argument;
                    break;
                case 3:
                    if(argument > 1000 || argument < 0){
                        fprintf(stderr, "Error: argument <TI> out of range <0-1000>\n");
                        free(endPtr);
                        return 0;
                    }
                    ar->TI = argument;
                    break;
                case 4:
                    if(argument > 1000 || argument < 0){
                        fprintf(stderr, "Error: argument <TB> out of range <0-1000>\n");
                        free(endPtr);
                        return 0;
                    }
                    ar->TB = argument;
                    break;
                default:
                    break;
            }
        }
    }
    free(endPtr);
    return 1;
}

// function for oxygen processes
void oxygen(int idO, argsStruct *ar){

    // "O started" print
    sem_wait(shm->writeSem);
    fprintf(f, "%d: O %d: started\n", shm->actionCount, idO);
    fflush(f);
    shm->actionCount++;
    sem_post(shm->writeSem);

    // sleeps for a random time period (up to TI), then goes to queue
    srand(time(NULL) * idO);
    usleep(1000 * (rand() % (ar->TI+1)));
    sem_wait(shm->writeSem);
    fprintf(f, "%d: O %d: going to queue\n", shm->actionCount, idO);
    fflush(f);
    shm->actionCount++;
    sem_post(shm->writeSem);

    // 1 oxygen is let in the molecule
    sem_wait(shm->oxygenBarrier);

    // if there is not enough remaining hydrogen, exit the process
    if(shm->remainingHydrogen < 2){
        sem_wait(shm->writeSem);
        fprintf(f, "%d: O %d: not enough H\n", shm->actionCount, idO);
        fflush(f);
        shm->actionCount++;
        shm->remainingHydrogen--;
        sem_post(shm->writeSem);
        sem_post(shm->oxygenBarrier);
        sem_post(shm->exitSem);
        exit(0);
    }

    //waiting for hydrogens to be ready
    sem_wait(shm->hydrogenReady);
    sem_wait(shm->hydrogenReady);
    // decrement remaining oxygen counter
    shm->remainingOxygen--;

    // this oxygen is ready, increment moleculeCount and free 2 hydrogens from the queue
    shm->moleculeCount++;
    sem_post(shm->oxygenReady);
    sem_post(shm->oxygenReady);
    sem_wait(shm->writeSem);
    fprintf(f, "%d: O %d: creating molecule %d\n", shm->actionCount, idO, shm->moleculeCount);
    fflush(f);
    shm->actionCount++;
    sem_post(shm->writeSem);

    // create the molecule
    usleep(1000 * (rand() % (ar->TB+1)));
    sem_wait(shm->hydrogenCreating);
    sem_wait(shm->hydrogenCreating);
    sem_wait(shm->writeSem);
    fprintf(f, "%d: O %d: molecule %d created\n", shm->actionCount, idO, shm->moleculeCount);
    fflush(f);
    shm->actionCount++;
    sem_post(shm->writeSem);

    // inform hydrogens that molecule is done
    sem_post(shm->moleculeBarrier);
    sem_post(shm->moleculeBarrier);

    // free the spot for another oxygen
    sem_post(shm->oxygenBarrier);

    sem_post(shm->exitSem);
    exit(0);
}

// function for hydrogen processes
void hydrogen(int idH, argsStruct *ar){

    // "H started" print
    sem_wait(shm->writeSem);
    fprintf(f, "%d: H %d: started\n", shm->actionCount, idH);
    fflush(f);
    shm->actionCount++;
    sem_post(shm->writeSem);

    // sleeps for a random time period (up to TI), then goes to queue
    srand(time(NULL) * idH);
    usleep(1000 * (rand() % (ar->TI+1)));
    sem_wait(shm->writeSem);
    fprintf(f, "%d: H %d: going to queue\n", shm->actionCount, idH);
    fflush(f);
    shm->actionCount++;
    sem_post(shm->writeSem);

    // 2 hydrogens are let in the molecule
    sem_wait(shm->hydrogenBarrier);

    // exits if there is not enough hydrogen or oxygen remaining
    if(shm->remainingHydrogen <= 1 || shm->remainingOxygen < 1){
        sem_wait(shm->writeSem);
        fprintf(f, "%d: H %d: not enough O or H\n", shm->actionCount, idH);
        fflush(f);
        shm->actionCount++;
        sem_post(shm->writeSem);

        shm->remainingHydrogen--;
        sem_post(shm->hydrogenBarrier);
        sem_post(shm->exitSem);
        exit(0);
    }

    // signal that this hydrogen is ready
    sem_post(shm->hydrogenReady);

    // wait for oxygen to be ready
    sem_wait(shm->oxygenReady);

    shm->remainingHydrogen--;

    // create molecule
    sem_wait(shm->writeSem);
    fprintf(f, "%d: H %d: creating molecule %d\n", shm->actionCount, idH, shm->moleculeCount);
    fflush(f);
    (shm->actionCount)++;
    sem_post(shm->writeSem);

    // inform oxygen that hydrogen has started "creating"
    // (so that oxygen doesn't finish the molecule before both hydrogens can say that they are creating)
    sem_post(shm->hydrogenCreating);

    // waits for oxygen to make the molecule
    sem_wait(shm->moleculeBarrier);


    // molecule created
    sem_wait(shm->writeSem);
    fprintf(f, "%d: H %d: molecule %d created\n", shm->actionCount, idH, shm->moleculeCount);
    fflush(f);
    (shm->actionCount)++;
    sem_post(shm->writeSem);

    // free the spot for another hydrogen
    sem_post(shm->hydrogenBarrier);
    sem_post(shm->exitSem);
    exit(0);
}

// initializes shared memory and sets default values, calls error handling function if it fails
void initializeSharedMemory(){
    if((shm = mmap(NULL, sizeof(struct sharedMemoryStruct), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS,
                   -1, 0)) == NULL) handleErrors(ERR_MEMORY);
    if((shm->writeSem = mmap(NULL, sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS,
                             -1, 0)) == NULL) handleErrors(ERR_MEMORY);
    if((shm->oxygenBarrier = mmap(NULL, sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS,
                                  -1, 0)) == NULL) handleErrors(ERR_MEMORY);
    if((shm->hydrogenBarrier = mmap(NULL, sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS,
                                    -1, 0)) == NULL) handleErrors(ERR_MEMORY);
    if((shm->moleculeBarrier = mmap(NULL, sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS,
                                    -1, 0)) == NULL) handleErrors(ERR_MEMORY);
    if((shm->hydrogenReady = mmap(NULL, sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS,
                                    -1, 0)) == NULL) handleErrors(ERR_MEMORY);
    if((shm->oxygenReady = mmap(NULL, sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS,
                                  -1, 0)) == NULL) handleErrors(ERR_MEMORY);
    if((shm->hydrogenCreating = mmap(NULL, sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS,
                                -1, 0)) == NULL) handleErrors(ERR_MEMORY);
    if((shm->exitSem = mmap(NULL, sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS,
                                     -1, 0)) == NULL) handleErrors(ERR_MEMORY);
}

void initializeSemaphores(){
    if(sem_init(shm->writeSem, 1, 1)) handleErrors(ERR_SEM);
    if(sem_init(shm->oxygenBarrier, 1, 1)) handleErrors(ERR_SEM);
    if(sem_init(shm->hydrogenBarrier, 1, 2)) handleErrors(ERR_SEM);
    if(sem_init(shm->moleculeBarrier, 1, 0)) handleErrors(ERR_SEM);
    if(sem_init(shm->oxygenReady, 1, 0)) handleErrors(ERR_SEM);
    if(sem_init(shm->hydrogenReady, 1, 0)) handleErrors(ERR_SEM);
    if(sem_init(shm->hydrogenCreating, 1, 0)) handleErrors(ERR_SEM);
    if(sem_init(shm->exitSem, 1, 0)) handleErrors(ERR_SEM);
}

void destroySemaphores(){
    sem_destroy(shm->writeSem);
    sem_destroy(shm->oxygenBarrier);
    sem_destroy(shm->hydrogenBarrier);
    sem_destroy(shm->moleculeBarrier);
    sem_destroy(shm->hydrogenReady);
    sem_destroy(shm->oxygenReady);
    sem_destroy(shm->hydrogenCreating);
    sem_destroy(shm->exitSem);
}

// frees shared memory
void freeMemory(){
    munmap(shm->writeSem, sizeof(sem_t));
    munmap(shm->oxygenBarrier, sizeof(sem_t));
    munmap(shm->hydrogenBarrier, sizeof(sem_t));
    munmap(shm->moleculeBarrier, sizeof(sem_t));
    munmap(shm->hydrogenReady, sizeof(sem_t));
    munmap(shm->oxygenReady, sizeof(sem_t));
    munmap(shm->hydrogenCreating, sizeof(sem_t));
    munmap(shm->exitSem, sizeof(sem_t));
    munmap(shm, sizeof(struct sharedMemoryStruct));
    fclose(f);
}


int main(int argc, char **argv){
    argsStruct* ar = malloc(sizeof(argsStruct));
    if(!ar){
        handleErrors(ERR_MEMORY);
    }

    // fill the arg struct with arguments or exit
    if(!parseArgs(argc, argv, ar)){
        exit(1);
    }

    f = fopen("proj2.out", "w+");

    initializeSharedMemory();

    // set default values to shared memory variables
    shm->actionCount = 1;
    shm->moleculeCount = 0;
    shm->remainingHydrogen = ar->NH;
    shm->remainingOxygen = ar->NO;

    initializeSemaphores();

    // PIDs
    int oxygenPID;
    int hydrogenPID;

    // unique atom identifiers
    int idH = 0;
    int idO = 0;

    // fork into oxygens
    for(int i = 0; i < ar->NO; i++){
        oxygenPID = fork();
        idO++;
        if(oxygenPID < 0){
            handleErrors(ERR_FORK);
        }
        if(oxygenPID > 0){
            oxygen(idO, ar);
        }
        if(oxygenPID == 0){
            continue;
        }
    }

    // fork into hydrogens
    for(int i = 0; i < ar->NH; i++){
        hydrogenPID = fork();
        idH++;
        if(hydrogenPID < 0){
            handleErrors(ERR_FORK);
        }
        if(hydrogenPID > 0){
            hydrogen(idH, ar);
        }
        if(hydrogenPID == 0){
            continue;
        }
    }

    // wait for all child processes to finish
    for (int i = 0; i < ar->NO + ar->NH; ++i) {
        sem_wait(shm->exitSem);
    }

    // clean up and exit
    free(ar);
    destroySemaphores();
    freeMemory();
    exit(0);
}
