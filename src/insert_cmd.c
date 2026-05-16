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
#include "../include/lock.h"
#include "../include/type.h"
#include "../include/utils.h"



/**
 * @brief Trouve un bloc libre, le verrouille en écriture, revérifie sous lock 
 *        qu'il est toujours libre, et le marque occupé.
 *        À la sortie, le bloc est verrouillé ET marqué, donc personne d'autre 
 *        ne peut le prendre.
 * @return numéro du bloc réservé, ou -1 si table pleine.
 */

int reserve_free_block(int fd, struct hache_entree *addr, size_t b, uint32_t hint) {
    uint32_t current_hint = hint;

    //si plusieurs processus tente en meme temps de trouver un block libre
    for (size_t attempts = 0; attempts < b; attempts++) {
        int candidate = find_free_block(addr, b, current_hint);
        if (candidate == -1) return -1;

        lock_write_block(fd, (uint32_t)candidate);
        
        // un autre processus a peut etre pris ce block entre le find_free_block et le lock_write_block.
        if (addr[candidate].longueur == 0xffffffffUL) {
            // On marque le bloc comme occupé immédiatement.
            addr[candidate].longueur = 0;
            addr[candidate].premier  = 0xffffffffUL;  
            addr[candidate].suivant  = 0xffffffffUL;
            return candidate;
        }

        //on realache et on retente de trouver un block 
        unlock_block(fd, (uint32_t)candidate);
        current_hint = (uint32_t)candidate;// on commence a partir de celui qui vient d'être prix
    }
    return -1;
}



int fill_data
(int fd, struct hache_entree *addr, size_t data_size, char *data, uint32_t start_block, long slowns, size_t b) {
    size_t remaining = data_size;
    size_t cursor    = 0;
    uint32_t current = start_block;

    // Cas particulier string = ""
    // pour eviter un possible deadlock
    if (data_size == 0) {
        sleep_nano(slowns);
        unlock_block(fd, current);
        return 0;
    }

    while (remaining > 0) {
        size_t chunk = (remaining < 1012) ? remaining : 1012;

        memcpy(addr[current].obj, data + cursor, chunk);
        addr[current].longueur = (uint32_t)remaining;

        remaining -= chunk;
        cursor    += chunk;

        if (remaining > 0) {
            // Il reste des données : on réserve le bloc suivant AVANT 
            int next = reserve_free_block(fd, addr, b, current);
            if (next == -1) {
                fprintf(stderr, "Erreur critique : table pleine pendant l'écriture.\n");
                sleep_nano(slowns);
                unlock_block(fd, current);
                return 1;
            }
            
            addr[current].suivant = (uint32_t)next;

            sleep_nano(slowns);
            unlock_block(fd, current);
            current = (uint32_t)next;
        } else {
            // Dernier morceau de la data : fin de chaîne.
            addr[current].suivant = 0xffffffffUL;
            sleep_nano(slowns);
            unlock_block(fd, current);
        }
    }
    return 0;
}


int insertstring(long slowns, char *data, int fd, type_hash_t hash, size_t size) {
    size_t b = size / 1024;
    size_t data_size = strlen(data);

    struct hache_entree *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    uint32_t index = apply_hash((unsigned char *)data, hash, b);
    if (index == UINT32_MAX) {
        fprintf(stderr, "Erreur : fonction de hachage inconnue.\n");
        munmap(addr, size);
        return 1;
    }

    //On determine le block de départ 

    lock_write_block(fd, index);
    uint32_t start_block;

    if (addr[index].longueur == 0xffffffffUL) {
        //CAS 1 : index libre, pas de collision 
        addr[index].longueur = 0;
        addr[index].premier  = index;          // Normalement make le fait deja 
        addr[index].suivant  = 0xffffffffUL;
        start_block = index;
    }
    else if (addr[index].premier == 0xffffffffUL) {
        // CAS 2 : index occupé par une data d'un autre hash
        // On réserve un bloc qui devient la tête de notre liste
        int free_block = reserve_free_block(fd, addr, b, index);
        if (free_block == -1) {
            fprintf(stderr, "Erreur : Table pleine.\n");
            sleep_nano(slowns);
            unlock_block(fd, index);
            munmap(addr, size);
            return 1;
        }
        // free_block est locké + len = 0 "occupé"  
        // Le hash original aura pour premier block free block
        addr[index].premier = (uint32_t)free_block;

        // On libère index car on ecrira dans free block
        sleep_nano(slowns); 
        unlock_block(fd, index);
        start_block = (uint32_t)free_block;
    }
    else {
        //CAS 3 : une liste existe déjà pour notre hash 
        // elle peut etre dans un autre block par contre donc on prend pas l'index 
        uint32_t current = addr[index].premier;

        //on passe au block suivant et on unlock l'actuelle sauf si current == index pas besoin
        if (current != index) {
            lock_write_block(fd, current);
            sleep_nano(slowns);
            unlock_block(fd, index);
        } 

        while (1) {
            
            //on cherche dans toute les chaine existante avec le meme hash si on a un string egal
            if (is_same_string(fd,addr, current, data,slowns)) {
                fprintf(stderr, "Warning : chaîne déjà présente.\n");
                sleep_nano(slowns);
                unlock_block(fd, current);
                munmap(addr, size);
                return 0;
            }

            //on parcour par chaine et pas par block donc si la longeur est superieur on va au block suivant
            while (addr[current].longueur > 1012) {
                uint32_t cont = addr[current].suivant;
                lock_write_block(fd, cont);
                sleep_nano(slowns);
                unlock_block(fd, current);
                current = cont;
            }


            if (addr[current].suivant == 0xffffffffUL) {
                // on réserve un bloc et on l'accroche.
                int free_block = reserve_free_block(fd, addr, b, current);
                if (free_block == -1) {
                    fprintf(stderr, "Erreur : Table pleine.\n");
                    sleep_nano(slowns);
                    unlock_block(fd, current);
                    munmap(addr, size);
                    return 1;
                }
                addr[current].suivant = (uint32_t)free_block;

                sleep_nano(slowns);
                unlock_block(fd, current);
                start_block = (uint32_t)free_block;
                break;
            }

            //on lock le suivant AVANT de lâcher current.
            uint32_t next = addr[current].suivant;
            lock_write_block(fd, next);
            sleep_nano(slowns);
            unlock_block(fd, current);
            current = next;
        }
    }

    //On ecrit la data 
    int rc = fill_data(fd, addr, data_size, data, start_block, slowns, b);

    munmap(addr, size);
    return rc;
}