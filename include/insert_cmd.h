#ifndef INSERT_CMD_H
#define INSERT_CMD_H

#include "hash.h"
#include "type.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

struct hache_entree;

struct file_metadata {
    uint64_t size;       // taille du fichier
    uint64_t mtime;      // timestamp modif
    uint32_t mode;       // permissions
    uint32_t name_len;   // taille du nom
};

struct chunk_read {
    char *file_addr;       
    size_t file_size;      
    size_t start;          
    size_t end;            
    int fd;               
    type_hash_t hash;
    size_t table_size;
    long slowns;
};

/**
 * @brief Écrit data dans la chaîne de blocs commençant à start_block
 *
 * @param fd          Descripteur du fichier table
 * @param addr        Adresse du mmap de la table
 * @param data_size   Taille de data en octets
 * @param data        Données à écrire
 * @param start_block Bloc de départ locké en écriture et marqué occupé par l'appelant (0 dans la len)
 * @param slowns      Nombre de nanosecondes d'attente après chaque accès
 * @param b           Nombre total de blocs de la table
 *
 * @return 0 si succès, 1 si table pleine pendant l'écriture
 */
int fill_data(int fd, struct hache_entree *addr, size_t data_size,
              char *data, uint32_t start_block, long slowns, size_t b);

/**
 * @brief Insère une chaîne de caractères dans la table
 *
 * @param slowns Nombre de nanosecondes d'attente après chaque accès
 * @param data   Chaîne à insérer
 * @param fd     Descripteur du fichier table
 * @param hash   Type de fonction de hachage à utiliser
 * @param size   Taille totale de la table en octets
 *
 * @return 0 si succès ou si la chaîne est déjà présente (warning),
 *         1 si erreur.
 */
int insertstring(long slowns, char *data, int fd, type_hash_t hash, size_t size);

int insertfile(long slowns,int fd,int file_fd,char* file_name,type_hash_t hash, size_t sizes,struct stat* sb_file);

int insertlines(long slowns,size_t nb_threads, int fd,int file_fd, type_hash_t hash, size_t size, struct stat* sb_file);


#endif