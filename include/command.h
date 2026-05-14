#ifndef COMMAND_H
#define COMMAND_H

#include "hash.h"
#include <stddef.h>
#include <stdint.h>



/**
 * @brief Initialise une nouvelle table de hachage vide.
 * @param slowns Nombre de nanosecondes durant lequel attendre
 * @param size_blocks Nombre b de blocs (de 1024 octets) à allouer.
 * @param fd Descripteur du fichier déjà ouvert 
 * @return 0 si succès, -1 si erreur (errno sera défini).
 */
int make(long slowns, uint32_t size_block,int fd);


int free_blocks(long slowns,int fd,size_t size);


int insertstring(long slowns,char *data,int fd,type_hash_t hash,size_t size);

int lookup(long slowns, char *data, int fd, type_hash_t hash, size_t size);

#endif


#ifndef CMD_H
#define CMD_H

#include <stdint.h>
#include <stddef.h>








/**
 * @brief Insère une chaîne de caractères dans la table.
 * @param fd Le descripteur du fichier de la table ouvert.
 * @param data La chaîne de caractères à insérer.
 * @param type Le type de fonction de hachage à utiliser (SHA1 ou CKS).
 * @return 0 si succès, -1 si erreur système, 1 si doublon.
 */
int cmd_insertstring(int fd, const char *data);

// (Les futures commandes viendront ici : cmd_insertfile, cmd_lookup, etc.)

#endif /* CMD_H */