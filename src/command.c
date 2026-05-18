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
#include <time.h>

#include "../include/hash.h"
#include "../include/command.h"
#include "../include/lock.h"
#include "../include/type.h"
#include "../include/utils.h"
#include "../include/insert_cmd.h"



/**
 * @brief Vérifie si l'objet stocké à partir de start_block correspond à data.
 * !!! start_block doit etre lock en ecriture ou lecture !!! 
 * @return 1 si l'objet == data, 0 sinon.
 */
int is_same_string(int fd, struct hache_entree *addr, uint32_t start_block, 
                   const char *data, long slowns,size_t data_len) 
{
    uint32_t current = start_block;
    size_t cursor = 0;
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
    if (size < 1024) {
        fprintf(stderr, "Erreur lookup: taille de table invalide\n");
        return -1;
    }

    size_t b = size / 1024;
    size_t data_len = strlen(data);

    struct hache_entree *addr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    uint32_t index = apply_hash((unsigned char *)data, hash, b, data_len);
    if (index == UINT32_MAX) {
        fprintf(stderr, "Erreur lookup : fonction de hachage non trouvé\n");
        munmap(addr, size);
        return -1;
    }

    // Lock pour pouvoir lire les champs
    lock_read_block(fd, index);

    // Case libre ou pas de chaîne pour ce hash -> pas trouvé
    if (addr[index].longueur == 0xffffffffUL || 
        addr[index].premier == 0xffffffffUL) {
        sleep_nano(slowns);
        unlock_block(fd, index);
        munmap(addr, size);
        return 1;
    }

    // Aller a la tête de la chaîne (peut être ailleurs en CAS 2)
    uint32_t current = addr[index].premier;
    if (current != index) {
        lock_read_block(fd, current);
        sleep_nano(slowns);
        unlock_block(fd, index);
    }

    // parcours de la chaine
    int found = 0;
    while (1) {
        if (is_same_string(fd, addr, current, data, slowns, data_len)) {
            found = 1;
            sleep_nano(slowns);
            unlock_block(fd, current);
            break;
        }

        // Aller au dernier bloc de l'objet courant si il est > 1012
        while (addr[current].longueur > 1012) {
            uint32_t cont = addr[current].suivant;
            lock_read_block(fd, cont);
            sleep_nano(slowns);
            unlock_block(fd, current);
            current = cont;
        }

        // pas d'objet suivant dans la chaîne ?
        if (addr[current].suivant == 0xffffffffUL) {
            sleep_nano(slowns);
            unlock_block(fd, current);
            break;
        }
        uint32_t next = addr[current].suivant;
        lock_read_block(fd, next);
        sleep_nano(slowns);
        unlock_block(fd, current);
        current = next;
    }

    munmap(addr, size);
    return found ? 0 : 1;
}

