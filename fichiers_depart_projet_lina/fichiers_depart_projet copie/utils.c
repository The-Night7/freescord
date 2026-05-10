/* Myriam Bensaid 22504229
Je déclare qu'il s'agit de mon propre travail.
Ce travail a été réalisé intégralement par un être humain. */

#include "utils.h"
#include <string.h>

/* Changer une ligne se terminant par CRLF (\r\n) en LF (\n) */
char *crlf_to_lf(char *line_with_crlf)
{
	/* On cherche la première occurrence de "\r\n" */
	char *pos = strstr(line_with_crlf, "\r\n");
	if (pos != NULL) {
		/* On remplace le \r par un \n */
		*pos = '\n';
		/* On remplace l'ancien \n par le caractère de fin de chaîne \0 */
		*(pos + 1) = '\0';
	}
	return line_with_crlf;
}

/* Changer une ligne se terminant par LF (\n) en CRLF (\r\n) */
char *lf_to_crlf(char *line_with_lf)
{
	/* On cherche la première occurrence de "\n" */
	char *pos = strchr(line_with_lf, '\n');
	if (pos != NULL) {
		/* On écrase le \n avec un \r, on met le \n juste après, puis on ferme la chaîne */
		*pos = '\r';
		*(pos + 1) = '\n';
		*(pos + 2) = '\0';
	}
	return line_with_lf;
}