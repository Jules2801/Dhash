#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

#include "hash.h"
#include "command.h"
#include "file.h"
#include "type.h"

bool make(char *nom,long slowns, int taille,int fd) {
    printf("make slowns : %ld, taille : %d\n",slowns,taille);    
    if (sleep_nano(slowns)) {
        return true;
    }
    ftruncate(fd, taille*1024);

    struct hache_entree * addr = mmap(NULL, taille*1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (addr == MAP_FAILED) {
        perror("mmap");
        return true;
    }
    for (int i = 0; i < taille; i++) {
        addr[i].longueur = 0xffffffffUL;  
        addr[i].premier = i;
        //apriori pas necessaire mais faisons le par securité
        addr[i].suivant = 0xffffffffUL;
    }
    munmap(addr, taille*1024);
    
    return false;
}

