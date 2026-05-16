#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/stat.h>

#include "../include/hash.h"
#include "../include/command.h"
#include "../include/lock.h"
#include "../include/type.h"
#include "../include/utils.h"



/**
 * @brief Vérifie si l'objet stocké à partir de start_block correspond à data.
 * !!! start_block doit etre lock en ecriture ou lecture !!! 
 * @return 1 si l'objet == data, 0 sinon.
 */
int is_same_string(int fd, struct hache_entree *addr, uint32_t start_block, 
                   const char *data, long slowns) 
{
    uint32_t current = start_block;
    size_t cursor = 0;
    size_t data_len = strlen(data);
    bool first = true;  // pour ne pas unlock pas le premier 

    while (1) {
        size_t chunk_len = (addr[current].longueur > 1012) 
                           ? 1012 
                           : addr[current].longueur;
        
        int match = (cursor + chunk_len <= data_len) &&
                    (memcmp(addr[current].obj, data + cursor, chunk_len) == 0);
        
        if (!match) {
            if (!first) {
                sleep_nano(slowns);
                unlock_block(fd, current);
            }
            return 0;
        }
        
        cursor += chunk_len;

        // on verifie si c'est le dernier block 
        if (addr[current].longueur <= 1012) {
            if (!first) {
                sleep_nano(slowns);
                unlock_block(fd, current);
            }
            return cursor == data_len;  // match seulement si on a tout verif
        }

        // on lock en lecture le suivant et on unlock (sauf si on est first)
        uint32_t next = addr[current].suivant;
        lock_read_block(fd, next);
        sleep_nano(slowns);
        if (!first) {
            unlock_block(fd, current);
        }
        first = false;
        current = next;
    }
}


int make(long slowns, uint32_t taille,int fd) {
    if (sleep_nano(slowns)) {
        return 1;
    }
    ftruncate(fd, taille*1024);

    struct hache_entree * addr = mmap(NULL, taille*1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (addr == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    for (uint32_t i = 0; i < taille; i++) {
        addr[i].longueur = 0xffffffffUL;  
        addr[i].premier = i;
        //apriori pas necessaire mais faisons le par securité
        addr[i].suivant = 0xffffffffUL;
    }
    munmap(addr, taille*1024);
    
    return 0;
}


int free_blocks(long slowns, int fd, size_t size) {

    if (sleep_nano(slowns)) {
        return -1; 
    }

    size_t b = size / 1024;
    
    struct hache_entree *addr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap dans free");
        return -1;
    }
    int free_count = 0;
    for (size_t i = 0; i < b; i++) {
        if (addr[i].longueur == 0xffffffffUL) {
            free_count++;
        }
    }

    munmap(addr, size);
    return free_count; 
}





int lookup(long slowns, char *data, int fd, type_hash_t hash, size_t size) {
    if (sleep_nano(slowns)) {
        return -1;
    }

    if (size < 1024) {
        fprintf(stderr, "Erreur lookup: taille de table invalide\n");
        return -1;
    }

    size_t b = size / 1024;

    struct hache_entree *addr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    uint32_t index = apply_hash((unsigned char *)data, hash, b);
    if (index == UINT32_MAX) {
        fprintf(stderr, "Erreur lookup : fonction de hachage non trouvé\n");
        munmap(addr, size);
        return -1;
    }

    int found = 0;
    for (size_t i = 0; i < b; i++) {
        uint32_t test_block = (index + i) % b;

        if (addr[test_block].longueur == 0xffffffffUL) {
            break;
        }
        if (addr[test_block].premier != index) {
            continue;
        }
        if (is_same_string(fd,addr, test_block, data,slowns)) {
            found = 1;
            break;
        }
    }

    munmap(addr, size);
    return found ? 0 : 1;
}



