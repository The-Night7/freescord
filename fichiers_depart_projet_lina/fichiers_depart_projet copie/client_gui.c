#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>

#define PORT_FREESCORD "4321"
#define MAX_MESSAGES 15
#define MAX_LEN 512

/* --- PARTIE RÉSEAU (Similaire à ton client.c) --- */
int connect_serveur(char *adresse, const char* port) {
    struct addrinfo hints, *res, *p;
    int fd;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(adresse, port, &hints, &res) != 0) return -1;
    for (p = res; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        close(fd);
    }
    freeaddrinfo(res);
    return (p == NULL) ? -1 : fd;
}

/* --- PARTIE GRAPHIQUE --- */
char history[MAX_MESSAGES][MAX_LEN];
int history_count = 0;

void add_to_history(const char* msg) {
    if (history_count < MAX_MESSAGES) {
        strncpy(history[history_count], msg, MAX_LEN - 1);
        history_count++;
    } else {
        /* Décaler l'historique vers le haut */
        for (int i = 1; i < MAX_MESSAGES; i++) {
            strcpy(history[i-1], history[i]);
        }
        strncpy(history[MAX_MESSAGES-1], msg, MAX_LEN - 1);
    }
}

void render_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y, SDL_Color color) {
    if (strlen(text) == 0) return;
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect destRect = { x, y, surface->w, surface->h };
    SDL_RenderCopy(renderer, texture, NULL, &destRect);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

int main(int argc, char *argv[]) {
    char *adresse = (argc < 2) ? "127.0.0.1" : argv[1];
    int sock = connect_serveur(adresse, PORT_FREESCORD);
    if (sock < 0) {
        printf("Erreur de connexion au serveur.\n");
        return 1;
    }

    /* Initialisation SDL */
    if (SDL_Init(SDL_INIT_VIDEO) < 0 || TTF_Init() == -1) {
        printf("Erreur SDL/TTF: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Freescord GUI", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    
    /* ATTENTION : Assure-toi d'avoir un fichier arial.ttf dans le même dossier ! */
    TTF_Font *font = TTF_OpenFont("arial.ttf", 20);
    if (!font) {
        printf("Erreur police (télécharge arial.ttf et place-le ici): %s\n", TTF_GetError());
        return 1;
    }

    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Color inputColor = {200, 200, 255, 255};

    char input_text[MAX_LEN] = "";
    int running = 1;
    SDL_Event e;

    struct pollfd fds[1] = {{ .fd = sock, .events = POLLIN }};
    SDL_StartTextInput();

    while (running) {
        /* 1. GESTION DES ÉVÉNEMENTS GRAPHIQUES (Clavier, Fenêtre) */
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            } 
            else if (e.type == SDL_TEXTINPUT) {
                if (strlen(input_text) + strlen(e.text.text) < MAX_LEN - 1) {
                    strcat(input_text, e.text.text);
                }
            } 
            else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_BACKSPACE && strlen(input_text) > 0) {
                    input_text[strlen(input_text) - 1] = '\0'; /* Suppression basique d'un char */
                } 
                else if (e.key.keysym.sym == SDLK_RETURN && strlen(input_text) > 0) {
                    /* Envoi au serveur (on ajoute le \r\n manuellement pour le réseau) */
                    char out_buf[MAX_LEN + 2];
                    snprintf(out_buf, sizeof(out_buf), "%s\r\n", input_text);
                    write(sock, out_buf, strlen(out_buf));
                    
                    input_text[0] = '\0'; /* On vide le champ */
                }
            }
        }

        /* 2. GESTION DU RÉSEAU (Non-bloquant) */
        int pll = poll(fds, 1, 0); /* Timeout de 0 pour ne pas figer l'image */
        if (pll > 0 && (fds[0].revents & POLLIN)) {
            char net_buf[MAX_LEN];
            ssize_t lus = read(sock, net_buf, sizeof(net_buf) - 1);
            if (lus > 0) {
                net_buf[lus] = '\0';
                /* Nettoyage du \r\n avant affichage */
                char *pos = strstr(net_buf, "\r\n");
                if (pos) *pos = '\0';
                add_to_history(net_buf);
            } else if (lus == 0) {
                add_to_history("[SERVEUR DECONNECTE]");
                running = 0;
            }
        }

        /* 3. DESSIN DE L'INTERFACE */
        SDL_SetRenderDrawColor(renderer, 44, 47, 51, 255); /* Gris foncé type Discord */
        SDL_RenderClear(renderer);

        /* Dessiner l'historique */
        int start_y = 20;
        for (int i = 0; i < history_count; i++) {
            render_text(renderer, font, history[i], 20, start_y + (i * 30), textColor);
        }

        /* Dessiner la zone de saisie (Ligne de séparation + Texte) */
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_RenderDrawLine(renderer, 20, 530, 780, 530);
        
        char display_input[MAX_LEN + 2];
        snprintf(display_input, sizeof(display_input), "> %s_", input_text);
        render_text(renderer, font, display_input, 20, 550, inputColor);

        SDL_RenderPresent(renderer);
    }

    SDL_StopTextInput();
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    close(sock);

    return 0;
}