#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT_FREESCORD 4321

/** * Créer et configurer une socket d'écoute sur le port donné en argument
 * retourne le descripteur de cette socket, ou -1 en cas d'erreur 
 */
int create_listening_sock(uint16_t port) {
    /* 1. Création de la socket IPv4 (AF_INET) et TCP (SOCK_STREAM) */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    /* 2. Configuration de l'adresse d'écoute (toutes les interfaces locales, port défini) */
    struct sockaddr_in sa = { 
        .sin_family = AF_INET, 
        .sin_port = htons(port), 
        .sin_addr.s_addr = htonl(INADDR_ANY) 
    };

    /* Option pour éviter l'erreur "Address already in use" lors du redémarrage rapide du serveur */
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));

    /* 3. Attachement de la socket à l'adresse et au port configurés */
    if (bind(sockfd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("bind");
        close(sockfd);
        return -1;
    }

    /* 4. Mise de la socket en mode écoute passive (128 est la taille de la file d'attente) */
    if (listen(sockfd, 128) < 0) { 
        perror("listen");
        close(sockfd);
        return -1; 
    }
    
    return sockfd;
}

int main() {
    int listensc = create_listening_sock(PORT_FREESCORD);
    if (listensc < 0) {
        fprintf(stderr, "Erreur lors de la création de la socket d'écoute\n");
        return 1;
    }

    printf("Serveur en écoute sur le port %d...\n", PORT_FREESCORD);

    /* Boucle principale du serveur pour traiter les clients un par un */
    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        /* a) Accepter la demande de connexion d'un client */
        int client_sock = accept(listensc, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("accept");
            continue; /* En cas d'erreur, on passe à la tentative suivante */
        }
        
        printf("Un client s'est connecté.\n");

        char buf[512];
        ssize_t lus;

        /* b) Lire les octets envoyés par le client et les lui renvoyer */
        /* c) Répéter jusqu'à ce que le client ferme sa socket (read retourne 0) */
        while ((lus = read(client_sock, buf, sizeof(buf))) > 0) {
            /* On réécrit exactement le même nombre d'octets vers le client (Echo) */
            if (write(client_sock, buf, lus) != lus) {
                perror("write vers client");
                break;
            }
        }

        if (lus == 0) {
            printf("Le client a fermé la connexion.\n");
        } else if (lus < 0) {
            perror("read depuis client");
        }

        /* Fermeture de la socket associée au client courant */
        close(client_sock);
    }

    close(listensc);
    return 0;
}