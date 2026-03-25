#ifndef TYPE
#define TYPE

#include <stdint.h>

struct hache_entree {
    uint32_t premier; // numero du premier bloc de la liste chain´ee
    uint32_t suivant; // numero du bloc suivant de la liste chain´ee
    uint32_t longueur; // longueur, en octets, de l’objet stock´e.
    char obj[1012]; // l’objet stock´e
};
#endif