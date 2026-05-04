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
#include "../include/file.h"
#include "../include/type.h"
#include "../include/utils.h"



/**
 * @brief Vérifie si la chaîne stockée à partir de 'start_block' correspond à 'data'
 * @return 1 si c'est la même chaîne 0 sinon
 */
int is_same_string(struct hache_entree *addr, uint32_t start_block, const char *data) {
    uint32_t current = start_block;
    size_t cursor = 0;
    size_t data_len = strlen(data);

    while (current != 0xffffffffUL) {
        size_t chunk_len = addr[current].longueur;
        
        if (cursor + chunk_len > data_len) {
            return 0;
        }
        if (strncmp((char *)addr[current].obj, data + cursor, chunk_len) != 0) {
            return 0;
        }
        cursor += chunk_len;
        current = addr[current].suivant; 
    }
    return cursor == data_len;
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
    for (int i = 0; i < taille; i++) {
        addr[i].longueur = 0xffffffffUL;  
        addr[i].premier = i;
        //apriori pas necessaire mais faisons le par securité
        addr[i].suivant = 0xffffffffUL;
    }
    munmap(addr, taille*1024);
    
    return 0;
}


int insertstring(long slowns, char *data, int fd, type_hash_t hash, size_t size) {
    if (sleep_nano(slowns)) {
        return 1;
    }

    size_t b = size / 1024;
    size_t data_size = strlen(data); 
    
    struct hache_entree *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    //Calcul du hash
    uint32_t index;
    if((index = apply_hash((unsigned char*)data, hash, b))==-1){
        fprintf(stderr, "Erreur : La fonction de hachage n'existe pas.\n");
        munmap(addr,size);
        return 1;
    }

    // On verifie les doublons (si la data est deja dans la table)
    for (size_t i = 0; i < b; i++) {
        uint32_t test_block = (index + i) % b;
        //Si vide pas la 
        if (addr[test_block].longueur == 0xffffffffUL) break; 
        //Si meme index
        if (addr[test_block].premier == index) {
            if (is_same_string(addr, test_block, data)) { 
                fprintf(stderr, "Warning : chaîne déjà présente.\n");
                munmap(addr, size);
                return 0; 
            }
        }
    }

    uint32_t start_block;
    
    if (addr[index].longueur == 0xffffffffUL) {
        start_block = index; 
    } else {
        int free_block = find_free_block(addr, b, index);
        if (free_block == -1) {
            fprintf(stderr, "Erreur : Table de hachage pleine.\n");
            munmap(addr, size);
            return 1;
        }
        start_block = (uint32_t)free_block;
    }

    
    size_t remaining_size = data_size;
    size_t data_cursor = 0;                       
    uint32_t current_block = start_block;

    while (remaining_size > 0) {
        size_t chunk_size = (remaining_size < 1012) ? remaining_size : 1012;
        

        memcpy(addr[current_block].obj, data + data_cursor, chunk_size);
        addr[current_block].longueur = (uint32_t)chunk_size; 
        
        addr[current_block].premier = index;
        
        remaining_size -= chunk_size;
        data_cursor += chunk_size;

        if (remaining_size > 0) {
            int next_block = find_free_block(addr, b, current_block);
            if (next_block == -1) {
                fprintf(stderr, "Erreur : Plus de place pour finir la chaîne.\n");
                munmap(addr, size * 1024);
                return 1;
            }
            addr[current_block].suivant = (uint32_t)next_block;
            current_block = (uint32_t)next_block;
        } else {
            addr[current_block].suivant = 0xffffffffUL;
        }
    }

    munmap(addr, size);
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



