#ifndef HASH
#define HASH

#include "stdbool.h"
#include <stdint.h>

uint32_t checksum32(unsigned char *data, int taille, int b);
uint32_t sha1(unsigned char *data,int taille, uint32_t b);

#endif