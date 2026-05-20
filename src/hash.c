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
        return UINT32_MAX;
    }
    
    // for(int i = 0; i < SHA_DIGEST_LENGTH; i++){
    //     printf("%02x", hash[i]);
    // }
    // printf("\n");
    uint32_t dernier;
    memcpy(&dernier, &hash[16], sizeof(uint32_t));
    // printf("val SHA1 : %d\n", dernier % b);
    return dernier % b;
}


uint32_t apply_hash(unsigned char *data,type_hash_t hash,uint32_t b,size_t data_size){
    if(hash==HASH_CKS){
        return checksum32(data, data_size, b);
    }else{
        if(hash==HASH_SHA1){
            return sha1(data, data_size, b);
        }
        else{
            return (uint32_t)-1;
        }
    }
} 


bool try_hash_as_name(char *buf, uint32_t declared_len, size_t caped_len,type_hash_t hash, size_t b, uint32_t *out_hash) {
    
    size_t header = 1 + sizeof(uint32_t);
    if (declared_len < header || caped_len < header || buf[0] != 'n') return false;

    uint32_t name_len;
    memcpy(&name_len, buf + 1, sizeof(uint32_t));

    if ((size_t)name_len > caped_len - header) return false;
    if (header + (size_t)name_len > declared_len) return false;
    size_t rest = declared_len - header - name_len;
    if (rest < sizeof(uint32_t) || rest % sizeof(uint32_t) != 0) return false;

    *out_hash = apply_hash((unsigned char *)(buf + header), hash, b, name_len);
    return true;
}


uint32_t hash_as_content(const char *buf, uint32_t declared_len, size_t caped_len,type_hash_t hash, size_t b) {
    // marqueur (1 octet) + contenu
    if (declared_len < 1 || caped_len < 1) {
        return UINT32_MAX;
    }
    // On hashe à partir de buf+1 (après le type)
    // borné par ce qu'on a réellement lu (caped_len-1)
    size_t hlen = declared_len - 1;
    size_t safe = caped_len - 1;
    if (hlen > safe) hlen = safe;
    return apply_hash((unsigned char *)(buf + 1), hash, b, hlen);
}


uint32_t recompute_hash(char *buf, uint32_t declared_len, size_t caped_len,type_hash_t hash, size_t b) {
    uint32_t h;
    if (try_hash_as_name(buf, declared_len, caped_len, hash, b, &h)) {
        return h;
    }
    return hash_as_content(buf, declared_len, caped_len, hash, b);
}