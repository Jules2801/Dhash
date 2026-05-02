#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/stat.h>

#include "../include/hash.h"
#include "../include/command.h"
#include "../include/file.h"
#include "../include/type.h"
#include "../include/utils.h"


int make(long slowns, uint32_t taille,int fd) {
    if (sleep_nano(slowns)) {
        return 1;
    }
    ftruncate(fd, taille*1024);

    struct hache_entree * addr = mmap(NULL, taille*1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (addr == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    for (int i = 0; i < taille; i++) {
        addr[i].longueur = 0xffffffffUL;  
        addr[i].premier = i;
        //apriori pas necessaire mais faisons le par securité
        addr[i].suivant = 0xffffffffUL;
    }
    munmap(addr, taille*1024);
    
    return 0;
}

int insertstring(long slowns,char *data, int fd,int hash,size_t size){

       

    if (sleep_nano(slowns)) {
        return 1;
    }

    struct hache_entree * addr = mmap(NULL, size*1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    uint32_t index;

    size_t b = size/1024;
    size_t data_size = strlen((char *)data);

    if(hash==HASH_CKS){
         index = checksum32(data, data_size, b);
    }else{
        if(hash==HASH_SHA1){
            index = sha1(data, data_size, b);
        }
        else{
            perror("La fonction de hachage n'existe pas.");
            munmap(addr,size*1024);
            return 1;
        }
    }

    size_t remaining_size = strlen((char *)data); 
    size_t data_cursor = 0;                       
    uint32_t current_block = index;
    
    if(addr[index].longueur==0xffffffffUL){
        //Bloque vide
       while (remaining_size > 0) {
            size_t chunk_size; 

            if (remaining_size < 1012) {
                chunk_size = remaining_size; 
            } else {
                chunk_size = 1012;           
            }
            
            memcpy(addr[current_block].obj, data + data_cursor, chunk_size);
            addr[current_block].longueur = (uint32_t)chunk_size; 
            
            remaining_size -= chunk_size;
            data_cursor += chunk_size;

            if(remaining_size >0){
                int next_block;
                if((next_block=find_free_block(addr,b,index))==-1){
                    perror("Plus de place dans le bloque");
                    munmap(addr,size*1024);
                }else{
                    addr[current_block].suivant = (uint32_t) next_block;
                    addr[next_block].premier = index;
                    current_block = (uint32_t) next_block;
                }
            }
        }
    }

    return 0;

}

