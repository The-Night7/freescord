/* Myriam Bensaid 22504229
Je déclare qu'il s'agit de mon propre travail.
Ce travail a été réalisé intégralement par un être humain. */

#include "user.h"
#include <stdlib.h>
#include <stdio.h>

/** * Accepter une connection TCP depuis la socket d'écoute sl et retourner un
 * pointeur vers un struct user, dynamiquement alloué et convenablement initialisé 
 */
struct user *user_accept(int sl) {
    /* Allocation dynamique de la structure */
    struct user *u = malloc(sizeof(struct user));
    if (u == NULL) {
        perror("malloc struct user");
        return NULL;
    }

    /* Allocation dynamique de la structure d'adresse (pour IPv4 ici) */
    u->address = malloc(sizeof(struct sockaddr_in));
    if (u->address == NULL) {
        perror("malloc sockaddr");
        free(u);
        return NULL;
    }
    
    u->addr_len = sizeof(struct sockaddr_in);

    /* Appel système accept pour bloquer jusqu'à la connexion d'un client */
    u->sock = accept(sl, u->address, &u->addr_len);
    if (u->sock < 0) {
        perror("accept");
        free(u->address);
        free(u);
        return NULL;
    }

    return u;
}

/** * Libérer toute la mémoire associée à user 
 */
void user_free(void *usr) {
    struct user *u = (struct user *)usr;
    if (u != NULL) {
        if (u->address != NULL) {
            free(u->address);
        }
        free(u);
    }
}