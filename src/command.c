#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/fcntl.h>
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
  
    if (ftruncate(fd, taille * 1024) == -1) {
    perror("ftruncate");
    return 1;
    }

    struct flock fl = {0};
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    if (fcntl(fd, F_SETLKW, &fl) == -1) {
        perror("fcntl lock dans make");
        return 1;
    }

    struct hache_entree * addr = mmap(NULL, taille*1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (addr == MAP_FAILED) {
        perror("mmap");
        fl.l_type = F_UNLCK;
        fcntl(fd, F_SETLKW, &fl);
        return 1;
    }
    for (uint32_t i = 0; i < taille; i++) {
        addr[i].longueur = 0xffffffffUL;  
        addr[i].premier = i;
        //apriori pas necessaire mais faisons le par securité
        addr[i].suivant = 0xffffffffUL;
    }
    sleep_nano(slowns);
    munmap(addr, taille*1024);

    fl.l_type = F_UNLCK;
    fcntl(fd, F_SETLKW, &fl);

    
    
    return 0;
}

int free_blocks(long slowns, int fd, size_t size,size_t *free_count, size_t *object_count) {
    size_t b = size / 1024;

    struct hache_entree *addr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap dans free_blocks");
        return -1;
    }

    bool *is_continuation = calloc(b, sizeof(bool));
    if (is_continuation == NULL) {
        perror("calloc dans free_blocks");
        munmap(addr, size);
        return -1;
    }

    size_t free_c = 0;

    // compte les libres + marque les continuations
    for (size_t i = 0; i < b; i++) {
        lock_read_block(fd, i);

        if (addr[i].longueur == 0xffffffffUL) {
            free_c++;
        } else if (addr[i].longueur > 1012 && addr[i].suivant != 0xffffffffUL) {
            is_continuation[addr[i].suivant] = true;
        }
        sleep_nano(slowns);
        unlock_block(fd, i);
    }
    // compte les objets
    size_t object_c = 0;
    for (size_t i = 0; i < b; i++) {
        lock_read_block(fd, i);

        if (addr[i].longueur != 0xffffffffUL && !is_continuation[i]) {
            object_c++;
        }

        sleep_nano(slowns);
        unlock_block(fd, i);
    }

    free(is_continuation);
    munmap(addr, size);

    *free_count = free_c;
    *object_count = object_c;
    return 0;
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



