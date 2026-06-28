#include "models.h"

#include <stdlib.h>

void free_recommandation_list(RecommandationList *list) {
    if (!list) {
        return;
    }

    free(list->items);
    list->items = NULL;
    list->count = 0;
}

void init_resultat_recherche(ResultatRecherche *resultat) {
    if (!resultat) {
        return;
    }

    resultat->films = NULL;
    resultat->nb_films = 0;
    resultat->livres = NULL;
    resultat->nb_livres = 0;
    resultat->series = NULL;
    resultat->nb_series = 0;
    resultat->animes = NULL;
    resultat->nb_animes = 0;
    resultat->message_ami[0] = '\0';
    resultat->humeur_analysee[0] = '\0';
}

void free_resultat_recherche(ResultatRecherche *resultat) {
    if (!resultat) {
        return;
    }

    free(resultat->films);
    free(resultat->livres);
    free(resultat->series);
    free(resultat->animes);
    init_resultat_recherche(resultat);
}
