#ifndef USER_H
#define USER_H

#include <sys/socket.h>
#include <netinet/in.h>

/* 1. Structure contenant la sockaddr, sa taille et le descripteur de la socket */
struct user {
    struct sockaddr *address;
    socklen_t addr_len;
    int sock;
};

/* Déclarations des fonctions de gestion de l'utilisateur */
struct user *user_accept(int sl);
void user_free(void *usr);

#endif /* USER_H */