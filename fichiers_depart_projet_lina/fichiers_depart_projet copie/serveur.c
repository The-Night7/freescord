#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "user.h"
#include "utils.h"
#include "list/list.h"
#include "buffer/buffer.h" /* Ajout du buffer de l'exercice 5 */

#define PORT_FREESCORD 4321

/* Variables globales pour le broadcast */
int tube[2];                     /* Le tube de communication inter-threads */
struct list *clts_connecte;      /* La liste des clients actuellement connectés */
pthread_mutex_t verrou_liste;    /* Mutex pour protéger l'accès à la liste */

int create_listening_sock(uint16_t port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); return -1; }

    struct sockaddr_in sa = { .sin_family = AF_INET, .sin_port = htons(port), .sin_addr.s_addr = htonl(INADDR_ANY) };

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));

    if (bind(sockfd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("bind"); close(sockfd); return -1;
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
    struct list *l = (struct list*) lst;
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
 * Thread client : gère l'authentification puis le chat
 * C'est lui qui ajoute le client à la liste (après validation du pseudo)
 */
void *handle_client(void *usr) {
    struct user *client = (struct user *)usr;
    char buf[512];

    /* On utilise notre super buffer créé à l'exercice 5 ! */
    buffer *b = buff_create(client->sock, 1024);

    /* 1. Message de bienvenue du protocole */
    char *welcome = "Bienvenue sur le serveur Freescord !\r\n"
                    "Entrez votre pseudonyme avec 'nickname <votre_pseudo>'\r\n\r\n";
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

    /* 3. Phase de messagerie normale (Chat room) */
    if (nick_ok) {
        printf("Nouvel utilisateur en ligne : %s\n", client->nickname);
        char message_diffuse[600];

        /* On lit les messages normaux du client */
        while (buff_fgets_crlf(b, buf, sizeof(buf))) {
            /* On prépare la ligne préfixée : "pseudo> message" */
            snprintf(message_diffuse, sizeof(message_diffuse), "%s> %s", client->nickname, buf);
            /* Envoi au répéteur (qui diffusera à tout le monde) */
            write(tube[1], message_diffuse, strlen(message_diffuse));
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

    printf("Serveur de Chat en écoute sur le port %d...\n", PORT_FREESCORD);

    for (;;) {
        struct user *client = user_accept(listensc);
        if (client == NULL) continue;

        /* Plus de list_add ici : c'est handle_client qui s'en chargera
           après validation du pseudo ! */
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