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




//Pour les strings
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

    //on ajoute le type s (pour verifier avec les string typé dans la table)
    size_t data_typed_size = data_len+1;
    char *data_typed = malloc(data_typed_size);
    data_typed[0]='s';
    memcpy(data_typed+1,data,data_len);

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
        if (is_same_string(fd, addr, current, data_typed, slowns, data_typed_size)) {
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

static void print_file_content(int fd, struct hache_entree *addr, uint32_t h_file, size_t b, size_t size_content, size_t content_offset, long slowns) {
    uint32_t curr = h_file;
    size_t written = 0;
    // Parcours des blocs de données du fichier
    while (written < size_content) {
        // Lock pour pouvoir lire le bloc courant
        lock_read_block(fd, curr);
        // Vérification si le bloc est libre
        if (addr[curr].longueur == 0xffffffffUL) {
            sleep_nano(slowns);
            unlock_block(fd, curr);
            fprintf(stderr, "Erreur: bloc de continuation libre\n");
            break;
        }
        // Espace disponible dans le bloc courant (1012 octets max pour les continuations, diminué de l'offset de l'en-tête pour le premier bloc)
        size_t chunk_avail = (written == 0) ? (1012 - content_offset) : 1012;
        // Pointeur vers le début des données utiles du bloc courant (après l'en-tête pour le premier bloc, ou au début de obj sinon)
        char *chunk_ptr = (written == 0) ? (addr[curr].obj + content_offset) : addr[curr].obj;
        // Nombre d'octets à écrire (le reste des données s'il est plus petit que l'espace disponible, sinon tout l'espace disponible)
        size_t to_write = (size_content - written < chunk_avail) ? (size_content - written) : chunk_avail;
        // Écriture du fragment sur la sortie standard (STDOUT)
        write(STDOUT_FILENO, chunk_ptr, to_write);
        written += to_write;
        uint32_t next = addr[curr].suivant;
        sleep_nano(slowns);
        unlock_block(fd, curr);
        // bloc suivant si nécessaire
        if (written < size_content) {
            if (next == 0xffffffffUL || next >= b) {
                fprintf(stderr, "Erreur: bloc de continuation invalide ou fin prématurée\n");
                break;
            }
            curr = next;
        }
    }
}

static void save_file_content(int fd, struct hache_entree *addr, uint32_t h_file, size_t b, size_t size_content, size_t content_offset, long slowns, const char *save_dest, mode_t mode, uint32_t version_num) {
    int out_fd = open(save_dest, O_RDWR | O_CREAT | O_TRUNC, mode);
    if (out_fd == -1) {
        perror("open save destination");
        return;
    }
    if (size_content > 0) {
        if (ftruncate(out_fd, size_content) == -1) {
            perror("ftruncate save destination");
        } else {
            char *out_addr = mmap(NULL, size_content, PROT_READ | PROT_WRITE, MAP_SHARED, out_fd, 0);
            if (out_addr == MAP_FAILED) {
                perror("mmap save destination");
            } else {
                uint32_t curr = h_file;
                size_t written = 0;
                // parcours des blocs pour copier
                while (written < size_content) {
                    // Lock du contenu courant
                    lock_read_block(fd, curr);
                    // Vérification si le bloc est libre
                    if (addr[curr].longueur == 0xffffffffUL) {
                        sleep_nano(slowns);
                        unlock_block(fd, curr);
                        fprintf(stderr, "Erreur: bloc de continuation libre\n");
                        break;
                    }
                    // Espace disponible dans le bloc courant (1012 octets max pour les continuations, diminué de l'offset de l'en-tête pour le premier bloc)
                    size_t chunk_avail = (written == 0) ? (1012 - content_offset) : 1012;
                    // Pointeur vers le début des données utiles du bloc courant (après l'en-tête pour le premier bloc, ou au début de obj sinon)
                    char *chunk_ptr = (written == 0) ? (addr[curr].obj + content_offset) : addr[curr].obj;
                    // Nombre d'octets à écrire (le reste des données s'il est plus petit que l'espace disponible, sinon tout l'espace disponible)
                    size_t to_write = (size_content - written < chunk_avail) ? (size_content - written) : chunk_avail;
                    // écriture du bloc dans le fichier de destination
                    memcpy(out_addr + written, chunk_ptr, to_write);
                    written += to_write;
                    uint32_t next = addr[curr].suivant;
                    sleep_nano(slowns);
                    unlock_block(fd, curr);
                    // bloc suivant si nécessaire
                    if (written < size_content) {
                        if (next == 0xffffffffUL || next >= b) {
                            fprintf(stderr, "Erreur: bloc de continuation invalide ou fin prématurée\n");
                            break;
                        }
                        curr = next;
                    }
                }
                munmap(out_addr, size_content);
            }
        }
    }
    close(out_fd);
    printf("Contenu de la version %u sauvegardé avec succès dans '%s'.\n", version_num, save_dest);
}

int lookup_file_by_name(long slowns, const char *filename, int fd, type_hash_t hash, size_t size, lookup_action_t action, const char *save_dest, int version_to_get) {

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

    lock_read_block(fd, filename_hash);
    if (addr[filename_hash].longueur == 0xffffffffUL || addr[filename_hash].premier == 0xffffffffUL) {
        sleep_nano(slowns);
        unlock_block(fd, filename_hash);
        munmap(addr, size);
        return 1;  // Fichier non trouvé
    }

    // Parcourir la chaîne avec un read_lock et chercher entrée 'n'
    uint32_t current = addr[filename_hash].premier;
    if (current != filename_hash) {
        lock_read_block(fd, current);
        sleep_nano(slowns);
        unlock_block(fd, filename_hash);
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

    bool action_print = (action == ACTION_PRINT);
    bool action_save = (action == ACTION_SAVE);

    if (action_print) {
        fprintf(stderr, "Fichier '%s' trouvé avec %u version(s):\n", filename, h_file_count);
    } else {
        printf("Fichier '%s' trouvé avec %u version(s):\n", filename, h_file_count);
    }

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
        if (metadata.name_len > 0 && 1 + sizeof(metadata) + metadata.name_len <= 1012 &&
            metadata.name_len < sizeof(file_name)) {
            memcpy(file_name, addr[h_file].obj + 1 + sizeof(metadata), metadata.name_len);
        } else if (metadata.name_len > 0) {
            snprintf(file_name, sizeof(file_name), "<nom trop long, %u octets>", metadata.name_len);
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
        if (action_print) {
            fprintf(stderr, "  Version %u:\n", i + 1);
            fprintf(stderr, "    Nom stocké: %s\n", file_name);
            fprintf(stderr, "    Taille: %zu octets\n", (size_t)metadata.size);
            fprintf(stderr, "    Modifié: %s\n", time_str);
            fprintf(stderr, "    Mode: 0%o\n", metadata.mode);
            fprintf(stderr, "    Hashcode: %u\n", h_file);
        } else {
            printf("  Version %u:\n", i + 1);
            printf("    Nom stocké: %s\n", file_name);
            printf("    Taille: %zu octets\n", (size_t)metadata.size);
            printf("    Modifié: %s\n", time_str);
            printf("    Mode: 0%o\n", metadata.mode);
            printf("    Hashcode: %u\n", h_file);
        }

        sleep_nano(slowns);
        unlock_block(fd, h_file);

        // Si c'est la version à récupérer, exécuter print/save
        uint32_t target_idx = (version_to_get == -1) ? (h_file_count - 1) : (uint32_t)(version_to_get - 1);
        if (i == target_idx && (action_print || action_save)) {
            size_t content_offset = 1 + sizeof(struct file_metadata) + metadata.name_len;
            if (action_print) {
                print_file_content(fd, addr, h_file, b, metadata.size, content_offset, slowns);
            } else if (action_save) {
                save_file_content(fd, addr, h_file, b, metadata.size, content_offset, slowns, save_dest, metadata.mode, i + 1);
            }
        }
    }

    sleep_nano(slowns);
    unlock_block(fd, name_entry_block);

    munmap(addr, size);
    return 0;
}


int deletehash(long slowns, uint32_t index, int fd, size_t size){
    size_t b = size / 1024;
    if (index >= b) {
        fprintf(stderr, "Erreur deletehash: hashcode hors table\n");
        return -1;
    }

    struct hache_entree *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap dans deletehash");
        return -1;
    }

    int deleted_count = 0;

    lock_write_block(fd, index);

    //si vide
    if (addr[index].longueur == 0xffffffffUL) {
        fprintf(stderr, "Warning : entrée vide.\n");
        sleep_nano(slowns);
        unlock_block(fd, index);
        munmap(addr, size);
        return 0;
    }

    uint32_t head = addr[index].premier;


    //si pas de d'objet a ce hash
    if (head == 0xffffffffUL) {
        fprintf(stderr, "Warning : entrée vide.\n");
        sleep_nano(slowns);
        unlock_block(fd, index);
        munmap(addr, size);
        return 0;
    }

   
    uint32_t current;

    if (head==index) {
        current = index;
    } else {
        lock_write_block(fd, head);
        sleep_nano(slowns);
        unlock_block(fd, index);
        current = head;
    }


    while (current != 0xffffffffUL) {
        uint32_t next = addr[current].suivant;

        //on verrouille tout de suite le next
        if (next != 0xffffffffUL) {
            lock_write_block(fd, next);
        }

        bool moved = false;

        //si un objet commencant par ce hash existe ailleur
        if (addr[current].premier != 0xffffffffUL && addr[current].premier != current) {
            uint32_t target = addr[current].premier;
            lock_write_block(fd, target);

            //si il ne fait que un block de taille 
            if (addr[target].longueur != 0xffffffffUL &&addr[target].longueur <= 1012 && addr[target].suivant == 0xffffffffUL){
                
                
                uint32_t target_len = addr[target].longueur;
                // target -> current
                memcpy(addr[current].obj, addr[target].obj, target_len);
                addr[current].longueur = target_len;
                addr[current].premier  = current;
                addr[current].suivant  = 0xffffffffUL;
                // on vide completement target
                addr[target].longueur = 0xffffffffUL;
                addr[target].premier  = target;
                addr[target].suivant  = 0xffffffffUL;

                sleep_nano(slowns);
                unlock_block(fd, target);

                moved = true;
            } else {
                sleep_nano(slowns);
                unlock_block(fd, target);
            }
        }

        if (!moved) {
            addr[current].longueur = 0xffffffffUL;
            addr[current].premier  = current;
            addr[current].suivant  = 0xffffffffUL;
        }

        sleep_nano(slowns);
        unlock_block(fd, current);

        deleted_count++;
        current = next;
    }

    // Nettoyage final
    if (!(head==index)) {
        lock_write_block(fd, index);
        addr[index].premier = 0xffffffffUL;
        sleep_nano(slowns);
        unlock_block(fd, index);
    }

    munmap(addr, size);
    return deleted_count;
}