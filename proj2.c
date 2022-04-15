#include <stdio.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>

typedef struct{
    int NO;
    int NH;
    int TI;
    int TB;
} args;

int actionCountSHM;
int writeSemSHM;

FILE *f;
sem_t* writeSem;
int *actionCount;



// handles errors with memory allocation, semaphores, etc.
void handleErrors(){
    fprintf(stderr, "Nastala chyba, exiting\n");
    exit(1);
}

// if arguments are in correct format, returns struct pointer with parsed args
args* parseArgs(){
    return NULL;
}

// function for oxygen processes
int oxygen(int idO, args *ar){

    // "O started" print
    sem_wait(writeSem);
    fprintf(f, "%d: O %d started\n", *actionCount, idO);
    fflush(f);
    (*actionCount)++;
    sem_post(writeSem);

    // sleeps for a random time period (up to TI), then goes to queue
    srand(idO);
    usleep(rand() % ar->TI);
    sem_wait(writeSem);
    fprintf(f, "%d: O %d going to queue\n", *actionCount, idO);
    fflush(f);
    (*actionCount)++;
    sem_post(writeSem);
    exit(0);
}

// function for hydrogen processes
int hydrogen(int idH, args *ar){

    // "H started" print
    sem_wait(writeSem);
    fprintf(f, "%d: H %d started\n", *actionCount, idH);
    fflush(f);
    (*actionCount)++;
    sem_post(writeSem);

    // sleeps for a random time period (up to TI), then goes to queue
    srand(idH);
    usleep(rand() % ar->TI);
    sem_wait(writeSem);
    fprintf(f, "%d: H %d going to queue\n", *actionCount, idH);
    fflush(f);
    (*actionCount)++;
    sem_post(writeSem);
    exit(0);
}

// initializes shared variables
int prepareMemory(){
    if((actionCountSHM = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | 0666)) == -1) handleErrors();
    if((writeSemSHM = shmget(IPC_PRIVATE, sizeof(sem_t), IPC_CREAT | IPC_EXCL | 0666)) == -1) handleErrors();
    if((writeSem = shmat(writeSemSHM, NULL, 0)) == (void*) -1) handleErrors();
    if((actionCount = shmat(actionCountSHM, NULL, 0)) == (void*) -1) handleErrors();
    return 1;
}

// frees allocated memory including shared memory
void freeMemory(){
    sem_destroy(writeSem);
    shmctl(actionCountSHM, IPC_RMID, NULL);
    shmctl(writeSemSHM, IPC_RMID, NULL);
    shmdt(actionCount);
    shmdt(writeSem);
    fclose(f);
}


int main(){
    args* ar = malloc(sizeof (args));
    ar->NO = 5;
    ar->NH = 5;
    ar->TI = 1000;
    ar->TB = 1000;

    f = fopen("out.txt", "w+");

    prepareMemory();

    sem_init(writeSem, 1, 1);
    *actionCount = 1;

    int oxygenId;
    int hydrogenId;
    int idH = 0;
    int idO = 0;

    for(int i = 0; i < ar->NO; i++){
        oxygenId = fork();
        idO++;
        if(oxygenId < 0){
            handleErrors();
        }
        if(oxygenId > 0){
            oxygen(idO, ar);
        }
        if(oxygenId == 0){

        }
    }
    for(int i = 0; i < ar->NH; i++){
        hydrogenId = fork();
        idH++;
        if(hydrogenId < 0){
            handleErrors();
        }
        if(hydrogenId > 0){
            hydrogen(idH, ar);
        }
        if(hydrogenId == 0){

        }

    }
    free(ar);
    freeMemory();
    exit(0);
}


