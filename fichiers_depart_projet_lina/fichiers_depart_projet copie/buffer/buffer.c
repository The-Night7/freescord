#include "buffer.h"
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

struct buffer {
	int fd;             /* Le descripteur de fichier/socket associé */
	char *data;         /* Le tableau alloué dynamiquement qui sert de tampon */
	size_t size;        /* La taille totale du tampon */
	size_t pos;         /* L'indice du prochain caractère à lire dans le tampon */
	size_t len;         /* Le nombre d'octets valides actuellement dans le tampon */
	int is_eof;         /* Indicateur de fin de fichier / déconnexion */
	int unget_char;     /* Stockage d'un caractère "remis" dans le tampon */
	int has_unget;      /* Vrai (1) si unget_char contient un caractère valide, faux (0) sinon */
};

buffer *buff_create(int fd, size_t buffsz)
{
	buffer *b = malloc(sizeof(struct buffer));
	if (b == NULL) return NULL;
	b->fd = fd;
	b->size = buffsz;
	b->data = malloc(buffsz); /* Allocation du tableau de la taille demandée */
	b->pos = 0;
	b->len = 0;
	b->is_eof = 0;
	b->has_unget = 0;
	return b;
}

int buff_getc(buffer *b)
{
	/* 1. Si un caractère a été remis, on le renvoie en priorité */
	if (b->has_unget) {
		b->has_unget = 0;
		return b->unget_char;
	}
	/* 2. Si on a consommé tout le buffer, on demande au système de le reremplir */
	if (b->pos >= b->len) {
		ssize_t n = read(b->fd, b->data, b->size);
		if (n < 0) {
			return EOF; /* Erreur de lecture */
		} else if (n == 0) {
			b->is_eof = 1; /* Fin de fichier / déconnexion */
			return EOF;
		}
		b->pos = 0;
		b->len = n;
	}
	/* 3. On retourne le caractère courant et on avance la position */
	return (unsigned char)b->data[b->pos++];
}

int buff_ungetc(buffer *b, int c)
{
	/* On simule la remise d'un caractère (souvent utilisé si on lit un caractère de trop) */
	b->unget_char = c;
	b->has_unget = 1;
	return c;
}

void buff_free(buffer *b)
{
	if (b != NULL) {
		free(b->data);
		free(b);
	}
}

int buff_eof(const buffer *buff)
{
	return buff->is_eof;
}

int buff_ready(const buffer *buff)
{
	/* Prêt si un unget_char est dispo OU s'il reste des caractères non lus dans le tableau */
	return buff->has_unget || (buff->pos < buff->len);
}

/* --- Fonctions de lecture par lignes --- */

char *buff_fgets(buffer *b, char *dest, size_t size)
{
	size_t i = 0;
	while (i < size - 1) {
		int c = buff_getc(b);
		if (c == EOF) break;
		dest[i++] = c;
		if (c == '\n') break; /* On s'arrête à la fin de la ligne Unix */
	}
	if (i == 0) return NULL; /* Rien n'a été lu */
	dest[i] = '\0'; /* Terminaison de la chaîne C */
	return dest;
}

char *buff_fgets_crlf(buffer *b, char *dest, size_t size)
{
	size_t i = 0;
	while (i < size - 1) {
		int c = buff_getc(b);
		if (c == EOF) break;
		dest[i++] = c;
		/* Vérification de la séquence Windows/Réseau \r\n */
		if (c == '\r') {
			int next_c = buff_getc(b);
			if (next_c == '\n') {
				if (i < size - 1) dest[i++] = '\n';
				break;
			} else if (next_c != EOF) {
				buff_ungetc(b, next_c); /* Fausse alerte, on remet le caractère suivant */
			}
		}
	}
	if (i == 0) return NULL;
	dest[i] = '\0';
	return dest;
}