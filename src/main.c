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
#include "../include/file.h"


int main(int argc,char **argv){
    if (argc == 1) {
        fprintf(stderr, "il faut une commande\n");
        return 1;
    }
    
    if (strcmp(argv[1], "make") == 0) {
        handle_make(argc-2, argv+2);
    }
    if (strcmp(argv[1], "free") == 0) {
        handle_free(argc-2,argv+2);
    }
    if (strcmp(argv[1], "insertstring") == 0) {
        handle_insertstring(argc-2, argv+2);
        
    }
    if (strcmp(argv[1], "insertfile") == 0) {
        printf("free\n");
    }
    if (strcmp(argv[1], "lookup") == 0) {
        handle_lookup(argc-2, argv+2);
    }
    if (strcmp(argv[1], "deletehash") == 0) {
        printf("free\n");
    }
    if (strcmp(argv[1], "delete") == 0) {
        printf("free\n");
    }
    if (strcmp(argv[1], "check") == 0) {
        printf("free\n");
    }
    if (strcmp(argv[1], "extend") == 0) {
        printf("free\n");
    }    
    if (strcmp(argv[1], "shrink") == 0) {
        printf("free\n");
    }    
    if (strcmp(argv[1], "ajust") == 0) {
        printf("free\n");
    }  


   

}