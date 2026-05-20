#include "../include/lock.h"
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <time.h>


static pthread_mutex_t *block_mutexes = NULL;
static size_t num_total_blocks = 0;


int init_block_mutexes(size_t num_blocks) {
    if (block_mutexes != NULL) return 0; //deja initialisé

    block_mutexes = malloc(num_blocks * sizeof(pthread_mutex_t));
    if (!block_mutexes) {
        perror("malloc block_mutexes");
        return -1;
    }
    for (size_t i = 0; i < num_blocks; i++) {
        pthread_mutex_init(&block_mutexes[i], NULL);
    }
    num_total_blocks = num_blocks;
    return 0;
}


void destroy_block_mutexes() {
    if (block_mutexes) {
        for (size_t i = 0; i < num_total_blocks; i++) {
            pthread_mutex_destroy(&block_mutexes[i]);
        }
        free(block_mutexes);
        block_mutexes = NULL;
    }
}


int lock_block(int fd, uint32_t block_index, short lock_type){

    if(lock_type==F_UNLCK){
        perror("F_UNLCK donné à lock_block");
        return -1;
    }

    //LOCK LE MUTEX AVANT !!!
    //evite qu'un thread passe au travers du verrou posix et une fois le mutex unlock
    //fasse des modification sans verrou posix lock

    if (block_mutexes && block_index < num_total_blocks) {
        int rc = (pthread_mutex_lock(&block_mutexes[block_index]));
        if(rc!=0){
            fprintf(stderr, "Erreur pthread_mutex_lock bloc %u : %s\n", block_index, strerror(rc));
            return -1;
        }
    }

    struct flock fl = {0};
    fl.l_type = lock_type;
    fl.l_whence = SEEK_SET;
    fl.l_start = block_index * BLOCK_SIZE;
    fl.l_len = BLOCK_SIZE;
    
    if (fcntl(fd, F_SETLKW, &fl) == -1) {
        perror("fcntl lock_block");
        return -1;
    }

    return 0;

}

int lock_write_block(int fd, uint32_t block_index){
    return lock_block(fd,block_index, F_WRLCK);
}

int lock_read_block(int fd, uint32_t block_index){
    return lock_block(fd,block_index, F_RDLCK);
}

int unlock_block(int fd, uint32_t block_index){

    struct flock fl = {0};
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = block_index * BLOCK_SIZE;
    fl.l_len = BLOCK_SIZE;

    if(fcntl(fd,F_SETLKW,&fl)==-1){
        perror("fcntl unlock_block");
        return -1;
    }

    if (block_mutexes && block_index < num_total_blocks) {
        int rc = (pthread_mutex_unlock(&block_mutexes[block_index]));
        if(rc!=0){
            fprintf(stderr, "Erreur pthread_mutex_unlock bloc %u : %s\n", block_index, strerror(rc));
            return -1;
        }
        
    }

    return 0;
}

bool sleep_nano(long slowns){
    // pas de sleep si slowns = 0
    if (slowns == 0) {
        return false;
    }
    struct timespec time;
    time.tv_sec  = slowns / 1000000000L;
    time.tv_nsec = slowns % 1000000000L;
    if (nanosleep(&time, NULL) != 0) {
        perror("nanosleep");
        return true; 
    }
    return false;
}


