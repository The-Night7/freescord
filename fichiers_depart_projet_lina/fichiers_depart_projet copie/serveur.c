#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "user.h"

#define PORT_FREESCORD 4321

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

/** * Gérer toutes les communications avec le client renseigné dans user 
 */
void *handle_client(void *usr) {
    /* Le thread récupère l'adresse de la structure utilisateur passée en argument */
    struct user *client = (struct user *)usr;
    char buf[512];
    ssize_t lus;

    printf("[Thread] Prise en charge du client (Socket: %d)\n", client->sock);

    /* Boucle d'écho : on lit et on renvoie au même client */
    while ((lus = read(client->sock, buf, sizeof(buf))) > 0) {
        if (write(client->sock, buf, lus) != lus) {
            perror("write vers client");
            break;
        }
    }

    if (lus == 0) {
        printf("[Thread] Le client (Socket: %d) s'est déconnecté.\n", client->sock);
    } else {
        perror("read depuis client");
    }

    /* Le thread a terminé son travail : on ferme la socket et on libère la mémoire */
    close(client->sock);
    user_free(client);
    return NULL;
}

int main() {
    int listensc = create_listening_sock(PORT_FREESCORD);
    if (listensc < 0) {
        fprintf(stderr, "Erreur create_listening_sock\n");
        return 1;
    }

    printf("Serveur multithread en écoute sur le port %d...\n", PORT_FREESCORD);

    for (;;) {
        /* a) Accepter un client avec notre nouvelle fonction encapsulée */
        struct user *client = user_accept(listensc);
        if (client == NULL) {
            continue; /* Si erreur, on passe au suivant */
        }

        /* b) Création du thread pour gérer ce client spécifiquement */
        pthread_t th;
        if (pthread_create(&th, NULL, handle_client, client) != 0) {
            perror("pthread_create");
            close(client->sock);
            user_free(client);
            continue;
        }

        /* c) Détacher le thread pour que ses ressources soient libérées automatiquement à sa fin */
        pthread_detach(th);
    }

    close(listensc);
    return 0;
}
