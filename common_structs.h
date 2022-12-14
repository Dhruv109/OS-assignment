#ifndef COMMON_STRUCTS
#define COMMON_STRUCTS

#include <time.h>

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

} proc_data;

typedef struct
{
    double ta_time[2];
    double wt_time[2];
    double run_time[2];
    struct timespec switch_time[2];
} time_logging;

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

#endif