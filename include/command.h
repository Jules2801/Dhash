#ifndef COMMAND_H
#define COMMAND_H

#include "hash.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "type.h"


/**
 * @brief Vérifie si l'objet stocké à partir de start_block correspond à data.
 * !!! start_block doit etre lock en ecriture ou lecture !!! 
 * @return 1 si l'objet == data, 0 sinon.
 */
int is_same_string(int fd, struct hache_entree *addr, uint32_t start_block, const char *data, long slowns, size_t data_len);

/**
 * @brief Initialise une nouvelle table de hachage vide.
 * @param slowns Nombre de nanosecondes durant lequel attendre
 * @param size_blocks Nombre b de blocs (de 1024 octets) à allouer.
 * @param fd Descripteur du fichier déjà ouvert 
 * @return 0 si succès, -1 si erreur (errno sera défini).
 */
int make(long slowns, uint32_t size_block,int fd);


int free_blocks(long slowns, int fd, size_t size,size_t *free_count, size_t *object_count) ;




int lookup(long slowns, char *data, int fd, type_hash_t hash, size_t size);

typedef enum {
    ACTION_INFO,
    ACTION_PRINT,
    ACTION_SAVE
} lookup_action_t;

/**
 * @brief Recherche un fichier par nom et affiche ses métadonnées et les hashes des versions
 * @param filename Nom du fichier à chercher
 * @param fd descripteur de la table
 * @param size taille de la table
 * @return 0 si trouvé, 1 si non trouvé, -1 si erreur
 */
int lookup_file_by_name(long slowns, const char *filename, int fd, type_hash_t hash, size_t size, lookup_action_t action, const char *save_dest, int version);


int deletehash(long slowns, uint32_t index, int fd, size_t size);

char detect_key_type(long slowns, const char *key, int fd, type_hash_t hash, size_t size);

#endif

