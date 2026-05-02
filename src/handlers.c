#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
    
    if (fstat(fd, sb) == -1) {
        perror("fstat");
        return -1;
    }
    
    printf("nom du fichier : %s\n", filename);
    printf("taille du fichier : %lld octets\n", (long long)sb->st_size);
    printf("dernière modif du fichier : %s", ctime(&sb->st_mtime));
    
    return 0;
}


int check_extension(char* filename,type_hachage_t* type_hash){

    bool is_cks = has_extension(filename , ".cks");
    bool is_sha = has_extension(filename , ".sha");


    if (!is_cks && !is_sha) {
        fprintf(stderr, "il faut un fichier en *.cks ou *.sha\n");
        return 1;
    }
    if(is_cks){
        *type_hash = HASH_CKS;
    }else{
        *type_hash = HASH_SHA1;
    }
    return 0;
}



int handle_make(int argc, char **argv){
    
    if (argc != 3) {
        fprintf(stderr, "il faut 3 paramètres\n");
        return 1;
    }

    type_hachage_t t_hash;

    if(check_extension(argv[0],&t_hash)){
        return 1;
    }
    
    long slowns = atol(argv[1]);
    int taille = atoi(argv[2]);
    int fd = open(argv[0], O_RDWR| O_CREAT,0644);
    if (fd == -1) {
        perror("open");
        return 1;
    }
    struct stat sb;
    print_file_stats(fd, argv[0],&sb);
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
    type_hachage_t t_hash;
    if(check_extension(argv[0],&t_hash)){
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