int lookup_file_by_name(long slowns, const char *filename, int fd, type_hash_t hash, size_t size) {
    if (sleep_nano(slowns)) {
        return -1;
    }

    if (size < 1024) {
        fprintf(stderr, "Erreur lookup_file: taille de table invalide\n");
        return -1;
    }

    size_t b = size / 1024;
    size_t name_len = strlen(filename);

    struct hache_entree *addr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    // 1. Calculer le hash
    uint32_t filename_hash = apply_hash((unsigned char *)filename, hash, b, name_len);
    if (filename_hash == UINT32_MAX) {
        fprintf(stderr, "Erreur lookup_file: fonction de hachage non trouvée\n");
        munmap(addr, size);
        return -1;
    }

    // 2. Vérifier si le slot est libre
    if (addr[filename_hash].longueur == 0xffffffffUL || addr[filename_hash].premier == 0xffffffffUL) {
        munmap(addr, size);
        return 1;  // Fichier non trouvé
    }

    // Parcourir la chaîne avec un read_lock et chercher entrée 'n'
    uint32_t current = addr[filename_hash].premier;
    if (current != filename_hash) {
        lock_read_block(fd, current);
        sleep_nano(slowns);
        unlock_block(fd, filename_hash);
    } else { 
        // On commence directement à filename_hash
        lock_read_block(fd, filename_hash);
    }

    uint32_t name_entry_block = 0xffffffffUL;

    while (1) {
        // Chercher le 'n' 
        if (addr[current].obj[0] == 'n' && 1 + sizeof(uint32_t) + name_len <= 1012) {
            uint32_t stored_name_len;
            memcpy(&stored_name_len, addr[current].obj + 1, sizeof(uint32_t));
            if (stored_name_len == name_len &&
                memcmp(addr[current].obj + 1 + sizeof(uint32_t), filename, name_len) == 0) {
                // on a trouvé
                name_entry_block = current;
                break;
            }
        }

        // Aller au dernier bloc si longueur > 1012
        while (addr[current].longueur > 1012) {
            uint32_t cont = addr[current].suivant;
            lock_read_block(fd, cont);
            sleep_nano(slowns);
            unlock_block(fd, current);
            current = cont;
        }

        // pas d'objet suivant dans la chaîne ?
        if (addr[current].suivant == 0xffffffffUL) {
            sleep_nano(slowns);
            unlock_block(fd, current);
            munmap(addr, size);
            return 1;  // Non trouvé
        }

        uint32_t next = addr[current].suivant;
        lock_read_block(fd, next);
        sleep_nano(slowns);
        unlock_block(fd, current);
        current = next;
    }

    // 3. Lire la liste des hashes h_file
    size_t h_file_offset = 1 + sizeof(uint32_t) + name_len;
    uint32_t h_file_count = (addr[name_entry_block].longueur - h_file_offset) / sizeof(uint32_t);

    printf("Fichier '%s' trouvé avec %d version(s):\n", filename, h_file_count);

    // 4. Pour chaque h_file, afficher métadonnées
    for (uint32_t i = 0; i < h_file_count; i++) {
        uint32_t h_file;
        memcpy(&h_file, addr[name_entry_block].obj + h_file_offset + i * sizeof(uint32_t), sizeof(uint32_t));

        if (h_file >= b) {
            fprintf(stderr, "Erreur: hashcode %d hors limites\n", h_file);
            continue;
        }

        // Lire l'entrée 'f' à addr[h_file]
        lock_read_block(fd, h_file);

        if (addr[h_file].longueur == 0xffffffffUL || addr[h_file].obj[0] != 'f') {
            fprintf(stderr, "Erreur: bloc %d n'est pas un fichier\n", h_file);
            sleep_nano(slowns);
            unlock_block(fd, h_file);
            continue;
        }

        struct file_metadata metadata;
        memcpy(&metadata, addr[h_file].obj + 1, sizeof(metadata));

        // Extraire le nom du fichier stocké
        char file_name[1024] = {0};
        if (metadata.name_len > 0 && metadata.name_len <= 1023) {
            memcpy(file_name, addr[h_file].obj + 1 + sizeof(metadata), metadata.name_len);
        }

        // Formater la date
        time_t mtime = (time_t)metadata.mtime;
        char time_str[32] = {0};
        ctime_r(&mtime, time_str);
        // ajout d'une \0 à la place du \n 
        size_t len = strlen(time_str);
        if (len > 0 && time_str[len - 1] == '\n') {
            time_str[len - 1] = '\0';
        }

        // Afficher les métadonnées
        printf("  Version %u:\n", i + 1);
        printf("    Nom stocké: %s\n", file_name);
        printf("    Taille: %zu octets\n", metadata.size);
        printf("    Modifié: %s\n", time_str);
        printf("    Mode: 0%o\n", metadata.mode);
        printf("    Hashcode: %u\n", h_file);

        sleep_nano(slowns);
        unlock_block(fd, h_file);
    }

    sleep_nano(slowns);
    unlock_block(fd, name_entry_block);

    munmap(addr, size);
    return 0;
}

