#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>


#include "../include/handlers.h"


int main(int argc,char **argv){
    if (argc == 1) {
        fprintf(stderr, "il faut une commande\n");
        return 1;
    }
    
    if (strcmp(argv[1], "make") == 0) {
        return handle_make(argc-2, argv+2);
    }
    if (strcmp(argv[1], "free") == 0) {
        return handle_free(argc-2,argv+2);
    }
    if (strcmp(argv[1], "insertstring") == 0) {
        return handle_insertstring(argc-2, argv+2);
    }
    if (strcmp(argv[1], "insertfile") == 0) {
        return handle_insertfile(argc-2, argv+2);
    }
    if (strcmp(argv[1], "lookup") == 0) {
        return handle_lookup(argc-2, argv+2);
    }
    if (strcmp(argv[1], "deletehash") == 0) {
        return handle_deletehash(argc-2, argv+2);
    }
    if (strcmp(argv[1], "delete") == 0) {
        return handle_delete(argc-2, argv+2);
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
    if (strcmp(argv[1], "insertlines") == 0) {
        return handle_insertlines(argc-2, argv+2);
    }


   fprintf(stderr, "commande inconnue\n");
   return 1;

}