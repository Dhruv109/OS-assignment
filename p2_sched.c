#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <time.h>
#include <errno.h>

#include "common_structs.h"

#define MAX_THREAD 16
#define THREAD_COUNT 4
int count = 0;

pthread_mutex_t lock;

// Matrix dimensions
int I, J, K;

// Shared memory
int *s1, *s2, *shm_flags;

// Output
int **mul;
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

void *thread_multiply(void *args)
{
    thread_args *t_info = args;
    // For new columns
    for (int i = 0; i < t_info->end_row && i < I; i++)
    {
        for (int j = t_info->start_col; j < t_info->end_col; j++)
        {

            mul[i][j] = 0;
            for (int k = 0; k < J; k++)
            {
                mul[i][j] += s1[i * J + k] * s2[k * K + j];
            }
        }
    }
    // For new rows
    for (int i = t_info->start_row; i < t_info->end_row && i < I; i++)
    {
        for (int j = 0; j < t_info->end_col; j++)
        {

            mul[i][j] = 0;
            for (int k = 0; k < J; k++)
            {
                mul[i][j] += s1[i * J + k] * s2[k * K + j];
            }
        }
    }
    pthread_exit(NULL);
}

void printMatrix(int **mul)
{
    printf("\n");
    for (int i = 0; i < I; ++i)
    {
        for (int j = 0; j < K; ++j)
        {
            printf("%d ", mul[i][j]);
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

    printf("Starting P2\n");

    I = atoi(argv[1]);
    J = atoi(argv[2]);
    K = atoi(argv[3]);

    // Initialsing mul
    mul = (int **)malloc(I * sizeof(long long int *));
    for (int i = 0; i < I; i++)
        mul[i] = (int *)malloc(K * sizeof(long long int));

    // Time logging
    key_t shm_time_logging = ftok("group44_assignment2.c", 54);
    int shm_id_time_logging = shmget(shm_time_logging, sizeof(time_logging), 0666);
    if (shm_id_time_logging == -1)
    {
        perror("Error in shm time logging P1\n");
        exit(1);
    }
    time_logging *time_log = shmat(shm_id_time_logging, NULL, 0);

    // Proc data
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

    // Shared matrix 1 and 2
    key_t shm_matrix1 = ftok("group44_assignment2.c", 51);
    key_t shm_matrix2 = ftok("group44_assignment2.c", 52);
    int shm_id_matrix1 = shmget(shm_matrix1, (I * J) * sizeof(int), 0666);
    if (shm_id_matrix1 == -1)
    {
        perror("Error in shmget matrix 1");
        exit(1);
    }
    int shm_id_matrix2 = shmget(shm_matrix2, (J * K) * sizeof(int), 0666);
    if (shm_id_matrix2 == -1)
    {
        perror("Error in shmget matrix 2");
        exit(1);
    }

    s1 = shmat(shm_id_matrix1, NULL, 0);
    s2 = shmat(shm_id_matrix2, NULL, 0);
    if (s1 == (int *)-1 || s2 == (int *)-1)
    {
        perror("Error attaching to shared memory\n");
        exit(1);
    }
    // printMatrixOneD(s1, I, J);
    // printMatrixOneD(s2, J, K);
    //    Thread flags
    key_t shm_threadFlags = ftok("group44_assignment2.c", 53);
    int shmid_id_flags = shmget(shm_threadFlags, (THREAD_COUNT) * sizeof(int), 0666 | IPC_CREAT);

    if (shmid_id_flags == -1)
    {
        perror("Error in shmget main_flags \n");
        exit(1);
    }
    shm_flags = (int *)shmat(shmid_id_flags, NULL, 0);

    // printf("P2 No of threads is %d \n",tc);
    int rowsToRead = I % THREAD_COUNT == 0 ? I / THREAD_COUNT : I / THREAD_COUNT + 1; // # of rows a thread will read from in1
    int colsToRead = K % THREAD_COUNT == 0 ? K / THREAD_COUNT : K / THREAD_COUNT + 1;
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
        t->end_col = sp_row + colsToRead;
        sp_col += colsToRead;

        while (shm_flags[t->thread_id] != 1)
        {
            ; // spin
        }
        // MAJOR PROBLEM: I only have limited cols of in2, but i need full in2 to multiply otherwise some places will be left
        // Thread ids are sequential so, i loop can be run from 0 everytime

        pthread_create(&threads[j], NULL, thread_multiply, t);
    }

    for (int j = 0; j < THREAD_COUNT; j++)
    {
        pthread_join(threads[j], NULL);
    }

    end_time = clock();

    // Calculating turnaround time
    double turnAround_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    time_log->ta_time[1] = turnAround_time;

    // printMatrix(mul);
    // Print matrix to file
    FILE *out = fopen(argv[6], "w");
    for (int i = 0; i < I; i++)
    {
        for (int j = 0; j < K; j++)
        {
            fprintf(out, "%d ", mul[i][j]);
        }
        fprintf(out, "\n");
    }
    // Mark p2 finished
    shared_proc_data->finished[1] = 1;
    return 0;
}