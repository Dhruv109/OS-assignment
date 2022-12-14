#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define NUMBER_OF_THREADS 4
#define TIME_SLICE 2000 // 1ms = 1000us

// Structs
typedef struct
{
    int p_id;
    double run_time;
    double wait_time;
    double admit_time;
    char *run_command;
    int priority;
    int running;
    int finished;
} process;

typedef struct
{
    int finished[2];

} isProcessFinished;

typedef struct
{
    double ta_time[2];
    double wt_time[2];
    double run_time[2];
    struct timespec switch_time[2];
} time_logging;

// Globals
int I, J, K;
char *in1, *in2, *out;
isProcessFinished *shared_proc_data;
time_logging *timings;
int shm_id_proc_data, shm_id_matrix1, shm_id_matrix2, flags_shm, shm_id_time_logging, shm_id_file1_map, shm_id_file2_map;
// Processes
pid_t process1, currentlyScheduledProcess, process2;
// Timing
struct timespec switchingTime_start[2], switchingTime_end[2], smallTime;
// Clocks
clock_t waitingTimeS[2], waitingTimeE[2];

enum
{
    NANOSECONDS = 1000000000
};

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

void mapTheFile(char *file, int *mp, int height, int width)
{

    FILE *f = fopen(file, "r");

    if (f == NULL)
    {
        printf("Error opening file while mapping \n");
        exit(1);
    }
    int temp, t, i = 0, j = 0;

    mp[0] = 0;

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
        mp[i * width + j] = t;
        j++;
        fscanf(f, "%d", &temp);
    }
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
void getSwitichingTime(int a, struct timespec a1, struct timespec a2, struct timespec *smallTime)
{
    // For sec
    smallTime->tv_sec = a2.tv_sec - a1.tv_sec;
    // For nsec
    smallTime->tv_nsec = a2.tv_nsec - a1.tv_nsec;

    if (smallTime->tv_sec < 0 && smallTime->tv_nsec > 0)
    {
        // printf("Trig sub 1\n");
        smallTime->tv_nsec -= NANOSECONDS;
        smallTime->tv_sec++;
        a++;
    }
    else if (smallTime->tv_sec > 0 && smallTime->tv_nsec < 0)
    {
        // printf("Trig sub 2\n");
        smallTime->tv_nsec += NANOSECONDS;
        smallTime->tv_sec--;
        a--;
    }
}

