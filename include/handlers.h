#ifndef HANDLERS_H
#define HANDLERS_H

/**
 * @brief Gère la commande 'make' [5.1]
 * Usage : ./hachement make <fichier_table> <taille_en_blocs>
 * @param argc Nombre d'arguments
 * @param argv Tableau des arguments
 * @return 0 si succès, 1 si erreur
 */
int handle_make(int argc, char **argv);

/**
 * @brief Gère la commande 'free' [5.2]
 * Usage : ./hachement free
 * @param argc Nombre d'arguments
 * @param argv Tableau des arguments
 * @return 0 si succès, 1 si erreur
 */
int handle_free(int argc, char **argv);

/**
 * @brief Insere une chaine de charatères dans la table si elle n'est pas déjà presente [5.3]
 * Usage : ./hachement insertstring <fichier_table> <slowns> <chaine>
 * @param argc Nombre d'arguments
 * @param argv Tableau des arguments
 * @return 0 si succès, 1 si erreur
 */
int handle_insertstring(int argc, char **argv);

/**
 * @brief Insere un fichier dans la table [5.4]
 * Usage : ./hachement insertfile <fichier_table> <slowns> <fichier_a_inserer>
 * @param argc Nombre d'arguments
 * @param argv Tableau des arguments
 * @return 0 si succès, 1 si erreur
 */
int handle_insertfile(int argc, char **argv);

/**
 * @brief Gère la commande 'lookup' (Recherche d'une donnée) [5.5]
 * Usage : ./hachement lookup <fichier_table> <slowns> (<chaine> ou <fichier>)
 * @param argc Nombre d'arguments
 * @param argv Tableau des arguments
 * @return 0 si succès, 1 si erreur
 */
int handle_lookup(int argc, char **argv);

/**
 * @brief Supprimer la (ou toutes les) entree(s) dont le hascode est donnee [5.6]
 * Usage : ./hachement deletehash ...
 * @param argc Nombre d'arguments
 * @param argv Tableau des arguments
 * @return 0 si succès, 1 si erreur
 */
int handle_deletehash(int argc, char **argv);


/**
 * @brief Gère la commande 'delete' (Suppression d'une donnée) [5.7]
 * Usage : ./hachement delete <fichier_table> <slowns> donnee
 * @param argc Nombre d'arguments
 * @param argv Tableau des arguments
 * @return 0 si succès, 1 si erreur
 */
int handle_delete(int argc, char **argv);

/**
 * @brief Gère la commande 'check' [5.8]
 * Usage : ./hachement check 
 * @param argc Nombre d'arguments
 * @param argv Tableau des arguments
 * @return 0 si succès, 1 si erreur
 */
int handle_check(int argc, char **argv);

/**
 * @brief [5.9]
 * Usage : 
 * @param argc Nombre d'arguments
 * @param argv Tableau des arguments
 * @return 0 si succès, 1 si erreur
 */
int handle_extend(int argc, char **argv);

/**
 * @brief [5.9]
 * Usage : 
 * @param argc Nombre d'arguments
 * @param argv Tableau des arguments
 * @return 0 si succès, 1 si erreur
 */
int handle_shrink(int argc, char **argv);

/**
 * @brief [5.9]
 * Usage : 
 * @param argc Nombre d'arguments
 * @param argv Tableau des arguments
 * @return 0 si succès, 1 si erreur
 */
int handle_ajust(int argc, char **argv);

/**
 * @brief Gère la commande 'insertlines' table file slown nb threads
 * @param argc Nombre d'arguments
 * @param argv Tableau des arguments
 * @return 0 si succès, 1 si erreur
 */
int handle_insertlines(int argc, char **argv);

/**
 * @brief Affiche l'aide globale du programme si l'utilisateur se trompe
 * Usage : ./hachement, sans rien ou avec des arguments non valide 
 * @param nom_programme Le nom de l'exécutable (généralement argv[0])
 */
void print_usage(const char *nom_programme);

#endif 