/* Myriam Bensaid 22504229
Je déclare qu'il s'agit de mon propre travail.
Ce travail a été réalisé intégralement par un être humain. */

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>         /* Pour getaddrinfo */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include "buffer/buffer.h" /* Inclusion de notre bibliothèque buffer */
#include "utils.h"         /* Inclusion de crlf_to_lf et lf_to_crlf */

#define PORT_FREESCORD "4321"

/**
 * Se connecter au serveur (compatible IPv4, IPv6 et Noms de Domaine)
 */
int connect_serveur_tcp(char *adresse, const char *port)
{
    struct addrinfo hints, *res, *p;
    int fd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;     /* IPv4 ou IPv6 */
    hints.ai_socktype = SOCK_STREAM;   /* TCP */

    /* La magie de getaddrinfo : elle résout les IP et les noms de domaine (ex: google.com, localhost) */
    if (getaddrinfo(adresse, port, &hints, &res) != 0) {
        fprintf(stderr, "Adresse IP ou nom de domaine invalide\n");
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) {
            break; /* Connexion réussie ! */
        }
        close(fd);
    }

    freeaddrinfo(res);

    if (p == NULL) {
        perror("connect");
        return -1;
    }

    return fd;
}

int main(int argc, char *argv[])
{
    int sc;

    if (argc < 2) {
        sc = connect_serveur_tcp("localhost", PORT_FREESCORD); /* Support de 'localhost' ! */
    } else {
        sc = connect_serveur_tcp(argv[1], PORT_FREESCORD);
    }

    if (sc < 0) {
        fprintf(stderr, "Erreur connect_serveur_tcp\n");
        return 1;
    }

    struct pollfd fds[2] = {
        { .fd = 0,  .events = POLLIN },  /* Entrée standard (clavier) */
        { .fd = sc, .events = POLLIN }   /* Socket du serveur */
    };

    char buf[512]; /* Max 512 octets par message selon freescord */

    /* 1. Création des tampons de 1024 octets pour le clavier et pour la socket */
    buffer *b_in   = buff_create(0,  1024);
    buffer *b_sock = buff_create(sc, 1024);

    printf("Connecté au serveur. (Ctrl-D pour quitter)\n");

    for (;;) {
        /* 2. Logique de poll modifiée :
           Si l'un des buffers a des données prêtes, timeout = 0 (ne bloque pas).
           Sinon, timeout = -1 (bloque indéfiniment jusqu'à l'arrivée de données). */
        int timeout = (buff_ready(b_in) || buff_ready(b_sock)) ? 0 : -1;

        int pll = poll(fds, 2, timeout);
        if (pll < 0) {
            perror("poll");
            break;
        }

        /* 3. Traitement de l'entrée standard (le clavier) */
        if ((fds[0].revents & POLLIN) || buff_ready(b_in)) {
            /* On lit une ligne Unix (terminée par \n) */
            if (buff_fgets(b_in, buf, sizeof(buf))) {
                /* On convertit \n en \r\n avant de l'envoyer au serveur ! */
                lf_to_crlf(buf);
                /* On envoie la ligne au serveur (strlen vérifie la vraie taille de la chaîne) */
                if (write(sc, buf, strlen(buf)) < 0) {
                    perror("write serveur");
                    break;
                }
            } else if (buff_eof(b_in)) {
                /* Utilisateur a fait Ctrl-D */
                printf("\nDéconnexion...\n");
                break;
            }
        }

        /* 4. Traitement de la socket (messages entrants du serveur) */
        if ((fds[1].revents & POLLIN) || buff_ready(b_sock)) {
            /* On lit une ligne réseau (terminée par \r\n) */
            if (buff_fgets_crlf(b_sock, buf, sizeof(buf))) {
                /* On convertit \r\n en \n pour l'affichage correct dans le terminal */
                crlf_to_lf(buf);
                /* On affiche la ligne (pas de \n manuel car il est déjà dans buf) */
                printf("%s", buf);
                fflush(stdout); /* S'assure que le terminal affiche le texte de suite */
            } else if (buff_eof(b_sock)) {
                printf("\nLe serveur s'est déconnecté\n");
                break;
            }
        }

        /* 5. Gestion des erreurs de socket (fermeture brutale, etc.) */
        if (fds[1].revents & (POLLERR | POLLHUP)) {
            printf("\nErreur ou déconnexion sur la socket\n");
            break;
        }
    }

    /* Nettoyage propre */
    buff_free(b_in);
    buff_free(b_sock);
    close(sc);
    return 0;
}