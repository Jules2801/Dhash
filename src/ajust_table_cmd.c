#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/stat.h>
#include "../include/hash.h"
#include "../include/command.h"
#include "../include/lock.h"
#include "../include/type.h"
#include "../include/insert_cmd.h"
#include "../include/ajust_table_cmd.h"


//lit sans verrou car on cherche a extend ou shrink la table 
// on donne le suivant et la longueur lu dans deux pointeurs out 
int read_object_nolock(struct hache_entree *addr, uint32_t start, size_t b,char *buffer, size_t buf_cap,uint32_t *out_total, uint32_t *out_next) {
    if (start >= b) return -1;
    //total = la longueur de l'objet
    uint32_t total = addr[start].longueur;
    if (total == 0xffffffffUL) return -1;
    if ((size_t)total > b * 1012) return -1;

    if (total == 0) {
        *out_next  = addr[start].suivant;
        *out_total = 0;
        return 0;
    }

    size_t remaining = total;
    size_t copied = 0;
    uint32_t current = start;
    size_t steps = 0;
    uint32_t last_suivant = 0xffffffffUL;

    while (remaining > 0) {
        //par securité 
        if (current >= b) return -1;
        if (steps > b) return -1;// on evite une boucle infini si on a un cycle (table corrompue )

        uint32_t suiv = addr[current].suivant;
        size_t chunk = (remaining < 1012) ? remaining : 1012;
        size_t to_copy = (copied + chunk <= buf_cap) ? chunk : (buf_cap - copied);
        if (to_copy > 0) memcpy(buffer + copied, addr[current].obj, to_copy);

        copied += to_copy;
        remaining -= chunk;
        last_suivant = suiv;

        //secu si il reste encore de quoi parcourir mais que suivant vide
        if (remaining > 0) {
            if (suiv == 0xffffffffUL) return -1;
            current = suiv;
        }
        steps++;
    }

    *out_total = total;
    *out_next  = last_suivant;
    return 0;
}

//met a jours tous les hash dans les objet de type nom ('n')
void update_name_entry_hfiles(char *buf, uint32_t total,struct hache_entree *old_addr,size_t b_old, size_t b_new,type_hash_t hash){
    
    uint32_t name_len;
    memcpy(&name_len, buf + 1, sizeof(uint32_t));

    
    size_t header = 1 + sizeof(uint32_t);// 'n' + namelen de 4 octet 
    size_t h_file_offset = header + name_len;

    //par secu on verifie juste si c'est coeherent 
    if (h_file_offset > total) return;
    size_t hfile_zone = total - h_file_offset;
    if (hfile_zone % sizeof(uint32_t) != 0) return;
    size_t nb_hfile = hfile_zone / sizeof(uint32_t);

    for (size_t k = 0; k < nb_hfile; k++) {
        uint32_t old_hfile;
        memcpy(&old_hfile, buf + h_file_offset + k * sizeof(uint32_t),sizeof(uint32_t));
        if (old_hfile >= b_old) continue;                  //pas dans la  table
        uint32_t file_len = old_addr[old_hfile].longueur;
        if (file_len == 0xffffffffUL) continue;            //bloc libre

        char *file_buf = malloc(file_len > 0 ? file_len : 1);
        if (file_buf == NULL) continue;

        uint32_t ftotal, fnext;
        if (read_object_nolock(old_addr, old_hfile, b_old, file_buf, file_len,&ftotal, &fnext) == 0){
            uint32_t new_hfile = recompute_hash(file_buf, ftotal, file_len,hash, b_new);
            if (new_hfile != UINT32_MAX) {
                memcpy(buf + h_file_offset + k * sizeof(uint32_t),&new_hfile, sizeof(uint32_t));
            }
        }
        free(file_buf);
    }
}

static void *resize_worker(void *args) {
    struct extend_args *ea = (struct extend_args *)args;

    size_t b_old = ea->b_old;
    struct hache_entree *old_addr = ea->old_addr;

    for (size_t i = ea->start; i < ea->end; i++) {
        uint32_t longueur = old_addr[i].longueur;
        uint32_t premier  = old_addr[i].premier;

        if (longueur == 0xffffffffUL) continue; //case libre
        if (premier == 0xffffffffUL) continue; //pas de chaine pour ce hash

        uint32_t current = premier;
        size_t guard = 0;

        while (current != 0xffffffffUL && guard <= b_old) {
            if (current >= b_old) break;//par secu

            uint32_t obj_len = old_addr[current].longueur;

            char *buf = malloc(obj_len > 0 ? obj_len : 1);
            if (buf == NULL) { perror("malloc extend"); break; }

            uint32_t total, next;
            if (read_object_nolock(old_addr, current, b_old,buf, obj_len, &total, &next) != 0) {
                free(buf);
                break;
            }

            //si c'est un nom il faut aussi recalculer le hash des fichiers 
            if (total >= 1 && buf[0] == 'n') {
                update_name_entry_hfiles(buf, total, old_addr, b_old, ea->b_new, ea->hash);
            }

            uint32_t new_hash = recompute_hash(buf, total, obj_len, ea->hash, ea->b_new);
            if (new_hash != UINT32_MAX) {
                insertbuff_at(ea->slowns, buf, ea->new_fd, ea->new_size, total, new_hash, NULL);
            }
            free(buf);
            current = next;
            guard++;
        }
    }

    return NULL;
}

