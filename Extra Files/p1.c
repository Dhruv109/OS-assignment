#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <errno.h>

#define MAX_THREAD_COUNT = 16

// One thread will read a row from in1 and a corresponding column from in2
// Will generate a seek map to find postions to read

// MATRIX SIZE
int I, J, K; // MAT 1 = i x j, MAT 2 = j x k

// Lock
pthread_mutex_t lock;

// Files
char *file1, *file2;
FILE *f1, *f2;
// Shared memory
int shmid1, shmid2;
int *s1, *s2;

// for mapping
int **mp1, **mp2;

// Arguments for thread
typedef struct thread_args
{
    // for in1
    int start_row;
    int end_row;
    // for in2
    int start_col;
    int end_col;

} thread_args;

void mapTheFile(const char *file, int **mp, int height, int width)
{
    FILE *f = fopen(file, "r");

    if (f == NULL)
    {
        printf("Error opening file while mapping \n");
        exit(1);
    }
    int temp, t, i = 0, j = 0;

    mp[i][j] = 0;

    while (!feof(f))
    {
        t = ftell(f);

        if (j >= width)
        {
            j = 0;
            i++;
        }
        if (i >= height)
            break;
        // printf("%d %d\n", i, j);
        mp[i][j] = t;
        j++;
        fscanf(f, "%d", &temp);
    }
    // for (int i = 0; i < height; ++i)
    // {
    //     for (int j = 0; j < width; ++j)
    //         printf("%d ", mp[i][j]);
    //     printf("\n");
    // }
    // printf("\n");
    fclose(f);
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
void *thread_read(void *args)
{
    thread_args *t_arg = args;

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

    // for in1

    //  Read end_row - start_row number of rows
    for (int i = t_arg->start_row; i < t_arg->end_row && i < I; i++)
    {
        // printf("%d ", i);

        for (int col = 0; col < J; ++col)
        {
            pthread_mutex_lock(&lock);
            fseek(f1, mp1[i][col], SEEK_SET);
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
            fseek(f2, mp2[i][col], SEEK_SET);
            fscanf(f2, "%d", &s2[i * K + col]);
            pthread_mutex_unlock(&lock);
        }
    }

    pthread_exit(NULL);
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

    // opening files
    file1 = argv[4];
    file2 = argv[5];

    FILE *res = fopen("p1_timetaken.txt", "wc");
    // mapping file 1
    mp1 = (int **)malloc(I * sizeof(int *));
    for (int i = 0; i < I; i++)
        mp1[i] = (int *)malloc(J * sizeof(int));

    // mapping file 2
    mp2 = (int **)malloc(J * sizeof(int *));
    for (int i = 0; i < J; i++)
        mp2[i] = (int *)malloc(K * sizeof(int));

    mapTheFile(argv[4], mp1, I, J);
    mapTheFile(argv[5], mp2, J, K);

    // // creating shared memory

    key_t key1 = ftok("p1.c", 51);
    key_t key2 = ftok("p2.c", 39);

    //  shared memory
    if ((shmid1 = shmget(key1, sizeof(int) * I * J, IPC_CREAT | 0666)) == -1)
    {
        perror("Error in shmget 1\n");
        exit(1);
    }

    if ((shmid2 = shmget(key2, sizeof(int) * J * K, IPC_CREAT | 0666)) == -1)
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

    f1 = fopen(file1, "r");
    f2 = fopen(file2, "r");

    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("\n mutex init has failed\n");
        return 1;
    }

    // Threading Starts here
    clock_t startTime, endTime;
    for (int i = 0; i < 8; ++i)
    {
        int tc = pow(2, i);
        int rowsToRead = I % tc == 0 ? I / tc : I / tc + 1; // # of rows a thread will read from in1
        int colsToRead = K % tc == 0 ? K / tc : K / tc + 1; // # of cols a thread will read from in2
        if (tc > I || tc > K)
            break;
        pthread_t *threads;
        threads = (pthread_t *)malloc(tc * sizeof(pthread_t));

        startTime = clock();
        int sp_row = 0, sp_col = 0;
        for (int n = 0; n < tc; n++)
        {
            thread_args *t = malloc(sizeof(thread_args));
            // for in1
            t->start_row = sp_row;
            t->end_row = sp_row + rowsToRead;
            sp_row += rowsToRead;

            // for in2
            t->start_col = sp_col;
            t->end_col = sp_col + colsToRead;
            sp_col += colsToRead;

            pthread_create(&threads[n], NULL, thread_read, t);
        }

        for (int j = 0; j < tc; j++)
        {
            pthread_join(threads[j], NULL);
        }
        endTime = clock();
        double cpu_time_used = ((double)(endTime - startTime)) / CLOCKS_PER_SEC;
        // printf("Time taken with %d threads = %f \n", tc, cpu_time_used);
        fprintf(res, "%lf, ", cpu_time_used);
    }
    // printMatrixOneD(s1, I, J);
    // printMatrixOneD(s2, J, K);
    fclose(f1);
    fclose(f2);
    // Removing shared memory
    // shmctl(shmid1, IPC_RMID, NULL);
    // shmctl(shmid2, IPC_RMID, NULL);
    return 0;
}