#ifndef HASH
#define HASH

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//type pour les differents hash existant (pour l'instant les 2 initiaux)
typedef enum {
    HASH_SHA1,
    HASH_CKS
} type_hash_t;

uint32_t checksum32(unsigned char *data, int taille, uint32_t b);
uint32_t sha1(unsigned char *data,int taille, uint32_t b);
uint32_t apply_hash(unsigned char *data,type_hash_t hash,uint32_t b,size_t data_size);
uint32_t recompute_hash(char *buf, uint32_t declared_len, size_t caped_len,type_hash_t hash, size_t b);

#endif