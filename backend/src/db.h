#ifndef DB_H
#define DB_H

#include "models.h"

int init_db(const char *db_path);

int creer_utilisateur(const char *db_path, const char *pseudo);

int inserer_recommandation(const char *db_path, const Recommandation *rec);
int lister_historique(const char *db_path, const char *pseudo, RecommandationList *out_list);

int ajouter_favori(const char *db_path, const Recommandation *rec);
int supprimer_favori(const char *db_path, int favori_id, const char *pseudo);
int lister_favoris(const char *db_path, const char *pseudo, RecommandationList *out_list);

int cache_get(const char *db_path, const char *humeur, const char *categories, const char *mode, char **out_json);
int cache_set(const char *db_path, const char *humeur, const char *categories, const char *mode, const char *json);

#endif
