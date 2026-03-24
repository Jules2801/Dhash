#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "file.h"

bool make(char *nom,long slowns, int taille){
    printf("make slowns : %ld, taille : %d\n",slowns,taille);

    //sleep 
    struct timespec time;
    time.tv_sec  = slowns / 1000000000L;
    time.tv_nsec = slowns % 1000000000L;
    if (nanosleep(&time, NULL) != 0) {
        perror("nanosleep");
        return true; 
    }

    
    return false;
}

int has_extension(const char *filename, const char *ext) {
    size_t nl = strlen(filename), el = strlen(ext);
    return nl >= el && strcmp(filename + nl - el, ext) == 0;
}