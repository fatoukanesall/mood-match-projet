#/*
 * MoodMatch — assistée par Fatou Sall
 * "Une étudiante qui avait besoin de ça. Alors elle l'a construit."
 *                                        — Fatoumata Sall
 */

#include "server.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define DEFAULT_PORT 8080
#define DEFAULT_DB_PATH "./moodmatch.db"

static volatile sig_atomic_t g_keep_running = 1;

static void handle_signal(int sig) {
    (void)sig;
    g_keep_running = 0;
}

static unsigned short lire_port_env(const char *port_str) {
    char *endptr = NULL;
    long value;

    if (!port_str || port_str[0] == '\0') {
        return (unsigned short)DEFAULT_PORT;
    }

    value = strtol(port_str, &endptr, 10);
    if (endptr == port_str || *endptr != '\0' || value <= 0 || value > 65535) {
        fprintf(stderr, "[main] PORT invalide (%s), fallback=%d\n", port_str, DEFAULT_PORT);
        return (unsigned short)DEFAULT_PORT;
    }

    return (unsigned short)value;
}

int main(void) {
    const char *groq_api_key = getenv("GROQ_API_KEY");
    const char *db_path = getenv("DB_PATH");
    const char *port_env = getenv("PORT");
    unsigned short port;

    if (!groq_api_key || groq_api_key[0] == '\0') {
        fprintf(stderr, "[main] Variable d'environnement GROQ_API_KEY manquante.\n");
        fprintf(stderr, "[main] Lance run.bat - il te demandera ta cle et la sauvegarde dans config.bat.\n");
        return 1;
    }

    if (!db_path || db_path[0] == '\0') {
        db_path = DEFAULT_DB_PATH;
    }

    port = lire_port_env(port_env);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (demarrer_serveur(port, db_path) != 0) {
        fprintf(stderr, "[main] Echec demarrage serveur sur le port %hu\n", port);
        return 1;
    }

    printf("[main] MoodMatch backend actif\n");
    printf("[main] URL: http://localhost:%hu\n", port);
    printf("[main] DB: %s\n", db_path);
    printf("[main] Appuyer sur CTRL+C pour arreter.\n");

    while (g_keep_running) {
#ifdef _WIN32
        Sleep(200);
#else
        sleep(1);
#endif
    }

    arreter_serveur();
    printf("[main] Serveur arrete proprement.\n");
    return 0;
}
