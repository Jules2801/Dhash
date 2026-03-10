#include <stdio.h>
#include <string.h>


int main(int argc,char **argv){
    if (argc == 1) {
        fprintf(stderr, "il faut une commande");
        return 1;
    }
    printf("%d\n",argc);
    for (int i = 0; i<argc ; i++) {
        printf("%s\n",argv[i]);
    }
    if (strcmp(argv[1], "make") == 0) {
        printf("make is used\n");
    }
}