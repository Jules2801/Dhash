#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <openssl/sha.h>

#include "../include/hash.h"

uint32_t checksum32(unsigned char *data, int size, uint32_t b){
    
    //on cast la data en uint_32 pour lire 4 octets par 4 octets
    uint32_t* data32 = (uint32_t*) data;
    //verification de n/4
    uint32_t rest = size%4;
    uint32_t real_size =  size/4;

    uint32_t sum = 0;
    
    for (size_t i = 0; i<real_size; i++) {
        sum += data32[i];
    }
    if(rest>0){
        uint32_t tmp = 0;
        
        
        memcpy(&tmp,&data[size-rest],rest);

        //pour test l'histoire de 24 et 1 
        //printf("AVANT ntohl : 0x%08x\n", tmp);
    
        uint32_t letmp = htonl(tmp);
        //printf("APRES ntohl : 0x%08x\n", letmp);
        sum += letmp;
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
    uint32_t dernier;
    memcpy(&dernier, &hash[16], sizeof(uint32_t));
    return dernier % b;
}


uint32_t apply_hash(unsigned char *data,type_hash_t hash,uint32_t b){
    size_t data_size = strlen((char *)data);
    if(hash==HASH_CKS){
        return checksum32(data, data_size, b);
    }else{
        if(hash==HASH_SHA1){
            return sha1(data, data_size, b);
        }
        else{
            return -1;
        }
    }
} 