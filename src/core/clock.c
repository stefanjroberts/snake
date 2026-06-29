#include "defines.h"
#include <time.h>
#include <unistd.h>

static f64 target_frame_time = 1000000.0 / 60.0;
static f64 time_elapsed;
static f64 frame_start;

void clock_start_timer()
{
    struct timespec ts_current_time;
    clock_gettime(CLOCK_MONOTONIC, &ts_current_time);
    frame_start = ts_current_time.tv_sec * 1000000 + ts_current_time.tv_nsec / 1000.0;
    return;
}

// NOTE:  returns the time by which a frame is missed in microseconds.
f64 clock_frame_sync()
{
    struct timespec ts_current_time;
    clock_gettime(CLOCK_MONOTONIC, &ts_current_time);
    time_elapsed = ts_current_time.tv_sec * 1000000 + ts_current_time.tv_nsec / 1000.0 - frame_start;
    
    usleep(target_frame_time - time_elapsed);
    return time_elapsed - target_frame_time;
}