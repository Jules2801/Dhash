#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "../include/file.h"

bool has_extension(const char *filename, const char *ext) {
    size_t filenameLength = strlen(filename);
    size_t extLength = strlen(ext);
    return filenameLength >= extLength &&
    strcmp(filename + filenameLength - extLength, ext) == 0;
}


bool sleep_nano(long slowns){
    // pas de sleep si slowns = 0
    if (slowns == 0) {
        return false;
    }
    struct timespec time;
    time.tv_sec  = slowns / 1000000000L;
    time.tv_nsec = slowns % 1000000000L;
    if (nanosleep(&time, NULL) != 0) {
        perror("nanosleep");
        return true; 
    }
    return false;
}