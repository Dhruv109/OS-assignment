#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <time.h>
#include <errno.h>

#define BILLION 1000000000.0

// Globals
int I, J, K;

// Shared memory
int shmid1, shmid2;
int *s1, *s2;
int **mul;

typedef struct thread_args
{
    int thread_id;
    // for in1
    int start_row;
    int end_row;
    // for in2
    int start_col;
    int end_col;

} thread_args;

void *thread_multiply(void *args)
{
    thread_args *t_info = args;

    for (int i = t_info->start_row; i < t_info->end_row && i < I; i++)
    {
        for (int j = 0; j < K; j++)
        {
            // printf("Thread %d : %d %d\n", t_info->thread_id, i, j);
            mul[i][j] = 0;
            for (int k = 0; k < J; k++)
            {
                mul[i][j] += s1[i * J + k] * s2[k * K + j];
                // printf("%d ", s1[i * J + k]);
            }
            // printf("\n");
        }
    }
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
    // take command line inputs
    if (argc != 7)
    {
        printf("Error in arguments");
    }

    I = atoi(argv[1]);
    J = atoi(argv[2]);
    K = atoi(argv[3]);

    // Opening file to write result
    FILE *res = fopen("p2_timetaken.txt", "wc");

    // creating shared memory

    key_t key1 = ftok("p1.c", 51);
    key_t key2 = ftok("p2.c", 39);

    // Removing shared memory
    if ((shmid1 = shmget(key1, sizeof(int) * I * J, 0666)) == -1)
    {
        perror("Error in shmget 1\n");
        exit(1);
    }

    if ((shmid2 = shmget(key2, sizeof(int) * J * K, 0666)) == -1)
    {
        perror("Error in shmget 2\n");
        exit(1);
    }

    s1 = shmat(shmid1, NULL, 0);
    s2 = shmat(shmid2, NULL, 0);

    if (s1 == (int *)-1 || s2 == (int *)-1)
    {
        perror("Error attaching to shared memory\n");
        exit(1);
    }
    // printMatrixOneD(s1, I, J);
    // printMatrixOneD(s2, J, K);

    // printf("%d ", s1[0]);
    mul = (int **)malloc(I * sizeof(long long int *));
    for (int i = 0; i < I; i++)
        mul[i] = (int *)malloc(K * sizeof(long long int));
    // Threading
    clock_t startTime, endTime;
    struct timespec start, end;
    for (int i = 0; i < 8; ++i)
    {
        int tc = pow(2, i);
        // printf("%d ", tc);
        int rowsToRead = I % tc == 0 ? I / tc : I / tc + 1; // # of rows a thread will read from in1

        if (tc > I)
            break;
        pthread_t *threads;
        threads = (pthread_t *)malloc(tc * sizeof(pthread_t));

        startTime = clock();
        int sp_row = 0;
        for (int j = 0; j < tc; j++)
        {
            thread_args *t = malloc(sizeof(thread_args));
            // for in1
            t->thread_id = j;
            t->start_row = sp_row;
            t->end_row = sp_row + rowsToRead;
            sp_row += rowsToRead;

            pthread_create(&threads[j], NULL, thread_multiply, t);
        }

        for (int j = 0; j < tc; j++)
        {
            pthread_join(threads[j], NULL);
        }

        // printMatrix(mul);

        endTime = clock();
        double cpu_time_used = ((double)(endTime - startTime)) / CLOCKS_PER_SEC;

        // printf("Time taken with %d threads = %f \n", tc, cpu_time_used);
        fprintf(res, "%lf, ", cpu_time_used);
    }
    // Removing shared memory
    fclose(res);
    shmctl(shmid1, IPC_RMID, NULL);
    shmctl(shmid2, IPC_RMID, NULL);
    return 0;
}
