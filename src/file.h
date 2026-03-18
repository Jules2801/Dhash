#ifndef FILE
#define FILE

#include "stdbool.h"

// commande make 
bool make(char *nom, long slowns, int taille);

bool free_file(char *nom,int slowns);

#endif