#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "user.h"
#include "list/list.h" /* Ajout de la bibliothèque de listes */

#define PORT_FREESCORD 4321

/* Variables globales pour le broadcast */
int tube[2];                     /* Le tube de communication inter-threads */
struct list *clts_connecte;      /* La liste des clients actuellement connectés */
pthread_mutex_t verrou_liste;    /* Mutex pour protéger l'accès à la liste */

int create_listening_sock(uint16_t port) {
    /* [Code inchangé de l'exercice 2] */
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

void *handle_client(void *usr) {
    struct user *client = (struct user *)usr;
    char buf[512];
    ssize_t lus;

    printf("[Thread] Client connecté (Socket: %d)\n", client->sock);

    /* d) Au lieu de renvoyer au client, on écrit dans l'entrée du tube (tube[1]) */
    while ((lus = read(client->sock, buf, sizeof(buf))) > 0) {
        if (write(tube[1], buf, lus) != lus) {
            perror("Erreur d'écriture dans le tube");
            break;
        }
    }

    /* Gestion de la déconnexion */
    pthread_mutex_lock(&verrou_liste);
    list_remove_element(clts_connecte, client); /* On retire le client de la liste */
    pthread_mutex_unlock(&verrou_liste);

    printf("[Thread] Client déconnecté (Socket: %d)\n", client->sock);
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

        /* Ajout du client à la liste (toujours protéger avec le mutex) */
        pthread_mutex_lock(&verrou_liste);
        list_add(clts_connecte, client);
        pthread_mutex_unlock(&verrou_liste);

        pthread_t th;
        if (pthread_create(&th, NULL, handle_client, client) != 0) {
            perror("pthread_create client");
            /* Nettoyage si échec de création du thread */
            pthread_mutex_lock(&verrou_liste);
            list_remove_element(clts_connecte, client);
            pthread_mutex_unlock(&verrou_liste);
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
