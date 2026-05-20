#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "../include/handlers.h"
#include "../include/hash.h"
#include "../include/command.h"
#include "../include/file.h"
#include "../include/insert_cmd.h"


int print_file_stats(int fd, const char *filename,struct stat* sb) {
    struct stat local_sb;

    if (fstat(fd, &local_sb) == -1) {
        perror("fstat");
        return -1;
    }
    
    printf("nom du fichier : %s\n", filename);
    printf("taille du fichier : %lld octets\n", (long long)local_sb.st_size);
    printf("dernière modif du fichier : %s", ctime(&local_sb.st_mtime));
    
    if (sb != NULL) {
        *sb = local_sb; 
    }

    return 0;
}


int check_extension(char* filename){

    bool is_cks = has_extension(filename , ".cks");
    bool is_sha = has_extension(filename , ".sha");


    if (!is_cks && !is_sha) {
        fprintf(stderr, "il faut un fichier en *.cks ou *.sha\n");
        return -1;
    }
    if(is_cks){
        return HASH_CKS;
    }else{
        return HASH_SHA1;
    }
    return -1;
}



int handle_make(int argc, char **argv){
    
    if (argc != 3) {
        fprintf(stderr, "il faut 3 paramètres\n");
        return 1;
    }

    if(check_extension(argv[0])==-1){
        return 1;
    }
    
    long slowns = atol(argv[1]);
    int taille = atoi(argv[2]);
    int fd = open(argv[0], O_RDWR| O_CREAT,0644);
    if (fd == -1) {
        perror("open");
        return 1;
    }
    print_file_stats(fd, argv[0],NULL);
    if(make(slowns, taille,fd)){
        perror("Échec de la commande make");
        close(fd);
        return 1;
    }
    else{
        printf("[ make slowns : %ld, taille : %d ]\n",slowns,taille);    
        close(fd);
        return 0;
    } 
}

int handle_insertstring(int argc, char **argv){

    if(argc!=3){
        fprintf(stderr, "il faut 3 paramètres\n");
        return 1;
    }
    type_hash_t t_hash;
    if((t_hash = check_extension(argv[0]))==UINT32_MAX){
        return 1;
    }

    long slowns = atol(argv[1]);
    int fd = open(argv[0], O_RDWR,0644);
    if (fd == -1) {
        perror("open");
        return 1;
    }
    
    char* data = argv[2];

    struct stat sb = {0};
    print_file_stats(fd, argv[0],&sb);
    
    if(insertstring(slowns, data, fd, t_hash,sb.st_size)){
        perror("Échec de la commande insertstring");
        close(fd);
        return 1;
    }
    else {
        printf("insertstring [ slowns : %ld, string : %s ]\n",slowns,data); 
        close(fd);
        return 0;
    }

}

int handle_insertfile(int argc, char **argv){

    if(argc!=3){
        fprintf(stderr, "il faut 3 paramètres\n");
        return 1;
    }
    type_hash_t t_hash;
    if((t_hash = check_extension(argv[0]))==UINT32_MAX){
        return 1;
    }

    long slowns = atol(argv[1]);
    int fd = open(argv[0], O_RDWR,0644);
    if (fd == -1) {
        perror("open hashtable");
        return 1;
    }
    


    struct stat sb = {0};
    print_file_stats(fd, argv[0],&sb);
    
    int fd_file = open(argv[2],O_RDONLY,0644);
    if (fd_file == -1){
        perror("open file");
        close(fd);
        return 1;
    }
    struct stat sb_file = {0};
    if (fstat(fd_file, &sb_file) == -1) {
        perror("fstat file");
        perror("fstat file");
        close(fd_file);
        close(fd);
        return -1;
    }
    
    char *file_name = argv[2];

    if(insertfile(slowns, fd,fd_file,file_name,t_hash,sb.st_size,&sb_file)){
        fprintf(stderr, "Échec de la commande insertfile");
        close(fd_file);
        close(fd);
        return 1;
    }
    else {
        close(fd_file);
        close(fd);
        return 0;
    }
}

int handle_free(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "il faut 2 paramètres\n");
        return 1;
    }

    if (check_extension(argv[0]) == -1) {
        return 1;
    }

    long slowns = atol(argv[1]);
    int fd = open(argv[0], O_RDONLY, 0644);
    if (fd == -1) {
        perror("open");
        return 1;  
    }

    struct stat sb = {0};
    print_file_stats(fd, argv[0], &sb);

    size_t size = sb.st_size;
    size_t blocks = size / 1024;

    size_t free_count, object_count;
    if (free_blocks(slowns, fd, size, &free_count, &object_count) == -1) {
        fprintf(stderr, "Erreur de la commande free\n");
        close(fd);
        return 1;
    }

    size_t occupied_count = blocks - free_count;
    double filling_rate = (blocks > 0) ? (100.0 * (double)occupied_count / (double)blocks) : 0.0;

    printf("Blocs libres    : %zu sur %zu\n", free_count, blocks);
    printf("Blocs occupés   : %zu sur %zu\n", occupied_count, blocks);
    printf("Nombre d'objets : %zu\n", object_count);
    printf("Taux remplissage: %.2f%%\n", filling_rate);

    close(fd);
    return 0;
}

