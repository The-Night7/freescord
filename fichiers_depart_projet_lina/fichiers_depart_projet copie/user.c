#include "user.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>

/** accepter une connection TCP depuis la socket d'écoute sl et retourner un
 * pointeur vers un struct user, dynamiquement alloué et convenablement
 * initialisé */
struct user *user_accept(int sl)
{
	struct user *user= malloc(sizeof(struct user));
	if(user==NULL){
		perror("malloc user");
		exit(1);
	}
	user->address=malloc(sizeof(struct sockaddr ));
	if(user->address==NULL){
		perror("malloc address");
		exit(1);
	}

	user->addr_len = sizeof(struct sockaddr);

	user->sock=accept(sl, user->address, &user->addr_len);
	if((user->sock)<0){
		perror("accept");
		free(user->address);
		free(user);
		return NULL;
	}

	return user;
}

/** libérer toute la mémoire associée à user */
void user_free(void *usr)
{
	struct user *user= (struct user *)usr;
	if (user == NULL) return;
	free(user->address);
	free(user);
}

void print_user(const void *usr) {
    const struct user *u = (const struct user *)usr;
    printf("User: sock=%d\n", u->sock);
} /* fonction pour utiliser list_print et afficher les different sock des users*/