void createSharedMemory()
{
    int s1, s2; // status flags

    // creating shared memory for process data
    key_t shm_finish_data = ftok("group44_assignment2.c", 50);
    // Proc data has if a process has finished or not
    shm_id_proc_data = shmget(shm_finish_data, sizeof(isProcessFinished), 0666 | IPC_CREAT);
    if (shm_id_proc_data == -1)
    {
        perror("Error cant create finished memory");
        exit(1);
    }
    shared_proc_data = (isProcessFinished *)shmat(shm_id_proc_data, NULL, 0);
    // Checking if the memory created is correct
    if (shared_proc_data != (isProcessFinished *)-1)
    {
        // OK
    }
    else if (shared_proc_data == (isProcessFinished *)-1)
    {
        perror("ERROR:  shm finish data\n");
        exit(1);
    }
    // Marking both process1 and process2 as not finished
    shared_proc_data->finished[0] = 0;
    shared_proc_data->finished[1] = 0;

    // Shared memory for matrices input
    key_t shm_matrix1 = ftok("group44_assignment2.c", 51);
    key_t shm_matrix2 = ftok("group44_assignment2.c", 52);
    shm_id_matrix1 = shmget(shm_matrix1, (I * J) * sizeof(int), 0666 | IPC_CREAT);
    if (shm_id_matrix1 == -1)
    {
        perror("Error in shmget matrix 1");
        exit(1);
    }
    shm_id_matrix2 = shmget(shm_matrix2, (J * K) * sizeof(int), 0666 | IPC_CREAT);
    if (shm_id_matrix2 == -1)
    {
        perror("Error in shmget matrix 2");
        exit(1);
    }

    // Shared memory for mapping files
    key_t shm_file1_map = ftok("group44_assignment2.c", 58);
    shm_id_file1_map = shmget(shm_file1_map, I * J * sizeof(int), 0666 | IPC_CREAT);
    if (shm_id_file1_map == -1)
    {
        perror("Error in shmget file 1 map");
        exit(1);
    }

    int *mp1 = shmat(shm_id_file1_map, NULL, 0);
    mapTheFile(in1, mp1, I, J);

    // File 2 map
    key_t shm_file2_map = ftok("group44_assignment2.c", 59);
    shm_id_file2_map = shmget(shm_file2_map, J * K * sizeof(int), 0666 | IPC_CREAT);
    if (shm_id_file2_map == -1)
    {
        perror("Error in shmget file 2 map");
        exit(1);
    }
    int *mp2 = shmat(shm_id_file2_map, NULL, 0);
    mapTheFile(in2, mp2, J, K);

    // Shared memory for thread flags (denote if that thread has completed its work)
    key_t shm_threadFlags = ftok("group44_assignment2.c", 53);
    flags_shm = shmget(shm_threadFlags, (NUMBER_OF_THREADS) * sizeof(int), 0666 | IPC_CREAT);
    // flags here
    if (flags_shm == -1)
    {
        perror("Error in shmget main_flags \n");
        exit(1);
    }
    int *flags = (int *)shmat(flags_shm, NULL, 0);
    int t = 0;
    while (t < NUMBER_OF_THREADS)
    {
        // initialize flags to 0
        flags[t] = 0;
        t++;
    }

    // Shared memory for time logging
    key_t shm_time_logging = ftok("group44_assignment2.c", 54);
    shm_id_time_logging = shmget(shm_time_logging, sizeof(time_logging), 0666 | IPC_CREAT);
    if (shm_id_time_logging == -1)
    {
        perror("Error in shm time logging\n");
        exit(1);
    }
    timings = shmat(shm_id_time_logging, NULL, 0);
}

void deleteSharedMemory()
{
    shmctl(shm_id_proc_data, IPC_RMID, NULL);
    shmctl(shm_id_matrix1, IPC_RMID, NULL);
    shmctl(shm_id_matrix2, IPC_RMID, NULL);
    shmctl(flags_shm, IPC_RMID, NULL);
    shmctl(shm_id_time_logging, IPC_RMID, NULL);
    shmctl(shm_id_file1_map, IPC_RMID, NULL);
    shmctl(shm_id_file2_map, IPC_RMID, NULL);
}

void continueProcess(int i)
{
    clock_gettime(CLOCK_REALTIME, &switchingTime_start[i]); // For switching overhead
    kill(currentlyScheduledProcess, SIGCONT);               // Resume process
    clock_gettime(CLOCK_REALTIME, &switchingTime_end[i]);
    getSwitichingTime(1, switchingTime_start[i], switchingTime_end[i], &smallTime);
    timings->switch_time[i].tv_sec += (int)smallTime.tv_sec;
    timings->switch_time[i].tv_nsec += smallTime.tv_nsec;

    waitingTimeE[i] = clock();
    timings->wt_time[i] += ((double)(waitingTimeE[i] - waitingTimeS[i])) / CLOCKS_PER_SEC;
}
void stopProcess(int i)
{
    clock_gettime(CLOCK_REALTIME, &switchingTime_start[i]); // For switching overhead
    kill(currentlyScheduledProcess, SIGSTOP);
    clock_gettime(CLOCK_REALTIME, &switchingTime_end[i]);
    getSwitichingTime(0, switchingTime_start[i], switchingTime_end[i], &smallTime);

    timings->switch_time[i].tv_sec += (int)smallTime.tv_sec;
    timings->switch_time[i].tv_nsec += smallTime.tv_nsec;
}

