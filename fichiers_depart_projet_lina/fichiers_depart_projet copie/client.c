/* Myriam Bensaid 22504229
Je déclare qu'il s'agit de mon propre travail.
Ce travail a été réalisé intégralement par un être humain. */

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT_FREESCORD 4321

/** * Se connecter au serveur TCP d'adresse donnée en argument et au port donné
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
        fprintf(stderr, "Adresse IP invalide : %s\n", adresse);
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
    if (sc < 0) {
        fprintf(stderr, "Impossible de se connecter au serveur %s:%d\n", adresse_serveur, PORT_FREESCORD);
        return 1;
    }

    printf("Connecté au serveur. Entrez votre texte (Ctrl-D pour quitter) :\n");

    char buf[512];
    ssize_t lus;

    /* 5. Boucle principale du client */
    /* On lit sur l'entrée standard (descripteur 0) */
    while ((lus = read(0, buf, sizeof(buf))) > 0) {
        
        /* Envoyer cette ligne au serveur */
        if (write(sc, buf, lus) != lus) {
            perror("Erreur d'envoi au serveur");
            break;
        }

        /* Lire la réponse du serveur (qui devrait être la même chose) */
        ssize_t reponse = read(sc, buf, sizeof(buf));
        if (reponse > 0) {
            /* La recopier sur le terminal (descripteur 1) */
            write(1, buf, reponse);
        } else if (reponse == 0) {
            printf("\nLe serveur a fermé la connexion de son côté.\n");
            break;
        } else {
            perror("Erreur de lecture depuis le serveur");
            break;
        }
    }

    /* 6. Lorsque l'utilisateur met fin à l'entrée avec Ctrl-D (lus == 0), 
       le client sort de la boucle, ferme sa socket et prend fin */
    if (lus == 0) {
         printf("\nDéconnexion...\n");
    }

    close(sc);
    return 0;
}