#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
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
#include "../include/insert_cmd.h"

int insertbuff(long slowns, char *data, int fd, type_hash_t hash, size_t size, size_t data_size, uint32_t *out_hash,bool* w);

int insertbuff_at(long slowns, char *data, int fd, size_t size, size_t data_size, uint32_t index,bool* w);


/**
 * @brief Cherche une entrée nom existante pour 'filename' dans la chaîne à h_name.
 *
 * @return 1 si trouvée (*last_block = dernier bloc, locké en write),
 *         0 si pas trouvée (rien locké),
 *        -1 si erreur (rien locké).
 */
int find_name_entry(int fd, struct hache_entree *addr, uint32_t h_name, const char *filename, size_t name_len,long slowns, uint32_t *last_block){
    lock_write_block(fd, h_name);

    // case libre : pas d'entrée nom ici
    if (addr[h_name].longueur == 0xffffffffUL) {
        sleep_nano(slowns);
        unlock_block(fd, h_name);
        return 0;
    }

    // pas de chaîne pour ce hash 
    if (addr[h_name].premier == 0xffffffffUL) {
        sleep_nano(slowns);
        unlock_block(fd, h_name);
        return 0;
    }

    // le fameux cas ou une autre chaine de hash different a pris la place 
    uint32_t current = addr[h_name].premier;
    if (current != h_name) {
        lock_write_block(fd, current);
        sleep_nano(slowns);
        unlock_block(fd, h_name);
    }

    // on cherche objet par objet 
    
    while (1) {
        bool is_match = false;
        // si le nom tien pas dans un block on prend pas en compte 
        if (addr[current].obj[0] == 'n' && 1 + sizeof(uint32_t) + name_len <= 1012) {
            uint32_t stored_name_len;
            memcpy(&stored_name_len, addr[current].obj + 1, sizeof(uint32_t));
            if (stored_name_len == name_len) {
                if (memcmp(addr[current].obj + 1 + sizeof(uint32_t),
                           filename, name_len) == 0) {
                    is_match = true;
                }
            }
        }

        if (is_match) {
            // trouvé : aller au dernier bloc de cet objet
            while (addr[current].longueur > 1012) {
                uint32_t cont = addr[current].suivant;
                lock_write_block(fd, cont);
                sleep_nano(slowns);
                unlock_block(fd, current);
                current = cont;
            }
            *last_block = current;
            return 1;
        }

        // pas la bonne entrée : aller à la fin de cet objet
        while (addr[current].longueur > 1012) {
            uint32_t cont = addr[current].suivant;
            lock_write_block(fd, cont);
            sleep_nano(slowns);
            unlock_block(fd, current);
            current = cont;
        }

        if (addr[current].suivant == 0xffffffffUL) {
            // fin de chaîne, pas trouvé
            sleep_nano(slowns);
            unlock_block(fd, current);
            return 0;
        }

        // objet suivant
        uint32_t next = addr[current].suivant;
        lock_write_block(fd, next);
        sleep_nano(slowns);
        unlock_block(fd, current);
        current = next;
    }
}

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

