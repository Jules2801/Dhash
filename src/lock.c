#include "../include/lock.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <time.h>


int lock_block(int fd, uint32_t block_index, short lock_type){

    if(lock_type==F_UNLCK){
        perror("F_UNLCK donné à lock_block");
        return -1;
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


