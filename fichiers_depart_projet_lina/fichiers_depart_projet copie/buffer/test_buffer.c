/* Myriam Bensaid 22504229
Je déclare qu'il s'agit de mon propre travail.
Ce travail a été réalisé intégralement par un être humain. */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "buffer.h"

int main() {
    printf("=== Test de la bibliothèque Buffer ===\n");
    printf("Tapez des phrases. Tapez 'quit' pour arrêter ou faites Ctrl-D.\n");
    printf("------------------------------------------------------------\n");

    /* * On crée un buffer sur l'entrée standard (descripteur 0).
     * On met volontairement une toute petite taille (5 octets) pour 
     * forcer le buffer à se recharger plusieurs fois pendant la frappe 
     * d'une longue phrase, afin de bien tester notre logique.
     */
    buffer *b = buff_create(0, 5);
    if (b == NULL) {
        fprintf(stderr, "Erreur lors de la création du buffer.\n");
        return 1;
    }

    char ligne[256];

    /* On utilise buff_fgets pour lire ligne par ligne jusqu'à la fin (Ctrl-D) */
    while (buff_fgets(b, ligne, sizeof(ligne)) != NULL) {
        
        printf("[Test] Ligne lue : %s", ligne); 
        /* Note: buff_fgets inclut le '\n' à la fin de la chaîne lue */

        /* Condition de sortie propre si l'utilisateur tape "quit" */
        if (strncmp(ligne, "quit", 4) == 0) {
            printf("=> Commande 'quit' reconnue. Fin du test.\n");
            break;
        }
    }

    /* On vérifie si on est sorti de la boucle à cause d'un vrai EOF (Ctrl-D) */
    if (buff_eof(b)) {
        printf("\n=> Fin de fichier (EOF) atteinte (Ctrl-D).\n");
    }

    /* Nettoyage de la mémoire */
    buff_free(b);
    printf("=== Test terminé, mémoire libérée ===\n");

    return 0;
}