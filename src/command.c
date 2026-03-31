#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "command.h"
#include "file.h"

bool make(char *nom,long slowns, int taille){
    printf("make slowns : %ld, taille : %d\n",slowns,taille);

    if (sleep(slowns)) {
        return true;
    }

    
    
    return false;
}

