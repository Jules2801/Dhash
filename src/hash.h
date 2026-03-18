#ifndef HASH
#define HASH

#include "stdbool.h"
#include <openssl/sha.h>

uint32_t checksum32(unsigned char *data, int taille, int b);
unsigned char * sha1(unsigned char *data,int taille, int b);

#endif