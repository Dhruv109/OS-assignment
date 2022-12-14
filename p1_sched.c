#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <errno.h>
#include <semaphore.h>

#include "common_structs.h"

#define MAX_THREAD 16
#define THREAD_COUNT 4

// Lock
pthread_mutex_t lock;
// Matrix dimensions
int I, J, K;
//  Files
char *file1, *file2;
FILE *f1, *f2;
int *mp1, *mp2;

// Shared memory
int *shm_flags;
int *s1, *s2;

// gcc -pthread p1.c -o p1.out -lm

void *thread_read(void *args)
{
    thread_args *t_arg = args;
    // for in1
    // printf("%d ", t_arg->end_col);
    //  Read end_row - start_row number of rows
    for (int i = t_arg->start_row; i < t_arg->end_row && i < I; i++)
    {
        // printf("%d ", i);

        for (int col = 0; col < J; ++col)
        {
            pthread_mutex_lock(&lock);
            fseek(f1, mp1[i * J + col], SEEK_SET);
            fscanf(f1, "%d", &s1[i * J + col]);
            pthread_mutex_unlock(&lock);
            // printf("%d ", temp);
        }
    }

    // for in2
    for (int col = t_arg->start_col; col < t_arg->end_col && col < K; col++)
    {
        for (int i = 0; i < J; ++i)
        {
            // for each row scan
            pthread_mutex_lock(&lock);
            fseek(f2, mp2[i * K + col], SEEK_SET);
            fscanf(f2, "%d", &s2[i * K + col]);
            pthread_mutex_unlock(&lock);
        }
    }

    shm_flags[t_arg->thread_id] = 1;
    pthread_exit(NULL);
}
void printMatrixOneD(int *a, int R, int C)
{
    printf("\n");
    for (int i = 0; i < R; ++i)
    {
        for (int j = 0; j < C; ++j)
        {
            printf("%d ", a[i * C + j]);
        }
        printf("\n");
    }
    printf("\n");
}

int main(int argc, char const *argv[])
{

    if (argc != 7)
    {
        printf("Error in arguments");
    }

    printf("Starting P1\n");
    I = atoi(argv[1]);
    J = atoi(argv[2]);
    K = atoi(argv[3]);

    file1 = argv[4];
    file2 = argv[5];
    const char *out = argv[6];

    f1 = fopen(file1, "r");
    f2 = fopen(file2, "r");
    if (f1 == NULL)
    {
        printf("Error opening file 1\n");
        exit(1);
    }
    if (f2 == NULL)
    {
        printf("Error opening file 2\n");
        exit(1);
    }
    // Attaching to shared memory

    // Time logging
    key_t shm_time_logging = ftok("group44_assignment2.c", 54);
    int shm_id_time_logging = shmget(shm_time_logging, sizeof(time_logging), 0666);
    if (shm_id_time_logging == -1)
    {
        perror("Error in shm time logging P1\n");
        exit(1);
    }
    time_logging *time_log = shmat(shm_id_time_logging, NULL, 0);

    // Thread flags
    key_t shm_threadFlags = ftok("group44_assignment2.c", 53);
    int shmid_id_flags = shmget(shm_threadFlags, (THREAD_COUNT) * sizeof(int), 0666 | IPC_CREAT);

    if (shmid_id_flags == -1)
    {
        perror("Error in shmget main_flags \n");
        exit(1);
    }
    shm_flags = (int *)shmat(shmid_id_flags, NULL, 0);

    // Process data
    key_t shm_proc_data = ftok("group44_assignment2.c", 50);
    int shm_id_proc_data = shmget(shm_proc_data, sizeof(proc_data), 0666);
    if (shm_id_proc_data == -1)
    {
        perror("Error in attach proc data \n");
        exit(1);
    }
    proc_data *shared_proc_data = (proc_data *)shmat(shm_id_proc_data, NULL, 0);
    if (shared_proc_data == (proc_data *)-1)
    {
        perror("Error attaching P1 \n");
        exit(errno);
    }

    // Matrix 1 and 2
    key_t shm_matrix1 = ftok("group44_assignment2.c", 51);
    key_t shm_matrix2 = ftok("group44_assignment2.c", 52);
    int shm_id_matrix1 = shmget(shm_matrix1, (I * J) * sizeof(int), 0666);
    if (shm_id_matrix1 == -1)
    {
        perror("P1: Error in shmget matrix 1 ");
        exit(1);
    }
    int shm_id_matrix2 = shmget(shm_matrix2, (J * K) * sizeof(int), 0666);
    if (shm_id_matrix2 == -1)
    {
        perror("P1: Error in shmget matrix 2");
        exit(1);
    }

    s1 = shmat(shm_id_matrix1, NULL, 0);
    s2 = shmat(shm_id_matrix2, NULL, 0);

    if (s1 == (int *)-1 || s2 == (int *)-1)
    {
        perror("Error attaching to shared memory\n");
        exit(1);
    }
    // File 1 map
    key_t shm_file1_map = ftok("group44_assignment2.c", 58);
    int shm_id_file1_map = shmget(shm_file1_map, I * J * sizeof(int), 0666);
    if (shm_id_file1_map == -1)
    {
        perror("Error in shmget file 1 map");
        exit(1);
    }
    mp1 = shmat(shm_id_file1_map, NULL, 0);
    if (mp1 == (int *)-1)
    {
        perror("Error attaching to shared memory\n");
        exit(1);
    }

    // File 2 map
    key_t shm_file2_map = ftok("group44_assignment2.c", 59);
    int shm_id_file2_map = shmget(shm_file2_map, J * K * sizeof(int), 0666);
    if (shm_id_file2_map == -1)
    {
        perror("Error in shmget file 2 map");
        exit(1);
    }
    mp2 = shmat(shm_id_file2_map, NULL, 0);
    if (mp2 == (int *)-1)
    {
        perror("Error attaching to shared memory\n");
        exit(1);
    }

    int rowsToRead = I % THREAD_COUNT == 0 ? I / THREAD_COUNT : I / THREAD_COUNT + 1; // # of rows a thread will read from in1
    int colsToRead = K % THREAD_COUNT == 0 ? K / THREAD_COUNT : K / THREAD_COUNT + 1; // # of cols a thread will read from in2s

    // Initialising lock
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("\n mutex init has failed\n");
        return 1;
    }
    pthread_t *threads;
    threads = (pthread_t *)malloc(THREAD_COUNT * sizeof(pthread_t));

    clock_t start_time, end_time;
    start_time = clock();
    int sp_row = 0, sp_col = 0;

    for (int j = 0; j < THREAD_COUNT; j++)
    {
        thread_args *t = malloc(sizeof(thread_args));
        t->thread_id = j;

        // for in1
        t->start_row = sp_row;
        t->end_row = sp_row + rowsToRead;
        sp_row += rowsToRead;

        // for in2
        t->start_col = sp_col;
        t->end_col = sp_col + colsToRead;
        sp_col += colsToRead;

        pthread_create(&threads[j], NULL, thread_read, t);
    }

    for (int j = 0; j < THREAD_COUNT; j++)
    {
        pthread_join(threads[j], NULL);
    }
    end_time = clock();

    // Calculating turnaround time
    double turnAround_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    time_log->ta_time[0] = turnAround_time;

    // Mark p1 finished
    shared_proc_data->finished[0] = 1;

    fclose(f1);
    fclose(f2);
    return 0;
}