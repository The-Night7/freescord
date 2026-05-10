#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include "buffer/buffer.h"
#include "utils.h"

#define PORT_FREESCORD 4321

/** se connecter au serveur TCP d'adresse donnée en argument sous forme de
 * chaîne de caractère et au port donné en argument
 * retourne le descripteur de fichier de la socket obtenue ou -1 en cas
 * d'erreur. */
int connect_serveur_tcp(char *adresse, uint16_t port);

int main(int argc, char *argv[])
{
	int sc;
	if (argc < 2) {
		sc=connect_serveur_tcp("127.0.0.1", PORT_FREESCORD);
	} 
	else {
		sc=connect_serveur_tcp(argv[1], PORT_FREESCORD);
	}
	
	if (sc < 0) {
		fprintf(stderr, "Erreur connect_serveur_tcp\n");
		return 1;
	}
	
	struct pollfd fds[2] = {{ .fd = 0, .events = POLLIN }, { .fd = sc, .events = POLLIN }}; 
	ssize_t mess;
	char buf[256];

	for(;;){

		int pll=poll(fds, 2, -1);
		if(pll<0){
			perror("pool");
			break;
		}

		if (fds[0].revents & (POLLIN| POLLHUP)) {
			mess = read(0, buf, sizeof(buf));
			if (mess > 0) {
				if (write(sc, buf, mess) != mess) {
					perror("write");
					break;
				}
			} else if (mess==0) {
					printf("Serveur deconnecté\n");
					break;
			}else{
				perror("read");
				break;
			}

			} 

		if (fds[1].revents & (POLLIN| POLLHUP)) {
			mess = read(sc, buf, sizeof(buf));
			if (mess > 0) {
				if (write(1, buf, mess) != mess) {
					perror("write");
					break;
				}
			} else if (mess==0) {
					printf("Serveur deconnecté\n");
					break;
			}else{
				perror("read");
				break;
			}

			}
			if (fds[1].revents & POLLERR) {
			printf("Erreur sur la socket\n");
			break;
		} 

	}
	close(sc);
	return 0;
}

int connect_serveur_tcp(char *adresse, uint16_t port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd<0){
		perror("socket");
		exit(1);
	}
	struct sockaddr_in sa = { .sin_family = AF_INET, .sin_port = htons(port) };
	if(inet_pton(AF_INET, adresse, &sa.sin_addr) != 1){
		perror("Adresse invalide");
		close(fd);
		exit(3);
	}
	socklen_t sl= sizeof(sa);
	if (connect(fd, (struct sockaddr *) &sa, sl) < 0) {
       perror("connect");
	   close(fd);
		exit(3);
	}

	return fd;
}
