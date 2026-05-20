#ifndef AJUST_TABLE_CMD
#define AJUST_TABLE_CMD

#include "hash.h"

struct extend_args {
    struct hache_entree *old_addr;  // mmap de l'ancienne table 
    size_t b_old;                   // nb blocs ancienne
    size_t start;                   // premiere case a traiter
    size_t end;                     // dernière (exclue)
    int new_fd;                     // fd de la nouvelle table
    size_t new_size;                // taille de la nouvelle table en octets
    size_t b_new;                   // nb blocs nouvelle
    type_hash_t hash;
    long slowns;
};

int ajust(long slowns, const char *table_path, type_hash_t hash,size_t size, size_t nb_threads);
int shrink(long slowns, const char *table_path, type_hash_t hash,size_t size, size_t nb_threads);
int extend(long slowns, const char *table_path, type_hash_t hash,size_t size, size_t nb_threads);

#endif





