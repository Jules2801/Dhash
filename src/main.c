#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#include "file.h"
#include "hash.h"


int main(int argc,char **argv){
    if (argc == 1) {
        fprintf(stderr, "il faut une commande");
        return 1;
    }
    unsigned char test[] = {"mklkvbmkslckmlqakjkjklmlmlmmlmmlmlmmaad"};
    
    sha1(test, sizeof(test)-1, 1);
    uint32_t check = checksum32(test, sizeof(test)-1, 1024*16);
    printf("%08x\n",check);
    //printf("%d\n",check);
    //printf("%d\n",argc);
    // for (int i = 0; i<argc ; i++) {
    //     printf("%s\n",argv[i]);
    // }
    if (strcmp(argv[1], "make") == 0) {
        if (argc != 5) {
            fprintf(stderr, "il faut 4 paramètres");
            return 1;
        }
        long slowns = atol(argv[3]);
        int taille = atoi(argv[4]);
        int fd = open(argv[2], O_RDWR| O_CREAT,0644);
        if (fd == -1) {
            perror("open");
            return 1;
        }
        struct stat sb;
        int res = fstat(fd, &sb);
        if (res == -1) {
            perror("fstat");
            return 1;
        }
        printf("nom du fichier : %s\n",argv[2]);
        printf("taille du fichier : %ld\n",sb.st_size);
        printf("dernière modif du fichier : %s\n",ctime(&sb.st_mtime));
        return make(argv[2], slowns, taille);
    }
    if (strcmp(argv[1], "free") == 0) {
        printf("free\n");
    }

}