#ifndef MODELS_H
#define MODELS_H

#include <stddef.h>

#define TYPE_MAX_LEN 16
#define TITRE_MAX_LEN 256
#define AUTEUR_MAX_LEN 256
#define EXPLICATION_MAX_LEN 1024
#define HUMEUR_MAX_LEN 256
#define TON_MAX_LEN 128
#define DUREE_PAGES_MAX_LEN 64
#define DATE_MAX_LEN 64

#define MAX_RECO 12

typedef struct {
    int id;
    char humeur_texte[HUMEUR_MAX_LEN];
    char type[TYPE_MAX_LEN];
    char titre[TITRE_MAX_LEN];
    char auteur_ou_realisateur[AUTEUR_MAX_LEN];
    int annee;
    char explication[EXPLICATION_MAX_LEN];
    char humeur_detectee[HUMEUR_MAX_LEN];
    char ton[TON_MAX_LEN];
    char duree_ou_pages[DUREE_PAGES_MAX_LEN];
    char date_creation[DATE_MAX_LEN];
    char pays[128];
    char genre[256];
    char sous_genre[128];
    char raison_precise[256];
    char fun_fact[512];
    char si_tu_aimes[256];
    char message_ami[512];
    char humeur_analysee[128];
    char niveau_intensite[32];
    char mode[32];
    char pseudo[64];
} Recommandation;

typedef struct {
    Recommandation *items;
    size_t count;
} RecommandationList;

typedef struct {
    Recommandation *films;
    int nb_films;
    Recommandation *livres;
    int nb_livres;
    Recommandation *series;
    int nb_series;
    Recommandation *animes;
    int nb_animes;
    char message_ami[512];
    char humeur_analysee[128];
} ResultatRecherche;

void free_recommandation_list(RecommandationList *list);
void init_resultat_recherche(ResultatRecherche *resultat);
void free_resultat_recherche(ResultatRecherche *resultat);

#endif
