/* Myriam Bensaid 22504229
Je déclare qu'il s'agit de mon propre travail.
Ce travail a été réalisé intégralement par un être humain. */

#ifndef USER_H
#define USER_H

#include <sys/socket.h>
#include <netinet/in.h>

struct user {
    struct sockaddr *address;
    socklen_t addr_len;
    int sock;
    char nickname[17]; /* 16 caractères max + le caractère de fin de chaîne '\0' */
};

struct user *user_accept(int sl);
void user_free(void *usr);

#endif /* USER_H */