#define perror_line(msg) \
    fprintf(stderr, "%s:%d %s: %s\n", __FILE__, __LINE__, msg, strerror(errno))

int handle_lookup(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "il faut au moins 3 paramètres : <fichier_table> <slowns> <string/nom_fichier>\n");
        return 1;
    }

    type_hash_t t_hash = check_extension(argv[0]);
    if (t_hash == UINT32_MAX) {
        return 1;
    }

    long slowns = atol(argv[1]);
    int fd = open(argv[0], O_RDONLY, 0644);
    if (fd == -1) {
        perror_line("open");
        return 1;
    }

    struct stat sb = {0};
    struct stat sb_target = {0};
    bool is_file_target = (stat(argv[2], &sb_target) == 0 && S_ISREG(sb_target.st_mode));

    lookup_action_t action = ACTION_INFO;
    char save_dest[1024] = {0};
    int version = -1;

    if (is_file_target) {
        // Prompt user
        fprintf(stderr, "Choisir une action pour le fichier :\n");
        fprintf(stderr, "  1. Afficher les informations (info)\n");
        fprintf(stderr, "  2. Afficher le contenu (print)\n");
        fprintf(stderr, "  3. Sauvegarder le contenu (save)\n");
        fprintf(stderr, "Votre choix [1-3] (défaut: info) : ");

        char choice_buf[20];
        if (fgets(choice_buf, sizeof(choice_buf), stdin)) {
            size_t len = strlen(choice_buf);
            if (len > 0 && choice_buf[len - 1] == '\n') {
                choice_buf[len - 1] = '\0';
            }
            if (strcmp(choice_buf, "1") == 0 || strcasecmp(choice_buf, "info") == 0) {
                action = ACTION_INFO;
            } else if (strcmp(choice_buf, "2") == 0 || strcasecmp(choice_buf, "print") == 0) {
                action = ACTION_PRINT;
            } else if (strcmp(choice_buf, "3") == 0 || strcasecmp(choice_buf, "save") == 0) {
                action = ACTION_SAVE;
            } else {
                fprintf(stderr, "Choix inconnu, utilisation de l'action par défaut 'info'\n");
            }
        }

        if (action == ACTION_PRINT || action == ACTION_SAVE) {
            fprintf(stderr, "Entrez le numéro de version à récupérer (laisser vide pour la dernière) : ");
            char ver_buf[10];
            if (fgets(ver_buf, sizeof(ver_buf), stdin)) {
                size_t len = strlen(ver_buf);
                if (len > 0 && ver_buf[len - 1] == '\n') {
                    ver_buf[len - 1] = '\0';
                }
                if (strlen(ver_buf) > 0) {
                    version = atoi(ver_buf);
                }
            }
        }

        if (action == ACTION_SAVE) {
            while (1) {
                fprintf(stderr, "Entrez le chemin du fichier de destination : ");
                if (fgets(save_dest, sizeof(save_dest), stdin)) {
                    size_t len = strlen(save_dest);
                    if (len > 0 && save_dest[len - 1] == '\n') {
                        save_dest[len - 1] = '\0';
                    }
                    if (strlen(save_dest) > 0) {
                        break;
                    }
                } else {
                    break;
                }
                fprintf(stderr, "Le chemin de destination ne peut pas être vide.\n");
            }
        }
    }

    // Print table stats
    if (is_file_target && action == ACTION_PRINT) {
        struct stat local_sb;
        if (fstat(fd, &local_sb) == -1) {
            perror("fstat");
        } else {
            fprintf(stderr, "nom du fichier : %s\n", argv[0]);
            fprintf(stderr, "taille du fichier : %lld octets\n", (long long)local_sb.st_size);
            char time_buf[32];
            ctime_r(&local_sb.st_mtime, time_buf);
            fprintf(stderr, "dernière modif du fichier : %s", time_buf);
            sb = local_sb;
        }
    } else {
        print_file_stats(fd, argv[0], &sb);
    }

    if (is_file_target) {
        int rc = lookup_file_by_name(slowns, argv[2], fd, t_hash, sb.st_size, action, save_dest, version);
        if (rc == 0) {
            if (action == ACTION_PRINT) {
                fprintf(stderr, "lookup : trouvé\n");
            } else {
                printf("lookup : trouvé\n");
            }
            close(fd);
            return 0;
        }
        if (rc == 1) {
            if (action == ACTION_PRINT) {
                fprintf(stderr, "lookup : non trouvé\n");
            } else {
                printf("lookup : non trouvé\n");
            }
            close(fd);
            return 1;
        }
        fprintf(stderr, "Erreur de la commande lookup (fichier)\n");
        close(fd);
        return 1;
    }

    // sinon lookup classique sur la chaîne
    int res = lookup(slowns, argv[2], fd, t_hash, sb.st_size);
    if (res == 0) {
        printf("lookup : trouvé\n");
        close(fd);
        return 0;
    }
    if (res == 1) {
        printf("lookup : non trouvé\n");
        close(fd);
        return 1;
    }

    fprintf(stderr, "Erreur de la commande lookup\n");
    close(fd);
    return 1;
}