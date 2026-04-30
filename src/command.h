#ifndef COMMAND_H
#define COMMAND_H

#include "stdbool.h"

// commande make 
bool make(char *nom, long slowns, int taille,int fd);
bool free_file(char *nom,int slowns);

#endif