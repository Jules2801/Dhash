#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#include "../include/handlers.h"
#include "../include/hash.h"
#include "../include/command.h"
#include "../include/file.h"


int print_file_stats(int fd, const char *filename,struct stat* sb) {
    struct stat local_sb;

    if (fstat(fd, &local_sb) == -1) {
        perror("fstat");
        return -1;
    }
    
    printf("nom du fichier : %s\n", filename);
    printf("taille du fichier : %lld octets\n", (long long)local_sb.st_size);
    printf("dernière modif du fichier : %s", ctime(&local_sb.st_mtime));
    
    if (sb != NULL) {
        *sb = local_sb; 
    }

    return 0;
}


int check_extension(char* filename){

    bool is_cks = has_extension(filename , ".cks");
    bool is_sha = has_extension(filename , ".sha");


    if (!is_cks && !is_sha) {
        fprintf(stderr, "il faut un fichier en *.cks ou *.sha\n");
        return 1;
    }
    if(is_cks){
        return HASH_CKS;
    }else{
        return HASH_SHA1;
    }
    return -1;
}



int handle_make(int argc, char **argv){
    
    if (argc != 3) {
        fprintf(stderr, "il faut 3 paramètres\n");
        return 1;
    }

    if(check_extension(argv[0])==-1){
        return 1;
    }
    
    long slowns = atol(argv[1]);
    int taille = atoi(argv[2]);
    int fd = open(argv[0], O_RDWR| O_CREAT,0644);
    if (fd == -1) {
        perror("open");
        return 1;
    }
    print_file_stats(fd, argv[0],NULL);
    if(make(slowns, taille,fd)){
        perror("Échec de la commande make");
        return 1;
    }
    else{
        printf("[ make slowns : %ld, taille : %d ]\n",slowns,taille);    
        return 0;
    } 
}

int handle_insertstring(int argc, char **argv){

    if(argc!=3){
        fprintf(stderr, "il faut 3 paramètres\n");
        return 1;
    }
    type_hash_t t_hash;
    if((t_hash = check_extension(argv[0]))==-1){
        return 1;
    }

    long slowns = atol(argv[1]);
    int fd = open(argv[0], O_RDWR,0644);
    if (fd == -1) {
        perror("open");
        return 1;
    }
    
    char* data = argv[2];

    struct stat sb;
    print_file_stats(fd, argv[0],&sb);
    
    if(insertstring(slowns, data, fd, t_hash,sb.st_size)){
        perror("Échec de la commande insertstring");
        return 1;
    }
    else {
        printf("insertstring [ slowns : %ld, string : %s ]\n",slowns,data); 
        return 0;
    }


}

int handle_free(int argc,char** argv){
    if(argc!=2){
        fprintf(stderr, "il faut 2 paramètres\n");
        return 1;
    }
    
    if(check_extension(argv[0])==-1){
        return 1;
    }

    long slowns = atol(argv[1]);
    int fd = open(argv[0], O_RDONLY,0644);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    struct stat sb;
    print_file_stats(fd, argv[0],&sb);
    int fblocks;
    size_t size = sb.st_size;

    if((fblocks = free_blocks(slowns,fd,size))==-1){
        fprintf(stderr, "Erreur de la commande free\n");
        return 1;
    }

    size_t blocks = size/1024;
    double filling_rate  = ((double)(blocks - fblocks) / (double)blocks) * 100.0;

    printf("%d bloque(s) libre(s) sur %zu bloque(s)\n",fblocks,blocks);
    printf("Taux de remplissage : %.2f%%\n",filling_rate);

    return 0;



}