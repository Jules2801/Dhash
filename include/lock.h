#ifndef LOCK_H
#define LOCK_H

#include <stdint.h>
#include <fcntl.h>
#include <time.h>
#include <stdbool.h>


#define BLOCK_SIZE 1024

/**
 * @brief Pose un verrou POSIX (fcntl) en mode ecriture (bloquant )sur un bloc spécifique du fichier
 * @param fd          Descripteur du fichier ouvert
 * @param block_index L'index du bloc à verrouiller (de 0 à b-1)
 * @param lock_type   F_RDLCK (verrou partagé en lecture) ou F_WRLCK (verrou exclusif en écriture)
 * @return 0 en cas de succès, -1 en cas d'erreur 
 */
int lock_write_block(int fd, uint32_t block_index);

/**
 * @brief Pose un verrou POSIX (fcntl) en mode lecture (bloquant )sur un bloc spécifique du fichier
 * @param fd          Descripteur du fichier ouvert
 * @param block_index L'index du bloc à verrouiller (de 0 à b-1)
 * @param lock_type   F_RDLCK (verrou partagé en lecture) ou F_WRLCK (verrou exclusif en écriture)
 * @return 0 en cas de succès, -1 en cas d'erreur 
 */
int lock_read_block(int fd, uint32_t block_index);


/**
 * @brief Libère un verrou POSIX existant sur un bloc
 * @param fd          Descripteur du fichier ouvert
 * @param block_index L'index du bloc à déverrouiller (de 0 à b-1)
 * * @return 0 en cas de succès, -1 en cas d'erreur 
 */
int unlock_block(int fd, uint32_t block_index);

bool sleep_nano(long slowns);



#endif 