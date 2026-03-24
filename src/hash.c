#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "hash.h"

uint32_t checksum32(unsigned char *data, int taille, int b){
    uint32_t sum = 0;
    for (int i = 0; i<taille; i++) {
        sum += data[i];
    }
    sum = sum % b;
    return sum;
}

unsigned char *sha1(unsigned char *data, int taille, int b){
    unsigned char *res = malloc(SHA_DIGEST_LENGTH);
    if(res == NULL){
        fprintf(stderr, "sha1 malloc error");
        return NULL;
    }
    if (!SHA1(data, taille, res)) {
        fprintf(stderr, "sha1 error");
        return NULL;
    }
    
    for(int i = 0; i < SHA_DIGEST_LENGTH; i++){
        printf("%02x", res[i]);
    }
    printf("\n");
    return res;
}