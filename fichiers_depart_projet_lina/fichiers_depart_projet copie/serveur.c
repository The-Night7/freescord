#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>          /* Remplaçant de arpa/inet.h pour getaddrinfo */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "user.h"
#include "utils.h"
#include "list/list.h"
#include "buffer/buffer.h"

#define PORT_FREESCORD "4321" /* Changé en chaîne de caractères pour getaddrinfo */

/* Variables globales pour le broadcast */
int tube[2];                    /* Le tube de communication inter-threads */
struct list *clts_connecte;     /* La liste des clients actuellement connectés */
pthread_mutex_t verrou_liste;   /* Mutex pour protéger l'accès à la liste */

/**
 * Créer et configurer une socket d'écoute compatible IPv4 et IPv6
 */
int create_listening_sock(const char *port) {
    struct addrinfo hints, *res, *p;
    int sockfd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;     /* Accepte IPv4 ou IPv6 */
    hints.ai_socktype = SOCK_STREAM;   /* TCP */
    hints.ai_flags    = AI_PASSIVE;    /* Utiliser l'IP de la machine locale */

    /* Résolution de l'adresse */
    if (getaddrinfo(NULL, port, &hints, &res) != 0) {
        perror("getaddrinfo"); return -1;
    }

    /* On parcourt les résultats jusqu'à réussir à "bind" */
    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0) continue;

        int opt = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
            break; /* Succès ! */
        }
        close(sockfd);
    }

    freeaddrinfo(res); /* Libération de la mémoire allouée par getaddrinfo */

    if (p == NULL) {
        fprintf(stderr, "Échec du bind sur toutes les adresses\n"); return -1;
    }

    if (listen(sockfd, 128) < 0) {
        perror("listen"); close(sockfd); return -1;
    }

    return sockfd;
}

/**
 * Thread Répéteur : Lit dans le tube et diffuse à tous les clients
 */
void *repeteur(void *lst) {
    struct list *l = (struct list *)lst;
    char buf[512];
    ssize_t n;

    /* Lecture bloquante sur la sortie du tube (tube[0]) */
    while ((n = read(tube[0], buf, sizeof(buf))) > 0) {
        /* Verrouiller la liste pendant qu'on la parcourt pour éviter qu'un
           client ne se déconnecte/connecte en plein milieu de la boucle */
        pthread_mutex_lock(&verrou_liste);
        for (size_t i = 0; i < list_length(l); i++) {
            struct user *u = (struct user *)list_get(l, i);
            /* Envoi du message à la socket de l'utilisateur courant */
            write(u->sock, buf, n);
        }
        pthread_mutex_unlock(&verrou_liste);
    }

    return NULL;
}

/**
 * Thread client : gère l'authentification puis le chat avec système de commandes
 */
