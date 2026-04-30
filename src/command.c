#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

#include "command.h"
#include "file.h"

bool make(char *nom,long slowns, int taille,int fd) {
    printf("make slowns : %ld, taille : %d\n",slowns,taille);    
    if (sleep_nano(slowns)) {
        return true;
    }
    ftruncate(fd, taille*1024);
    char *addr = mmap(NULL, taille*1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (addr == MAP_FAILED) {
        perror("mmap");
        return true;
    }
    for (int i = 0; i < taille*1024; i++) {
        addr[i] = 0;
    }
    munmap(addr, taille*1024);
    
    return false;
}

