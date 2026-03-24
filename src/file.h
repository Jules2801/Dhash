#ifndef FILE
#define FILE

#include "stdbool.h"

// commande make 
bool make(char *nom, long slowns, int taille);
bool free_file(char *nom,int slowns);
int has_extension(const char *filename, const char *ext);

#endif