int main(int argc, char *argv[])
{
    if (argc != 7)
    {
        printf("Error in arguments");
    }

    I = atoi(argv[1]);
    J = atoi(argv[2]);
    K = atoi(argv[3]);

    in1 = argv[4];
    in2 = argv[5];
    out = argv[6];

    // Opening files to write results
    FILE *ta_wl1, *ta_wl2, *st_wl1, *st_wl2, *wt_wl1, *wt_wl2;
    if (TIME_SLICE == 1000)
    {
        ta_wl1 = fopen("p3_ta_p1_1ms.txt", "a");
        st_wl1 = fopen("p3_st_p1_1ms.txt", "a");
        wt_wl1 = fopen("p3_wt_p1_1ms.txt", "a");

        ta_wl2 = fopen("p3_ta_p2_1ms.txt", "a");
        st_wl2 = fopen("p3_st_p2_1ms.txt", "a");
        wt_wl2 = fopen("p3_wt_p2_1ms.txt", "a");
    }
    else
    {
        ta_wl1 = fopen("p3_ta_p1_2ms.txt", "a");
        st_wl1 = fopen("p3_st_p1_2ms.txt", "a");
        wt_wl1 = fopen("p3_wt_p1_2ms.txt", "a");

        ta_wl2 = fopen("p3_ta_p2_2ms.txt", "a");
        st_wl2 = fopen("p3_st_p2_2ms.txt", "a");
        wt_wl2 = fopen("p3_wt_p2_2ms.txt", "a");
    }

    createSharedMemory();

    process1 = fork();
    if (process1 > 0)
    {
        process2 = fork();
        if (process2 > 0)
        {
            // Halting child processes initially to time them

            kill(process2, SIGSTOP);
            waitingTimeS[1] = clock();

            kill(process1, SIGSTOP);
            waitingTimeS[0] = clock();

            currentlyScheduledProcess = process1;

            for (;;)
            {

                // check if process1 and process2 finished
                if (shared_proc_data->finished[1] == 1 && shared_proc_data->finished[0] == 1)
                {
                    // PROCESS IS FINISHED
                    printf("Done\n");
                    break;
                }

                // Round robin
                int i = 0;
                while (i < 2)
                {

                    if (!shared_proc_data->finished[i])
                    {
                        continueProcess(i);
                        // Sleeping the parent process so the currentlyScheduledProcess runs for TIME_SLICE time
                        usleep(TIME_SLICE);
                        stopProcess(i);

                        // Changing turns
                        if (currentlyScheduledProcess != process1)
                        {
                            waitingTimeS[1] = clock();
                            currentlyScheduledProcess = process1;
                        }
                        else if (currentlyScheduledProcess != process2)
                        {
                            waitingTimeS[0] = clock();
                            currentlyScheduledProcess = process2;
                        }
                    }
                    i++;
                }
            }

            // Printing results to file
            fprintf(ta_wl1, "%lf, ", timings->ta_time[0]);
            fprintf(ta_wl2, "%lf, ", timings->ta_time[1]);
            fprintf(wt_wl1, "%lf, ", timings->wt_time[0]);
            fprintf(wt_wl2, "%lf, ", timings->wt_time[1]);
            fprintf(st_wl1, "%d.%.9ld, ", (int)timings->switch_time[0].tv_sec, timings->switch_time[0].tv_nsec);
            fprintf(st_wl2, "%d.%.9ld, ", (int)timings->switch_time[1].tv_sec, timings->switch_time[1].tv_nsec);
            // printf("%d.%.9ld, ", (int)timings->switch_time[1].tv_sec, timings->switch_time[1].tv_nsec);

            // Deleting shared memory
            deleteSharedMemory();

            fclose(ta_wl1);
            fclose(wt_wl1);
            fclose(st_wl1);
            fclose(ta_wl2);
            fclose(wt_wl2);
            fclose(st_wl2);
        }
        else
        {
            // running process1
            int t = execlp("./p1_sched.out", "./p1_sched.out", argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], NULL);
            if (t == -1)
            {
                printf("P1: error running process1  \n");
            }
        }
    }
    else
    {
        // running process2
        int t = execlp("./p2_sched.out", "./p2_sched.out", argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], NULL);
        if (t == -1)
        {
            printf("P2: error running process2 \n");
        }
    }
}