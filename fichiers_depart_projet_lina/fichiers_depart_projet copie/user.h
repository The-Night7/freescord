#ifndef USER_H
#define USER_H
#include <sys/socket.h>
#include <netinet/in.h>

struct user {
	struct sockaddr *address;
	socklen_t addr_len;
	int sock;
	/* autres champs éventuels */
};

struct user *user_accept(int sl);
void user_free(void *user);
void print_user(const void *usr);

#endif /* ifndef USER_H */
