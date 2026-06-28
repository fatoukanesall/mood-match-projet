#include "db.h"

#include <sqlite3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int copy_sqlite_text(char *dest, size_t dest_size, const unsigned char *src) {
    if (!dest || dest_size == 0) return -1;
    if (!src) { dest[0] = '\0'; return 0; }
    int written = snprintf(dest, dest_size, "%s", (const char *)src);
    if (written < 0 || (size_t)written >= dest_size) return -1;
    return 0;
}

static int open_db(const char *db_path, sqlite3 **out_db) {
    if (!db_path || !out_db) return -1;
    int rc = sqlite3_open(db_path, out_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[db] sqlite3_open failed: %s\n", sqlite3_errmsg(*out_db));
        if (*out_db) { sqlite3_close(*out_db); *out_db = NULL; }
        return -1;
    }
    return 0;
}

int init_db(const char *db_path) {
    sqlite3 *db = NULL;
    char *err_msg = NULL;
    int rc;

    if (open_db(db_path, &db) != 0) return -1;

    const char *sql_recommandations =
        "CREATE TABLE IF NOT EXISTS recommandations ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "pseudo TEXT NOT NULL DEFAULT 'anonyme',"
        "humeur_texte TEXT NOT NULL,"
        "type TEXT NOT NULL,"
        "titre TEXT NOT NULL,"
        "auteur_ou_realisateur TEXT NOT NULL,"
        "annee INTEGER NOT NULL,"
        "explication TEXT NOT NULL,"
        "humeur_detectee TEXT NOT NULL,"
        "ton TEXT NOT NULL,"
        "duree_ou_pages TEXT NOT NULL,"
        "pays TEXT,"
        "genre TEXT,"
        "sous_genre TEXT,"
        "raison_precise TEXT,"
        "fun_fact TEXT,"
        "si_tu_aimes TEXT,"
        "message_ami TEXT,"
        "humeur_analysee TEXT,"
        "niveau_intensite TEXT,"
        "mode TEXT,"
        "date_creation TEXT NOT NULL"
        ");";

    rc = sqlite3_exec(db, sql_recommandations, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[db] init recommandations: %s\n", err_msg ? err_msg : "?");
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    /* Migration: ajoute la colonne pseudo si elle n'existe pas encore */
    sqlite3_exec(db,
        "ALTER TABLE recommandations ADD COLUMN pseudo TEXT NOT NULL DEFAULT 'anonyme'",
        NULL, NULL, NULL);

    const char *sql_favoris =
        "CREATE TABLE IF NOT EXISTS favoris ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "pseudo TEXT NOT NULL,"
        "humeur_texte TEXT NOT NULL,"
        "type TEXT NOT NULL,"
        "titre TEXT NOT NULL,"
        "auteur_ou_realisateur TEXT NOT NULL,"
        "annee INTEGER NOT NULL,"
        "explication TEXT NOT NULL,"
        "humeur_detectee TEXT NOT NULL,"
        "ton TEXT NOT NULL,"
        "duree_ou_pages TEXT NOT NULL,"
        "pays TEXT,"
        "genre TEXT,"
        "raison_precise TEXT,"
        "fun_fact TEXT,"
        "si_tu_aimes TEXT,"
        "niveau_intensite TEXT,"
        "date_ajout TEXT NOT NULL"
        ");";

    rc = sqlite3_exec(db, sql_favoris, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[db] init favoris: %s\n", err_msg ? err_msg : "?");
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    const char *sql_utilisateurs =
        "CREATE TABLE IF NOT EXISTS utilisateurs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "pseudo TEXT NOT NULL UNIQUE,"
        "date_creation TEXT NOT NULL"
        ");";

    rc = sqlite3_exec(db, sql_utilisateurs, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[db] init utilisateurs: %s\n", err_msg ? err_msg : "?");
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    const char *sql_cache =
        "CREATE TABLE IF NOT EXISTS cache ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "cache_key TEXT NOT NULL UNIQUE,"
        "resultat_json TEXT NOT NULL,"
        "date_creation TEXT NOT NULL"
        ");";

    rc = sqlite3_exec(db, sql_cache, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[db] init cache: %s\n", err_msg ? err_msg : "?");
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    sqlite3_close(db);
    return 0;
}

/* Retourne  0 : pseudo créé avec succès (nouveau compte)
             1 : pseudo déjà pris
            -1 : erreur base de données */
int creer_utilisateur(const char *db_path, const char *pseudo) {
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "INSERT INTO utilisateurs (pseudo, date_creation) "
        "VALUES (?, datetime('now'));";

    if (!pseudo || pseudo[0] == '\0') return -1;
    if (open_db(db_path, &db) != 0) return -1;

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { sqlite3_close(db); return -1; }

    sqlite3_bind_text(stmt, 1, pseudo, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if (rc == SQLITE_DONE)       return 0;  /* créé */
    if (rc == SQLITE_CONSTRAINT) return 1;  /* déjà pris */
    return -1;                              /* erreur */
}

int inserer_recommandation(const char *db_path, const Recommandation *rec) {
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "INSERT INTO recommandations ("
        "pseudo, humeur_texte, type, titre, auteur_ou_realisateur, annee, "
        "explication, humeur_detectee, ton, duree_ou_pages, pays, genre, sous_genre, "
        "raison_precise, fun_fact, si_tu_aimes, message_ami, humeur_analysee, "
        "niveau_intensite, mode, date_creation"
        ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    if (!rec) return -1;
    if (open_db(db_path, &db) != 0) return -1;

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[db] inserer_recommandation prepare: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    sqlite3_bind_text(stmt,  1, rec->pseudo[0] ? rec->pseudo : "anonyme", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  2, rec->humeur_texte, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  3, rec->type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  4, rec->titre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  5, rec->auteur_ou_realisateur, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt,  6, rec->annee);
    sqlite3_bind_text(stmt,  7, rec->explication, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  8, rec->humeur_detectee, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  9, rec->ton, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, rec->duree_ou_pages, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 11, rec->pays, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 12, rec->genre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 13, rec->sous_genre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 14, rec->raison_precise, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 15, rec->fun_fact, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 16, rec->si_tu_aimes, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 17, rec->message_ami, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 18, rec->humeur_analysee, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 19, rec->niveau_intensite, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 20, rec->mode, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 21, rec->date_creation, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[db] inserer_recommandation step failed\n");
        return -1;
    }
    return 0;
}

static int lire_recommandation_from_stmt(sqlite3_stmt *stmt, Recommandation *rec) {
    memset(rec, 0, sizeof(*rec));
    rec->id   = sqlite3_column_int(stmt, 0);
    rec->annee = sqlite3_column_int(stmt, 6);

    if (copy_sqlite_text(rec->pseudo,                sizeof(rec->pseudo),                sqlite3_column_text(stmt,  1)) != 0) return -1;
    if (copy_sqlite_text(rec->humeur_texte,          sizeof(rec->humeur_texte),          sqlite3_column_text(stmt,  2)) != 0) return -1;
    if (copy_sqlite_text(rec->type,                  sizeof(rec->type),                  sqlite3_column_text(stmt,  3)) != 0) return -1;
    if (copy_sqlite_text(rec->titre,                 sizeof(rec->titre),                 sqlite3_column_text(stmt,  4)) != 0) return -1;
    if (copy_sqlite_text(rec->auteur_ou_realisateur, sizeof(rec->auteur_ou_realisateur), sqlite3_column_text(stmt,  5)) != 0) return -1;
    if (copy_sqlite_text(rec->explication,           sizeof(rec->explication),           sqlite3_column_text(stmt,  7)) != 0) return -1;
    if (copy_sqlite_text(rec->humeur_detectee,       sizeof(rec->humeur_detectee),       sqlite3_column_text(stmt,  8)) != 0) return -1;
    if (copy_sqlite_text(rec->ton,                   sizeof(rec->ton),                   sqlite3_column_text(stmt,  9)) != 0) return -1;
    if (copy_sqlite_text(rec->duree_ou_pages,        sizeof(rec->duree_ou_pages),        sqlite3_column_text(stmt, 10)) != 0) return -1;
    if (copy_sqlite_text(rec->pays,                  sizeof(rec->pays),                  sqlite3_column_text(stmt, 11)) != 0) return -1;
    if (copy_sqlite_text(rec->genre,                 sizeof(rec->genre),                 sqlite3_column_text(stmt, 12)) != 0) return -1;
    if (copy_sqlite_text(rec->raison_precise,        sizeof(rec->raison_precise),        sqlite3_column_text(stmt, 13)) != 0) return -1;
    if (copy_sqlite_text(rec->fun_fact,              sizeof(rec->fun_fact),              sqlite3_column_text(stmt, 14)) != 0) return -1;
    if (copy_sqlite_text(rec->si_tu_aimes,           sizeof(rec->si_tu_aimes),           sqlite3_column_text(stmt, 15)) != 0) return -1;
    if (copy_sqlite_text(rec->niveau_intensite,      sizeof(rec->niveau_intensite),      sqlite3_column_text(stmt, 16)) != 0) return -1;
    if (copy_sqlite_text(rec->date_creation,         sizeof(rec->date_creation),         sqlite3_column_text(stmt, 17)) != 0) return -1;
    return 0;
}

static int lire_liste_depuis_stmt(sqlite3 *db, sqlite3_stmt *stmt, RecommandationList *out_list) {
    Recommandation *items = NULL;
    size_t count = 0, capacity = 0;
    int rc;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (count == capacity) {
            size_t new_cap = (capacity == 0) ? 8 : capacity * 2;
            Recommandation *tmp = (Recommandation *)realloc(items, new_cap * sizeof(Recommandation));
            if (!tmp) { free(items); sqlite3_finalize(stmt); return -1; }
            items = tmp;
            capacity = new_cap;
        }
        if (lire_recommandation_from_stmt(stmt, &items[count]) != 0) {
            free(items);
            sqlite3_finalize(stmt);
            return -1;
        }
        count++;
    }

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[db] step failed: %s\n", sqlite3_errmsg(db));
        free(items);
        sqlite3_finalize(stmt);
        return -1;
    }

    sqlite3_finalize(stmt);
    out_list->items = items;
    out_list->count = count;
    return 0;
}

int lister_historique(const char *db_path, const char *pseudo, RecommandationList *out_list) {
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    const char *sql_all =
        "SELECT id, pseudo, humeur_texte, type, titre, auteur_ou_realisateur, annee, "
        "explication, humeur_detectee, ton, duree_ou_pages, pays, genre, "
        "raison_precise, fun_fact, si_tu_aimes, niveau_intensite, date_creation "
        "FROM recommandations "
        "ORDER BY date_creation DESC, id DESC;";
    const char *sql_pseudo =
        "SELECT id, pseudo, humeur_texte, type, titre, auteur_ou_realisateur, annee, "
        "explication, humeur_detectee, ton, duree_ou_pages, pays, genre, "
        "raison_precise, fun_fact, si_tu_aimes, niveau_intensite, date_creation "
        "FROM recommandations WHERE pseudo = ? "
        "ORDER BY date_creation DESC, id DESC;";

    if (!out_list) return -1;
    out_list->items = NULL;
    out_list->count = 0;

    if (open_db(db_path, &db) != 0) return -1;

    int rc;
    if (!pseudo || pseudo[0] == '\0') {
        rc = sqlite3_prepare_v2(db, sql_all, -1, &stmt, NULL);
    } else {
        rc = sqlite3_prepare_v2(db, sql_pseudo, -1, &stmt, NULL);
        if (rc == SQLITE_OK) sqlite3_bind_text(stmt, 1, pseudo, -1, SQLITE_TRANSIENT);
    }

    if (rc != SQLITE_OK) {
        fprintf(stderr, "[db] lister_historique prepare: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    rc = lire_liste_depuis_stmt(db, stmt, out_list);
    sqlite3_close(db);
    return rc;
}

int ajouter_favori(const char *db_path, const Recommandation *rec) {
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "INSERT INTO favoris ("
        "pseudo, humeur_texte, type, titre, auteur_ou_realisateur, annee, "
        "explication, humeur_detectee, ton, duree_ou_pages, pays, genre, "
        "raison_precise, fun_fact, si_tu_aimes, niveau_intensite, date_ajout"
        ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    if (!rec) return -1;
    if (open_db(db_path, &db) != 0) return -1;

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { sqlite3_close(db); return -1; }

    sqlite3_bind_text(stmt,  1, rec->pseudo[0] ? rec->pseudo : "anonyme", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  2, rec->humeur_texte, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  3, rec->type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  4, rec->titre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  5, rec->auteur_ou_realisateur, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt,  6, rec->annee);
    sqlite3_bind_text(stmt,  7, rec->explication, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  8, rec->humeur_detectee, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  9, rec->ton, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, rec->duree_ou_pages, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 11, rec->pays, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 12, rec->genre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 13, rec->raison_precise, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 14, rec->fun_fact, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 15, rec->si_tu_aimes, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 16, rec->niveau_intensite, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 17, rec->date_creation, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    int last_id = (int)sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if (rc != SQLITE_DONE) return -1;
    return last_id;
}

int supprimer_favori(const char *db_path, int favori_id, const char *pseudo) {
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    const char *sql = "DELETE FROM favoris WHERE id = ? AND pseudo = ?;";

    if (!pseudo) return -1;
    if (open_db(db_path, &db) != 0) return -1;

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { sqlite3_close(db); return -1; }

    sqlite3_bind_int (stmt, 1, favori_id);
    sqlite3_bind_text(stmt, 2, pseudo, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int lister_favoris(const char *db_path, const char *pseudo, RecommandationList *out_list) {
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT id, pseudo, humeur_texte, type, titre, auteur_ou_realisateur, annee, "
        "explication, humeur_detectee, ton, duree_ou_pages, pays, genre, "
        "raison_precise, fun_fact, si_tu_aimes, niveau_intensite, date_ajout "
        "FROM favoris WHERE pseudo = ? "
        "ORDER BY date_ajout DESC, id DESC;";

    if (!out_list || !pseudo) return -1;
    out_list->items = NULL;
    out_list->count = 0;

    if (open_db(db_path, &db) != 0) return -1;

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { sqlite3_close(db); return -1; }
    sqlite3_bind_text(stmt, 1, pseudo, -1, SQLITE_TRANSIENT);

    rc = lire_liste_depuis_stmt(db, stmt, out_list);
    sqlite3_close(db);
    return rc;
}

int cache_get(const char *db_path, const char *humeur, const char *categories, const char *mode, char **out_json) {
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT resultat_json FROM cache "
        "WHERE cache_key = ? "
        "AND datetime(date_creation) > datetime('now', '-1 hour') "
        "LIMIT 1;";
    char key[768];

    if (!humeur || !categories || !mode || !out_json) return -1;
    *out_json = NULL;

    snprintf(key, sizeof(key), "%s|%s|%s", humeur, categories, mode);

    if (open_db(db_path, &db) != 0) return -1;

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { sqlite3_close(db); return -1; }
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const unsigned char *json = sqlite3_column_text(stmt, 0);
        if (json) {
            *out_json = (char *)malloc(strlen((const char *)json) + 1);
            if (*out_json) strcpy(*out_json, (const char *)json);
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return (*out_json) ? 0 : -1;
}

int cache_set(const char *db_path, const char *humeur, const char *categories, const char *mode, const char *json) {
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "INSERT INTO cache (cache_key, resultat_json, date_creation) "
        "VALUES (?, ?, datetime('now')) "
        "ON CONFLICT(cache_key) DO UPDATE SET resultat_json=excluded.resultat_json, date_creation=excluded.date_creation;";
    char key[768];
    char timestamp[64];

    if (!humeur || !categories || !mode || !json) return -1;
    snprintf(key, sizeof(key), "%s|%s|%s", humeur, categories, mode);

    /* Use current UTC time */
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

    if (open_db(db_path, &db) != 0) return -1;

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { sqlite3_close(db); return -1; }
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, json, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return (rc == SQLITE_DONE) ? 0 : -1;
}