static int make_empty_table(const char *path, size_t b_new, long slowns) {
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open temp");
        return -1;
    }
    int rc = make(slowns, (uint32_t)b_new, fd); 
    close(fd);
    return rc;
}


int resize(long slowns, const char *table_path, type_hash_t hash,     size_t size, size_t b_new, size_t nb_threads) {
    size_t b_old = size / 1024;
    size_t new_size = b_new * 1024;

    if (b_new == 0) {
        fprintf(stderr, "resize : taille cible nulle, abandon\n");
        return -1;
    }

    //chemain temporaire
    size_t tmp_len = strlen(table_path) + 5; // + .tmp + \0 donc 5 
    char *tmp_path = malloc(tmp_len);
    if (tmp_path == NULL) { perror("malloc tmp_path"); return -1; }
    snprintf(tmp_path, tmp_len, "%s.tmp", table_path);

    //on cree la nouvelle table vide
    if (make_empty_table(tmp_path, b_new,slowns) != 0) {
        free(tmp_path);
        return -1;
    }

    //On ouvre l'ancienne en lecture et la nouvelle en ecriture
    int old_fd = open(table_path, O_RDONLY);
    if (old_fd == -1) { perror("open ancienne table"); free(tmp_path); return -1; }

    struct hache_entree *old_addr = mmap(NULL, size, PROT_READ, MAP_SHARED, old_fd, 0);
    if (old_addr == MAP_FAILED) {
        perror("mmap ancienne");
        close(old_fd);
        free(tmp_path);
        return -1;
    }

    int new_fd = open(tmp_path, O_RDWR);
    if (new_fd == -1) {
        perror("open nouvelle table");
        munmap(old_addr, size);
        close(old_fd);
        free(tmp_path);
        return -1;
    }

    if (init_block_mutexes(b_new) != 0) {
        munmap(old_addr, size);
        close(old_fd);
        close(new_fd);
        free(tmp_path);
        return -1;
    }
    
    pthread_t threads[nb_threads];
    struct extend_args *ea = malloc(nb_threads * sizeof(struct extend_args));
    if (ea == NULL) {
        perror("malloc extend_args");
        destroy_block_mutexes();
        munmap(old_addr, size);
        close(old_fd);
        close(new_fd);
        free(tmp_path);
        return -1;
    }
    size_t zone = b_old / nb_threads;

    for (size_t i = 0; i < nb_threads; i++) {
        ea[i].old_addr   = old_addr;
        ea[i].b_old      = b_old;
        ea[i].start = i * zone;
        ea[i].end   = (i == nb_threads - 1) ? b_old : (i + 1) * zone;
        ea[i].new_fd     = new_fd;
        ea[i].new_size   = new_size;
        ea[i].b_new      = b_new;
        ea[i].hash       = hash;
        ea[i].slowns     = slowns;

        int rc = pthread_create(threads + i, NULL, resize_worker, &ea[i]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create : %s\n", strerror(rc));
            // on join les thread 
            for (size_t j = 0; j < i; j++) pthread_join(threads[j], NULL);
            destroy_block_mutexes();
            munmap(old_addr, size);
            close(old_fd);
            close(new_fd);
            free(ea);
            free(tmp_path);
            return -1;
        }
    }
    for (size_t i = 0; i < nb_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    destroy_block_mutexes();
    munmap(old_addr, size);
    close(old_fd);
    close(new_fd);
    free(ea);

    //on rename la tmp 
    if (rename(tmp_path, table_path) != 0) {
        perror("rename");
        free(tmp_path);
        return -1;
    }
    free(tmp_path);
    return 0;
}

int extend(long slowns, const char *table_path, type_hash_t hash,size_t size, size_t nb_threads){
    size_t b_old = size / 1024;
    return resize(slowns, table_path, hash, size, b_old * 2, nb_threads);
}

int shrink(long slowns, const char *table_path, type_hash_t hash,size_t size, size_t nb_threads){
    size_t b_old = size / 1024;
    if (b_old < 2) {
        fprintf(stderr, "shrink : table déjà minimale\n");
        return -1;
    }
    return resize(slowns, table_path, hash, size, b_old / 2, nb_threads);
}

int ajust(long slowns, const char *table_path, type_hash_t hash,size_t size, size_t nb_threads){
    size_t b = size / 1024;

    //on calcul le taux de remplissage via free_blocks
    int fd = open(table_path, O_RDONLY);
    if (fd == -1) {perror("open ajust"); return -1;}

    size_t free_count = 0, object_count = 0;
    if (free_blocks(slowns, fd, size, &free_count, &object_count) != 0){
        close(fd);
        return -1;
    }
    close(fd);

    size_t occupied = b-free_count;
    double taux = (double)occupied/(double)b;

    if (taux > 0.70) {
        printf("Taux %.0f%% > 70%% : extend\n", taux * 100);
        fflush(stdout);
        return extend(slowns, table_path, hash, size, nb_threads);
    } else if (taux < 0.10) {
        printf("Taux %.0f%% < 10%% : shrink\n", taux * 100);
        fflush(stdout);
        return shrink(slowns, table_path, hash, size, nb_threads);
    } else {
        printf("Taux %.0f%% : rien à faire\n", taux * 100);
        return 0;
    }
}


