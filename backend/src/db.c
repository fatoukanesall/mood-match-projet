#include "db.h"

#include <libpq-fe.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static PGconn *open_pg(void) {
    const char *url = getenv("DATABASE_URL");
    if (!url || url[0] == '\0') {
        fprintf(stderr, "[db] DATABASE_URL non definie\n");
        return NULL;
    }
    PGconn *conn = PQconnectdb(url);
    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "[db] connexion echouee: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return NULL;
    }
    return conn;
}

static void copy_pg_text(char *dest, size_t dest_size, const char *src) {
    if (!dest || dest_size == 0) return;
    if (!src) { dest[0] = '\0'; return; }
    snprintf(dest, dest_size, "%s", src);
}

int init_db(const char *db_path) {
    (void)db_path;
    PGconn *conn = open_pg();
    if (!conn) return -1;
    PQfinish(conn);
    return 0;
}

int creer_utilisateur(const char *db_path, const char *pseudo) {
    (void)db_path;
    if (!pseudo || pseudo[0] == '\0') return -1;

    PGconn *conn = open_pg();
    if (!conn) return -1;

    const char *params[1] = { pseudo };
    PGresult *res = PQexecParams(conn,
        "INSERT INTO utilisateurs (pseudo, date_creation) VALUES ($1, NOW()::text)",
        1, NULL, params, NULL, NULL, 0);

    int rc;
    if (PQresultStatus(res) == PGRES_COMMAND_OK) {
        rc = 0;
    } else {
        const char *sqlstate = PQresultErrorField(res, PG_DIAG_SQLSTATE);
        if (sqlstate && strcmp(sqlstate, "23505") == 0) {
            rc = 1; /* pseudo deja pris */
        } else {
            fprintf(stderr, "[db] creer_utilisateur: %s\n", PQresultErrorMessage(res));
            rc = -1;
        }
    }

    PQclear(res);
    PQfinish(conn);
    return rc;
}

int inserer_recommandation(const char *db_path, const Recommandation *rec) {
    (void)db_path;
    if (!rec) return -1;

    PGconn *conn = open_pg();
    if (!conn) return -1;

    char annee_str[16];
    snprintf(annee_str, sizeof(annee_str), "%d", rec->annee);

    const char *params[21] = {
        rec->pseudo[0] ? rec->pseudo : "anonyme",
        rec->humeur_texte,
        rec->type,
        rec->titre,
        rec->auteur_ou_realisateur,
        annee_str,
        rec->explication,
        rec->humeur_detectee,
        rec->ton,
        rec->duree_ou_pages,
        rec->pays,
        rec->genre,
        rec->sous_genre,
        rec->raison_precise,
        rec->fun_fact,
        rec->si_tu_aimes,
        rec->message_ami,
        rec->humeur_analysee,
        rec->niveau_intensite,
        rec->mode,
        rec->date_creation
    };

    PGresult *res = PQexecParams(conn,
        "INSERT INTO recommandations "
        "(pseudo, humeur_texte, type, titre, auteur_ou_realisateur, annee, "
        "explication, humeur_detectee, ton, duree_ou_pages, pays, genre, sous_genre, "
        "raison_precise, fun_fact, si_tu_aimes, message_ami, humeur_analysee, "
        "niveau_intensite, mode, date_creation) "
        "VALUES ($1,$2,$3,$4,$5,$6::integer,$7,$8,$9,$10,$11,$12,$13,$14,$15,$16,$17,$18,$19,$20,$21)",
        21, NULL, params, NULL, NULL, 0);

    int rc = (PQresultStatus(res) == PGRES_COMMAND_OK) ? 0 : -1;
    if (rc != 0) fprintf(stderr, "[db] inserer_recommandation: %s\n", PQresultErrorMessage(res));
    PQclear(res);
    PQfinish(conn);
    return rc;
}

