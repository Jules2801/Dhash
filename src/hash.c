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

uint32_t sha1(unsigned char *data, int taille, uint32_t b){
    unsigned char hash[SHA_DIGEST_LENGTH];
    if (!SHA1(data, taille, hash)) {
        fprintf(stderr, "sha1 error");
        return 0;
    }
    
    // for(int i = 0; i < SHA_DIGEST_LENGTH; i++){
    //     printf("%02x", hash[i]);
    // }
    printf("\n");
    uint32_t dernier = *(uint32_t *)&hash[16];
    //printf("val sha : %08x\n",dernier%b);
    return dernier%b;
}