void *handle_client(void *usr) {
    struct user *client = (struct user *)usr;
    char buf[512];

    buffer *b = buff_create(client->sock, 1024);

    /* 1. Message de bienvenue du protocole */
    char *welcome = "Bienvenue ! Commandes: 'nickname <pseudo>', 'msg <texte>', 'list', 'whisper <pseudo> <texte>'\r\n\r\n";
    write(client->sock, welcome, strlen(welcome));

    int nick_ok = 0;

    /* 2. Boucle d'authentification */
    while (!nick_ok && buff_fgets_crlf(b, buf, sizeof(buf))) {
        /* Nettoyage du CRLF pour faciliter les comparaisons */
        crlf_to_lf(buf);
        char *newline = strchr(buf, '\n');
        if (newline) *newline = '\0';

        /* Code 3 : Vérifie si la commande commence bien par "nickname " */
        if (strncmp(buf, "nickname ", 9) != 0) {
            write(client->sock, "3 \r\n", 4);
            continue;
        }

        char *pseudo = buf + 9;

        /* Code 2 : Vérifie les règles (max 16 chars, pas d'espace) */
        if (strlen(pseudo) > 16 || strchr(pseudo, ' ') != NULL) {
            write(client->sock, "2 \r\n", 4);
            continue;
        }

        /* Code 1 : Vérifie si le nom est déjà pris (protection avec mutex) */
        pthread_mutex_lock(&verrou_liste);
        int pris = 0;
        for (size_t i = 0; i < list_length(clts_connecte); i++) {
            struct user *u = (struct user *)list_get(clts_connecte, i);
            if (strcmp(u->nickname, pseudo) == 0) {
                pris = 1;
                break;
            }
        }

        if (pris) {
            pthread_mutex_unlock(&verrou_liste);
            write(client->sock, "1 \r\n", 4);
        } else {
            /* Code 0 : Succès ! On enregistre le pseudo et on ajoute à la liste */
            strcpy(client->nickname, pseudo);
            list_add(clts_connecte, client);
            pthread_mutex_unlock(&verrou_liste);
            write(client->sock, "0 \r\n", 4);
            nick_ok = 1; /* Sortie de la boucle d'authentification */
        }
    }

    /* 3. Phase de discussion avec le système de commandes */
    if (nick_ok) {
        printf("Connexion : %s\n", client->nickname);
        char message_diffuse[600];

        while (buff_fgets_crlf(b, buf, sizeof(buf))) {
            crlf_to_lf(buf);
            char *newline = strchr(buf, '\n');
            if (newline) *newline = '\0';

            /* Commande : list (afficher les utilisateurs connectés) */
            if (strcmp(buf, "list") == 0) {
                char rep[1024] = "list ";
                pthread_mutex_lock(&verrou_liste);
                for (size_t i = 0; i < list_length(clts_connecte); i++) {
                    strcat(rep, ((struct user *)list_get(clts_connecte, i))->nickname);
                    strcat(rep, " ");
                }
                pthread_mutex_unlock(&verrou_liste);
                strcat(rep, "\r\n");
                write(client->sock, rep, strlen(rep));
            }
            /* Commande : msg (message public diffusé à tous) */
            else if (strncmp(buf, "msg ", 4) == 0) {
                snprintf(message_diffuse, sizeof(message_diffuse),
                         "msg %s> %s\r\n", client->nickname, buf + 4);
                write(tube[1], message_diffuse, strlen(message_diffuse));
            }
            /* Commande : whisper (message privé) */
            else if (strncmp(buf, "whisper ", 8) == 0) {
                char *target = buf + 8;
                char *space = strchr(target, ' ');
                if (space) {
                    *space = '\0'; /* Sépare le pseudo du reste du message */
                    char *priv_msg = space + 1;
                    char message_prive[600];
                    snprintf(message_prive, sizeof(message_prive),
                             "whisper %s> %s\r\n", client->nickname, priv_msg);
                    pthread_mutex_lock(&verrou_liste);
                    int found = 0;
                    for (size_t i = 0; i < list_length(clts_connecte); i++) {
                        struct user *u = (struct user *)list_get(clts_connecte, i);
                        if (strcmp(u->nickname, target) == 0) {
                            write(u->sock, message_prive, strlen(message_prive));
                            found = 1;
                            break;
                        }
                    }
                    pthread_mutex_unlock(&verrou_liste);
                    if (!found)
                        write(client->sock, "erreur Utilisateur introuvable\r\n", 32);
                } else {
                    write(client->sock, "erreur Format: whisper <pseudo> <message>\r\n", 43);
                }
            }
            /* Commande inconnue */
            else {
                write(client->sock, "erreur Commande inconnue (utilisez msg, list, whisper)\r\n", 56);
            }
        }

        /* Déconnexion : on retire l'utilisateur de la liste publique */
        pthread_mutex_lock(&verrou_liste);
        list_remove_element(clts_connecte, client);
        pthread_mutex_unlock(&verrou_liste);

        printf("Déconnexion : %s\n", client->nickname);
    }

    buff_free(b);
    close(client->sock);
    user_free(client);
    return NULL;
}

int main() {
    /* a) Création du tube */
    if (pipe(tube) < 0) {
        perror("pipe");
        return 1;
    }

    /* b) Initialisation de la liste et du mutex */
    clts_connecte = list_create();
    pthread_mutex_init(&verrou_liste, NULL);

    int listensc = create_listening_sock(PORT_FREESCORD);
    if (listensc < 0) { return 1; }

    /* c) Lancement du thread répéteur au démarrage */
    pthread_t th_rep;
    if (pthread_create(&th_rep, NULL, repeteur, clts_connecte) != 0) {
        perror("pthread_create repeteur");
        return 1;
    }
    pthread_detach(th_rep);

    printf("Serveur de Chat en écoute sur le port %s...\n", PORT_FREESCORD);

    for (;;) {
        struct user *client = user_accept(listensc);
        if (client == NULL) continue;

        pthread_t th;
        if (pthread_create(&th, NULL, handle_client, client) != 0) {
            perror("pthread_create client");
            close(client->sock);
            user_free(client);
            continue;
        }
        pthread_detach(th);
    }

    pthread_mutex_destroy(&verrou_liste);
    close(listensc);
    return 0;
}