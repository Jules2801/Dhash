#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>
#include "type.h" 

/**
 * @brief Cherche la prochaine case libre (0xffffffffUL) dans la table.
 * @param addr Pointeur vers le début de la mémoire partagée (mmap).
 * @param b Le nombre total de blocs dans la table.
 * @param index L'index à partir duquel on commence à chercher.
 * @return L'index de la case libre, ou -1 si la table est pleine à 100%.
 */
int find_free_block(struct hache_entree *addr, uint32_t b, uint32_t index);


#endif 