static int lire_recommandation_from_pg(PGresult *res, int row, Recommandation *rec) {
    memset(rec, 0, sizeof(*rec));

    /* colonnes: id, pseudo, humeur_texte, type, titre, auteur_ou_realisateur, annee,
       explication, humeur_detectee, ton, duree_ou_pages, pays, genre,
       raison_precise, fun_fact, si_tu_aimes, niveau_intensite, date_creation/date_ajout */

    const char *id_str = PQgetvalue(res, row, 0);
    rec->id = id_str ? atoi(id_str) : 0;

    copy_pg_text(rec->pseudo,                sizeof(rec->pseudo),                PQgetvalue(res, row, 1));
    copy_pg_text(rec->humeur_texte,          sizeof(rec->humeur_texte),          PQgetvalue(res, row, 2));
    copy_pg_text(rec->type,                  sizeof(rec->type),                  PQgetvalue(res, row, 3));
    copy_pg_text(rec->titre,                 sizeof(rec->titre),                 PQgetvalue(res, row, 4));
    copy_pg_text(rec->auteur_ou_realisateur, sizeof(rec->auteur_ou_realisateur), PQgetvalue(res, row, 5));

    const char *annee_str = PQgetvalue(res, row, 6);
    rec->annee = annee_str ? atoi(annee_str) : 0;

    copy_pg_text(rec->explication,      sizeof(rec->explication),      PQgetvalue(res, row, 7));
    copy_pg_text(rec->humeur_detectee,  sizeof(rec->humeur_detectee),  PQgetvalue(res, row, 8));
    copy_pg_text(rec->ton,              sizeof(rec->ton),              PQgetvalue(res, row, 9));
    copy_pg_text(rec->duree_ou_pages,   sizeof(rec->duree_ou_pages),   PQgetvalue(res, row, 10));
    copy_pg_text(rec->pays,             sizeof(rec->pays),             PQgetvalue(res, row, 11));
    copy_pg_text(rec->genre,            sizeof(rec->genre),            PQgetvalue(res, row, 12));
    copy_pg_text(rec->raison_precise,   sizeof(rec->raison_precise),   PQgetvalue(res, row, 13));
    copy_pg_text(rec->fun_fact,         sizeof(rec->fun_fact),         PQgetvalue(res, row, 14));
    copy_pg_text(rec->si_tu_aimes,      sizeof(rec->si_tu_aimes),      PQgetvalue(res, row, 15));
    copy_pg_text(rec->niveau_intensite, sizeof(rec->niveau_intensite), PQgetvalue(res, row, 16));
    copy_pg_text(rec->date_creation,    sizeof(rec->date_creation),    PQgetvalue(res, row, 17));

    return 0;
}

static int lire_liste_depuis_pg(PGresult *res, RecommandationList *out_list) {
    int n = PQntuples(res);
    out_list->items = NULL;
    out_list->count = 0;
    if (n == 0) return 0;

    Recommandation *items = (Recommandation *)malloc((size_t)n * sizeof(Recommandation));
    if (!items) return -1;

    for (int i = 0; i < n; i++) {
        if (lire_recommandation_from_pg(res, i, &items[i]) != 0) {
            free(items);
            return -1;
        }
    }

    out_list->items = items;
    out_list->count = (size_t)n;
    return 0;
}

int lister_historique(const char *db_path, const char *pseudo, RecommandationList *out_list) {
    (void)db_path;
    if (!out_list) return -1;
    out_list->items = NULL;
    out_list->count = 0;

    PGconn *conn = open_pg();
    if (!conn) return -1;

    PGresult *res;
    if (!pseudo || pseudo[0] == '\0') {
        res = PQexec(conn,
            "SELECT id, pseudo, humeur_texte, type, titre, auteur_ou_realisateur, annee, "
            "explication, humeur_detectee, ton, duree_ou_pages, pays, genre, "
            "raison_precise, fun_fact, si_tu_aimes, niveau_intensite, date_creation "
            "FROM recommandations ORDER BY date_creation DESC, id DESC");
    } else {
        const char *params[1] = { pseudo };
        res = PQexecParams(conn,
            "SELECT id, pseudo, humeur_texte, type, titre, auteur_ou_realisateur, annee, "
            "explication, humeur_detectee, ton, duree_ou_pages, pays, genre, "
            "raison_precise, fun_fact, si_tu_aimes, niveau_intensite, date_creation "
            "FROM recommandations WHERE pseudo = $1 ORDER BY date_creation DESC, id DESC",
            1, NULL, params, NULL, NULL, 0);
    }

    int rc = -1;
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        rc = lire_liste_depuis_pg(res, out_list);
    } else {
        fprintf(stderr, "[db] lister_historique: %s\n", PQresultErrorMessage(res));
    }

    PQclear(res);
    PQfinish(conn);
    return rc;
}

int ajouter_favori(const char *db_path, const Recommandation *rec) {
    (void)db_path;
    if (!rec) return -1;

    PGconn *conn = open_pg();
    if (!conn) return -1;

    char annee_str[16];
    snprintf(annee_str, sizeof(annee_str), "%d", rec->annee);

    const char *params[17] = {
        rec->pseudo[0] ? rec->pseudo : "anonyme",
        rec->humeur_texte,
        rec->type,
        rec->titre,
        rec->auteur_ou_realisateur,
        annee_str,
        rec->explication,
        rec->humeur_detectee,
        rec->ton,
        rec->duree_ou_pages,
        rec->pays,
        rec->genre,
        rec->raison_precise,
        rec->fun_fact,
        rec->si_tu_aimes,
        rec->niveau_intensite,
        rec->date_creation
    };

    PGresult *res = PQexecParams(conn,
        "INSERT INTO favoris "
        "(pseudo, humeur_texte, type, titre, auteur_ou_realisateur, annee, "
        "explication, humeur_detectee, ton, duree_ou_pages, pays, genre, "
        "raison_precise, fun_fact, si_tu_aimes, niveau_intensite, date_ajout) "
        "VALUES ($1,$2,$3,$4,$5,$6::integer,$7,$8,$9,$10,$11,$12,$13,$14,$15,$16,$17) "
        "RETURNING id",
        17, NULL, params, NULL, NULL, 0);

    int new_id = -1;
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        new_id = atoi(PQgetvalue(res, 0, 0));
    } else {
        fprintf(stderr, "[db] ajouter_favori: %s\n", PQresultErrorMessage(res));
    }

    PQclear(res);
    PQfinish(conn);
    return new_id;
}