int insertfile(long slowns, int fd, int file_fd, char *filename,type_hash_t hash, size_t size, struct stat *sb_file){
    size_t file_size = sb_file->st_size;
    size_t name_len = strlen(filename);

    void *file_addr = NULL;
    if (file_size > 0) {
        file_addr = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, file_fd, 0);
        if (file_addr == MAP_FAILED) {
            perror("mmap fichier à insérer");
            return 1;
        }
    }
   // construire le buff fichier
    struct file_metadata metadata = {
        .size     = (uint64_t)file_size,
        .mtime    = (uint64_t)sb_file->st_mtime,
        .mode     = (uint32_t)sb_file->st_mode,
        .name_len = (uint32_t)name_len
    };

    size_t buff_size = 1 + sizeof(metadata) + name_len + file_size;
    char *buff = malloc(buff_size);
    if (buff == NULL) {
        perror("malloc blob");
        if (file_addr) munmap(file_addr, file_size);
        return 1;
    }

    size_t offset = 0;
    buff[offset++] = 'f';
    memcpy(buff + offset, &metadata, sizeof(metadata));
    offset += sizeof(metadata);
    memcpy(buff + offset, filename, name_len);
    offset += name_len;
    if (file_size > 0) {
        memcpy(buff + offset, file_addr, file_size);
    }

    if (file_addr) munmap(file_addr, file_size);
    // On insert le fichier 
    uint32_t h_file;
    bool w = false;
    int rc = insertbuff(slowns, buff, fd, hash, size, buff_size, &h_file,&w);
    free(buff);
    if (rc != 0) return rc;

    if(w){
        return 0;
    }

    // Hasher juste le nom du fichier
    size_t b = size / 1024;
    uint32_t h_name = apply_hash((unsigned char*)filename, hash, b, name_len);
    if (h_name == UINT32_MAX) {
        fprintf(stderr, "Erreur : fonction de hachage inconnue.\n");
        return 1;
    }

    //On cherche l'entre nom
    struct hache_entree *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    uint32_t last_block;
    int found = find_name_entry(fd, addr, h_name, filename, name_len, slowns, &last_block);

    if (found == -1) {
        munmap(addr, size);
        return 1;
    }

    if (found == 1) {
        // Entrée nom existante : on l'étend avec h_file
        uint32_t current_len = addr[last_block].longueur;
        if (current_len + 4 > 1012) {
            fprintf(stderr, "Entrée nom trop pleine \n");
            sleep_nano(slowns);
            unlock_block(fd, last_block);
            munmap(addr, size);
            return 1;
        }
        memcpy(addr[last_block].obj + current_len, &h_file, sizeof(uint32_t));
        addr[last_block].longueur = current_len + 4;
        sleep_nano(slowns);
        unlock_block(fd, last_block);
        munmap(addr, size);
        return 0;
    }

    // Pas d'entrée nom existante : on en crée une nouvelle
    munmap(addr, size);

    size_t name_blob_size = 1 + sizeof(uint32_t) + name_len + sizeof(uint32_t);
    char *name_blob = malloc(name_blob_size);
    if (name_blob == NULL) {
        perror("malloc name_blob");
        return 1;
    }

    uint32_t name_len_u32 = (uint32_t)name_len;
    name_blob[0] = 'n';
    memcpy(name_blob + 1, &name_len_u32, sizeof(uint32_t));
    memcpy(name_blob + 1 + sizeof(uint32_t), filename, name_len);
    memcpy(name_blob + 1 + sizeof(uint32_t) + name_len, &h_file, sizeof(uint32_t));

    
    int rc_name = insertbuff_at(slowns, name_blob, fd, size, name_blob_size, h_name,NULL);
    free(name_blob);
    return rc_name;
}
  

int insertbuff(long slowns, char *data, int fd, type_hash_t hash, size_t size, size_t data_size, uint32_t *out_hash, bool* w) {
    size_t b = size / 1024;
    uint32_t index = apply_hash((unsigned char *)data, hash, b,data_size);
    if (index == UINT32_MAX) {
        fprintf(stderr, "Erreur : fonction de hachage inconnue.\n");
        return 1;
    }

    if (out_hash != NULL) {
        *out_hash = index;
    }
    return insertbuff_at(slowns, data, fd, size, data_size, index,w);
}

int insertbuff_at(long slowns, char *data, int fd,size_t size, size_t data_size,uint32_t index,bool* w) {
    size_t b = size / 1024;

    if (w != NULL) *w = false;// par secu 

    struct hache_entree *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
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
            if (is_same_string(fd,addr, current, data,slowns,data_size)) {
                fprintf(stderr, "Warning : chaîne déjà présente.\n");
                if(w!=NULL){
                    *w = true;
                }
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

int insertstring(long slowns, char *data, int fd, type_hash_t hash, size_t size){
    size_t b = size / 1024;
    size_t data_size = strlen(data);
    uint32_t index = apply_hash((unsigned char *)data, hash, b,data_size);
    if (index == UINT32_MAX) {
        fprintf(stderr, "Erreur : fonction de hachage inconnue.\n");
        return 1;
    }
    size_t data_typed_size = data_size+1;
    char *data_typed = malloc(data_typed_size);
    data_typed[0]='s';
    memcpy(data_typed+1,data,data_size);
    return insertbuff_at(slowns, data_typed, fd, size, data_typed_size, index,NULL);
}