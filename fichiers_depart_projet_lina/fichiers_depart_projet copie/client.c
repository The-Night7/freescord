/* Myriam Bensaid 22504229
Je déclare qu'il s'agit de mon propre travail.
Ce travail a été réalisé intégralement par un être humain. */

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>

#define PORT_FREESCORD 4321

/**
 * Se connecter au serveur TCP d'adresse donnée en argument et au port donné
 * retourne le descripteur de fichier de la socket obtenue ou -1 en cas d'erreur.
 */
int connect_serveur_tcp(char *adresse, uint16_t port) {
    /* 1. Création de la socket */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    /* 2. Configuration de l'adresse de destination */
    struct sockaddr_in sa = {
        .sin_family = AF_INET,
        .sin_port = htons(port)
    };

    /* Conversion de l'adresse textuelle en format binaire réseau */
    if (inet_pton(AF_INET, adresse, &sa.sin_addr) != 1) {
        fprintf(stderr, "Adresse invalide\n");
        close(fd);
        return -1;
    }

    /* 3. Demande de connexion au serveur */
    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }

    return fd;
}

int main(int argc, char *argv[]) {
    int sc;
    char *adresse_serveur = (argc < 2) ? "127.0.0.1" : argv[1];

    /* 4. Se connecter au serveur dont l'adresse IP est donnée comme argument */
    sc = connect_serveur_tcp(adresse_serveur, PORT_FREESCORD);
    if (sc < 0)
        return 1;

    printf("Connecté au chat ! Entrez vos messages (Ctrl-D pour quitter) :\n");

    /* a) Préparation du tableau pollfd */
    /* fds[0] surveille l'entrée standard (clavier) */
    /* fds[1] surveille la socket connectée au serveur */
    struct pollfd fds[2] = {
        { .fd = 0,  .events = POLLIN },
        { .fd = sc, .events = POLLIN }
    };

    char buf[512];
    ssize_t lus;

    for (;;) {
        /* Bloque jusqu'à ce qu'au moins un descripteur soit prêt à être lu */
        int pll = poll(fds, 2, -1);
        if (pll < 0) {
            perror("poll");
            break;
        }

        /* b) Vérification de l'entrée standard (clavier) */
        if (fds[0].revents & POLLIN) {
            lus = read(0, buf, sizeof(buf));
            if (lus > 0) {
                if (write(sc, buf, lus) != lus) {
                    perror("write au serveur");
                    break;
                }
            } else if (lus == 0) {
                /* L'utilisateur a fait Ctrl-D */
                printf("\nDéconnexion...\n");
                break;
            }
        }

        /* c) Vérification de la socket du serveur (messages entrants) */
        if (fds[1].revents & POLLIN) {
            lus = read(sc, buf, sizeof(buf));
            if (lus > 0) {
                if (write(1, buf, lus) != lus) {
                    perror("write sur le terminal");
                    break;
                }
            } else if (lus == 0) {
                /* Le serveur a fermé la connexion */
                printf("\nLe serveur a fermé la connexion.\n");
                break;
            }
        }

        /* Gestion des erreurs de connexion inattendues */
        if (fds[1].revents & (POLLERR | POLLHUP)) {
            printf("\nErreur critique sur la socket.\n");
            break;
        }
    }

    close(sc);
    return 0;
}
