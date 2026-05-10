#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list/list.h"
#include "user.h"

#define PORT_FREESCORD 4321
int tube[2];
struct list *clts_connecte;
pthread_mutex_t verrou_liste; /*Plusieurs pthreads modifient clts_connecte pour eviter les problemes je creer ce mutex*/


/** Gérer toutes les communications avec le client renseigné dans
 * user, qui doit être l'adresse d'une struct user */
void *handle_client(void *user);
/** Créer et configurer une socket d'écoute sur le port donné en argument
 * retourne le descripteur de cette socket, ou -1 en cas d'erreur */
int create_listening_sock(uint16_t port);

void *repeteur(void* l);

int main(int argc, char *argv[])
{
	if (pipe(tube) < 0) {
            perror("pipe"); exit(1);
        }
	
	int listensc=create_listening_sock(PORT_FREESCORD);
	if (listensc < 0) {
        fprintf(stderr, "Erreur create_listening_sock\n");
        return 1;
    }
	clts_connecte= list_create();

	pthread_mutex_init(&verrou_liste, NULL);
	
	pthread_t th1;
	if(pthread_create(&th1, NULL, repeteur, clts_connecte) != 0) {
			perror("pthread_create");
            list_free(clts_connecte, user_free);
        }
	pthread_detach(th1);

	for(;;){
		struct user *client = user_accept(listensc);
			if (client == NULL){
				continue;
		}

		pthread_t th;
		
		if(pthread_create(&th, NULL, handle_client, client) != 0) {
			perror("pthread_create");
            user_free(client);
            continue;
        }


		pthread_detach(th);
		pthread_mutex_lock(&verrou_liste);
		list_add(clts_connecte, client);
		pthread_mutex_unlock(&verrou_liste);
		printf("Un client s'est connecté \n");
		printf("Nombres de clients: %zu\n", list_length(clts_connecte));
		/*list_print(clts_connecte, print_user ); test pour verifier que le bon elements est ajouter puis retirer*/
		
	}
	pthread_mutex_destroy(&verrou_liste);
	close(listensc);
	return 0;
}
	

void *handle_client(void *clt)
{
	struct user *client= (struct user *)clt;
	char buf[256];
	ssize_t mess;

	while((mess=read(client->sock, buf, 256))>0){
		write(tube[1], buf, mess);
	}

	if (mess==0){
		pthread_mutex_lock(&verrou_liste);
		list_remove_element(clts_connecte, client);
		pthread_mutex_unlock(&verrou_liste);

		printf("Client deconnecte\n");
		printf("Nombres de clients: %zu\n", list_length(clts_connecte));
		/*list_print(clts_connecte, print_user ); test pour verifier que le bon elements est ajouter puis retirer*/

	}
	else{
		printf("Read error\n");
	}
	user_free(client);
	return NULL;
}

int create_listening_sock(uint16_t port)
{
	int sockfd= socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd<0){
		perror("socket");
		exit(1);
	}

	struct sockaddr_in sa = { .sin_family = AF_INET, .sin_port = htons(port), .sin_addr.s_addr = htonl(INADDR_ANY) };
	int opt = 1;
	socklen_t sl = sizeof(sa);
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));

	if (bind(sockfd, (struct sockaddr *) &sa, sl) < 0) {
        perror("bind");
		close(sockfd);
		exit(3); 
	}

	if (listen(sockfd, 128) < 0) { 
		perror("listen");
		exit(2); }
	
	return sockfd;
}

void *repeteur(void *lst){
	struct list *l = (struct list*) lst;
	ssize_t n;
	char buf[256];
	while ((n = read(tube[0], buf, sizeof(buf))) > 0) {
		pthread_mutex_lock(&verrou_liste);/*Etre sur que la liste ne change pas tant que tout les clients n'ont pas recu le message precedant*/
		for(size_t i=0; i<list_length(l); i++ ){
			write(((struct user *)list_get(l, i))->sock, buf, n);
		}
		pthread_mutex_unlock(&verrou_liste);
	}
	return NULL;
}