int supprimer_favori(const char *db_path, int favori_id, const char *pseudo) {
    (void)db_path;
    if (!pseudo) return -1;

    PGconn *conn = open_pg();
    if (!conn) return -1;

    char id_str[16];
    snprintf(id_str, sizeof(id_str), "%d", favori_id);

    const char *params[2] = { id_str, pseudo };
    PGresult *res = PQexecParams(conn,
        "DELETE FROM favoris WHERE id = $1::integer AND pseudo = $2",
        2, NULL, params, NULL, NULL, 0);

    int rc = (PQresultStatus(res) == PGRES_COMMAND_OK) ? 0 : -1;
    if (rc != 0) fprintf(stderr, "[db] supprimer_favori: %s\n", PQresultErrorMessage(res));
    PQclear(res);
    PQfinish(conn);
    return rc;
}

int lister_favoris(const char *db_path, const char *pseudo, RecommandationList *out_list) {
    (void)db_path;
    if (!out_list || !pseudo) return -1;
    out_list->items = NULL;
    out_list->count = 0;

    PGconn *conn = open_pg();
    if (!conn) return -1;

    const char *params[1] = { pseudo };
    PGresult *res = PQexecParams(conn,
        "SELECT id, pseudo, humeur_texte, type, titre, auteur_ou_realisateur, annee, "
        "explication, humeur_detectee, ton, duree_ou_pages, pays, genre, "
        "raison_precise, fun_fact, si_tu_aimes, niveau_intensite, date_ajout "
        "FROM favoris WHERE pseudo = $1 ORDER BY date_ajout DESC, id DESC",
        1, NULL, params, NULL, NULL, 0);

    int rc = -1;
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        rc = lire_liste_depuis_pg(res, out_list);
    } else {
        fprintf(stderr, "[db] lister_favoris: %s\n", PQresultErrorMessage(res));
    }

    PQclear(res);
    PQfinish(conn);
    return rc;
}

int cache_get(const char *db_path, const char *humeur, const char *categories, const char *mode, char **out_json) {
    (void)db_path;
    if (!humeur || !categories || !mode || !out_json) return -1;
    *out_json = NULL;

    char key[768];
    snprintf(key, sizeof(key), "%s|%s|%s", humeur, categories, mode);

    PGconn *conn = open_pg();
    if (!conn) return -1;

    const char *params[1] = { key };
    PGresult *res = PQexecParams(conn,
        "SELECT resultat_json FROM cache "
        "WHERE cache_key = $1 "
        "AND date_creation::timestamptz > NOW() - INTERVAL '1 hour' "
        "LIMIT 1",
        1, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        const char *json = PQgetvalue(res, 0, 0);
        if (json && json[0]) {
            *out_json = (char *)malloc(strlen(json) + 1);
            if (*out_json) strcpy(*out_json, json);
        }
    }

    PQclear(res);
    PQfinish(conn);
    return (*out_json) ? 0 : -1;
}

int cache_set(const char *db_path, const char *humeur, const char *categories, const char *mode, const char *json) {
    (void)db_path;
    if (!humeur || !categories || !mode || !json) return -1;

    char key[768];
    char timestamp[64];
    snprintf(key, sizeof(key), "%s|%s|%s", humeur, categories, mode);

    {
        time_t now = time(NULL);
        struct tm t;
#ifdef _WIN32
        gmtime_s(&t, &now);
#else
        gmtime_r(&now, &t);
#endif
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &t);
    }

    PGconn *conn = open_pg();
    if (!conn) return -1;

    const char *params[3] = { key, json, timestamp };
    PGresult *res = PQexecParams(conn,
        "INSERT INTO cache (cache_key, resultat_json, date_creation) VALUES ($1, $2, $3) "
        "ON CONFLICT (cache_key) DO UPDATE SET "
        "resultat_json = EXCLUDED.resultat_json, date_creation = EXCLUDED.date_creation",
        3, NULL, params, NULL, NULL, 0);

    int rc = (PQresultStatus(res) == PGRES_COMMAND_OK) ? 0 : -1;
    if (rc != 0) fprintf(stderr, "[db] cache_set: %s\n", PQresultErrorMessage(res));
    PQclear(res);
    PQfinish(conn);
    return rc;
}
