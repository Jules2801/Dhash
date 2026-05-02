#include <stdint.h>
#include <stdio.h>

#include "../include/hash.h"
#include "../include/util.h"
#include "../include/file.h"
#include "../include/type.h"






int find_free_block(struct hache_entree *addr, int b, uint32_t index) {
    //permet de boucler sur tous les bloques en commencant par le bloque suivant 
    for (size_t i = 1; i <= b; i++) {
        int test_index = (index + i) % b; 
        if (addr[test_index].longueur == 0xffffffffUL) {
            return test_index; 
        }
    }
    
    return -1; 
}


