#include "server.h"

#include "ai_client.h"
#include "db.h"

#include <cjson/cJSON.h>
#include <microhttpd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_DB_PATH_LEN 512
#define MAX_REQUEST_BODY_LEN 16384

typedef struct {
    char *body;
    size_t body_len;
    int is_post;
} RequestContext;

static struct MHD_Daemon *g_daemon = NULL;
static char g_db_path[MAX_DB_PATH_LEN] = {0};

static void timestamp_now(char *out, size_t out_size) {
    time_t now;
    struct tm t;
    if (!out || out_size == 0) return;
    now = time(NULL);
#ifdef _WIN32
    gmtime_s(&t, &now);
#else
    gmtime_r(&now, &t);
#endif
    strftime(out, out_size, "%Y-%m-%dT%H:%M:%SZ", &t);
}

static int add_cors_headers(struct MHD_Response *response) {
    if (MHD_add_response_header(response, "Access-Control-Allow-Origin", "*") != MHD_YES) return -1;
    if (MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS") != MHD_YES) return -1;
    if (MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type, X-Pseudo") != MHD_YES) return -1;
    if (MHD_add_response_header(response, "Content-Type", "application/json; charset=utf-8") != MHD_YES) return -1;
    return 0;
}

static enum MHD_Result queue_json_response(struct MHD_Connection *connection, unsigned int status, const char *json) {
    struct MHD_Response *response;
    enum MHD_Result result;
    const char *payload = json ? json : "{}";

    response = MHD_create_response_from_buffer(strlen(payload), (void *)payload, MHD_RESPMEM_MUST_COPY);
    if (!response) return MHD_NO;
    if (add_cors_headers(response) != 0) { MHD_destroy_response(response); return MHD_NO; }
    result = MHD_queue_response(connection, status, response);
    MHD_destroy_response(response);
    return result;
}

static enum MHD_Result queue_no_content_response(struct MHD_Connection *connection) {
    struct MHD_Response *response;
    enum MHD_Result result;

    response = MHD_create_response_from_buffer(0, (void *)"", MHD_RESPMEM_PERSISTENT);
    if (!response) return MHD_NO;
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type, X-Pseudo");
    result = MHD_queue_response(connection, MHD_HTTP_NO_CONTENT, response);
    MHD_destroy_response(response);
    return result;
}

/* Serialize a single Recommandation to cJSON object */
static cJSON *recommandation_to_json(const Recommandation *rec) {
    cJSON *obj;
    if (!rec) return NULL;
    obj = cJSON_CreateObject();
    if (!obj) return NULL;

    cJSON_AddNumberToObject(obj, "id",                    rec->id);
    cJSON_AddStringToObject(obj, "pseudo",                rec->pseudo);
    cJSON_AddStringToObject(obj, "humeur_texte",          rec->humeur_texte);
    cJSON_AddStringToObject(obj, "type",                  rec->type);
    cJSON_AddStringToObject(obj, "titre",                 rec->titre);
    cJSON_AddStringToObject(obj, "auteur_ou_realisateur", rec->auteur_ou_realisateur);
    cJSON_AddNumberToObject(obj, "annee",                 rec->annee);
    cJSON_AddStringToObject(obj, "explication",           rec->explication);
    cJSON_AddStringToObject(obj, "humeur_detectee",       rec->humeur_detectee);
    cJSON_AddStringToObject(obj, "ton",                   rec->ton);
    cJSON_AddStringToObject(obj, "duree_ou_pages",        rec->duree_ou_pages);
    cJSON_AddStringToObject(obj, "pays",                  rec->pays);
    cJSON_AddStringToObject(obj, "genre",                 rec->genre);
    cJSON_AddStringToObject(obj, "raison_precise",        rec->raison_precise);
    cJSON_AddStringToObject(obj, "fun_fact",              rec->fun_fact);
    cJSON_AddStringToObject(obj, "si_tu_aimes",           rec->si_tu_aimes);
    cJSON_AddStringToObject(obj, "niveau_intensite",      rec->niveau_intensite);
    cJSON_AddStringToObject(obj, "date_creation",         rec->date_creation);
    return obj;
}

static int serialiser_liste_json(const RecommandationList *list, const char *key, char **out_json) {
    cJSON *root, *arr;
    size_t i;

    if (!list || !key || !out_json) return -1;
    root = cJSON_CreateObject();
    if (!root) return -1;
    arr = cJSON_CreateArray();
    if (!arr) { cJSON_Delete(root); return -1; }

    for (i = 0; i < list->count; i++) {
        cJSON *obj = recommandation_to_json(&list->items[i]);
        if (!obj) { cJSON_Delete(arr); cJSON_Delete(root); return -1; }
        cJSON_AddItemToArray(arr, obj);
    }

    cJSON_AddItemToObject(root, key, arr);
    *out_json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return (*out_json) ? 0 : -1;
}

static int ajouter_tableau_si_necessaire(cJSON *root, const char *key, const Recommandation *items, int count) {
    cJSON *arr;
    int i;
    if (!root || !key || !items || count <= 0) return 0;
    arr = cJSON_CreateArray();
    if (!arr) return -1;
    for (i = 0; i < count; i++) {
        cJSON *obj = recommandation_to_json(&items[i]);
        if (!obj) { cJSON_Delete(arr); return -1; }
        cJSON_AddItemToArray(arr, obj);
    }
    cJSON_AddItemToObject(root, key, arr);
    return 0;
}

static int serialiser_resultat_json(const ResultatRecherche *resultat, char **out_json) {
    cJSON *root;
    if (!resultat || !out_json) return -1;
    root = cJSON_CreateObject();
    if (!root) return -1;

    if (resultat->message_ami[0])    cJSON_AddStringToObject(root, "message_ami",    resultat->message_ami);
    if (resultat->humeur_analysee[0]) cJSON_AddStringToObject(root, "humeur_analysee", resultat->humeur_analysee);

    if (ajouter_tableau_si_necessaire(root, "films",  resultat->films,  resultat->nb_films)  != 0 ||
        ajouter_tableau_si_necessaire(root, "livres", resultat->livres, resultat->nb_livres) != 0 ||
        ajouter_tableau_si_necessaire(root, "series", resultat->series, resultat->nb_series) != 0 ||
        ajouter_tableau_si_necessaire(root, "animes", resultat->animes, resultat->nb_animes) != 0) {
        cJSON_Delete(root);
        return -1;
    }

    *out_json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return (*out_json) ? 0 : -1;
}

/* Stamp pseudo + timestamp on all items then insert into historique */
static int inserer_tous_les_resultats(ResultatRecherche *resultat, const char *pseudo) {
    char timestamp[64];
    int i;
    timestamp_now(timestamp, sizeof(timestamp));

    for (i = 0; i < resultat->nb_films; i++) {
        snprintf(resultat->films[i].pseudo,        sizeof(resultat->films[i].pseudo),        "%s", pseudo);
        snprintf(resultat->films[i].date_creation, sizeof(resultat->films[i].date_creation), "%s", timestamp);
        if (inserer_recommandation(g_db_path, &resultat->films[i]) != 0) return -1;
    }
    for (i = 0; i < resultat->nb_livres; i++) {
        snprintf(resultat->livres[i].pseudo,        sizeof(resultat->livres[i].pseudo),        "%s", pseudo);
        snprintf(resultat->livres[i].date_creation, sizeof(resultat->livres[i].date_creation), "%s", timestamp);
        if (inserer_recommandation(g_db_path, &resultat->livres[i]) != 0) return -1;
    }
    for (i = 0; i < resultat->nb_series; i++) {
        snprintf(resultat->series[i].pseudo,        sizeof(resultat->series[i].pseudo),        "%s", pseudo);
        snprintf(resultat->series[i].date_creation, sizeof(resultat->series[i].date_creation), "%s", timestamp);
        if (inserer_recommandation(g_db_path, &resultat->series[i]) != 0) return -1;
    }
    for (i = 0; i < resultat->nb_animes; i++) {
        snprintf(resultat->animes[i].pseudo,        sizeof(resultat->animes[i].pseudo),        "%s", pseudo);
        snprintf(resultat->animes[i].date_creation, sizeof(resultat->animes[i].date_creation), "%s", timestamp);
        if (inserer_recommandation(g_db_path, &resultat->animes[i]) != 0) return -1;
    }
    return 0;
}

static enum MHD_Result gerer_post_recommandation(struct MHD_Connection *connection, const char *body) {
    cJSON *root = NULL;
    cJSON *humeur_json = NULL, *type_json = NULL, *mode_json = NULL;
    const char *humeur, *type_val, *mode_val, *pseudo;
    ResultatRecherche resultat = {0};
    char *json_response = NULL;
    char *cached_json = NULL;
    char err_buf[700];

    if (!body)
        return queue_json_response(connection, MHD_HTTP_BAD_REQUEST, "{\"erreur\":\"Corps de requete manquant\"}");

    root = cJSON_Parse(body);
    if (!root)
        return queue_json_response(connection, MHD_HTTP_BAD_REQUEST, "{\"erreur\":\"JSON invalide\"}");

    humeur_json = cJSON_GetObjectItemCaseSensitive(root, "humeur");
    type_json   = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (!cJSON_IsString(humeur_json) || !humeur_json->valuestring ||
        !cJSON_IsString(type_json)   || !type_json->valuestring) {
        cJSON_Delete(root);
        return queue_json_response(connection, MHD_HTTP_BAD_REQUEST, "{\"erreur\":\"Champs humeur/type invalides\"}");
    }

    humeur   = humeur_json->valuestring;
    type_val = type_json->valuestring;
    mode_json = cJSON_GetObjectItemCaseSensitive(root, "mode");
    mode_val  = (cJSON_IsString(mode_json) && mode_json->valuestring) ? mode_json->valuestring : "courage";

    pseudo = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Pseudo");
    if (!pseudo || pseudo[0] == '\0') pseudo = "anonyme";

    /* --- Verifier le cache --- */
    if (cache_get(g_db_path, humeur, type_val, mode_val, &cached_json) == 0 && cached_json) {
        /* Cache hit: parser le resultat pour l'inserer dans l'historique */
        ResultatRecherche cached_res = {0};
        if (parser_recommandations_json(cached_json, humeur, type_val, &cached_res) == 0) {
            inserer_tous_les_resultats(&cached_res, pseudo);
            free_resultat_recherche(&cached_res);
        }
        enum MHD_Result result = queue_json_response(connection, MHD_HTTP_OK, cached_json);
        free(cached_json);
        cJSON_Delete(root);
        return result;
    }
    free(cached_json);

    /* --- Appel Groq --- */
    if (obtenir_recommandation(humeur, type_val, mode_val, &resultat) != 0) {
        const char *detail = ai_last_error[0] ? ai_last_error : "Erreur inconnue lors de l appel IA.";
        snprintf(err_buf, sizeof(err_buf),
            "{\"erreur\":\"Echec appel IA\",\"detail\":\"%s\"}", detail);
        cJSON_Delete(root);
        return queue_json_response(connection, MHD_HTTP_BAD_GATEWAY, err_buf);
    }

    /* --- Serialiser et mettre en cache --- */
    if (serialiser_resultat_json(&resultat, &json_response) != 0) {
        free_resultat_recherche(&resultat);
        cJSON_Delete(root);
        return queue_json_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR,
            "{\"erreur\":\"Echec serialisation JSON\"}");
    }
    cache_set(g_db_path, humeur, type_val, mode_val, json_response);

    /* --- Inserer dans l'historique --- */
    if (inserer_tous_les_resultats(&resultat, pseudo) != 0) {
        free(json_response);
        free_resultat_recherche(&resultat);
        cJSON_Delete(root);
        return queue_json_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR,
            "{\"erreur\":\"Echec sauvegarde base de donnees\"}");
    }

    free_resultat_recherche(&resultat);
    cJSON_Delete(root);

    enum MHD_Result result = queue_json_response(connection, MHD_HTTP_OK, json_response);
    cJSON_free(json_response);
    return result;
}

static enum MHD_Result gerer_get_historique(struct MHD_Connection *connection) {
    RecommandationList list = {0};
    char *json_response = NULL;
    enum MHD_Result result;

    const char *pseudo = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Pseudo");

    if (lister_historique(g_db_path, pseudo, &list) != 0)
        return queue_json_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR,
            "{\"erreur\":\"Echec lecture historique\"}");

    if (serialiser_liste_json(&list, "historique", &json_response) != 0) {
        free_recommandation_list(&list);
        return queue_json_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR,
            "{\"erreur\":\"Echec serialisation historique\"}");
    }

    result = queue_json_response(connection, MHD_HTTP_OK, json_response);
    cJSON_free(json_response);
    free_recommandation_list(&list);
    return result;
}

static enum MHD_Result gerer_get_favoris(struct MHD_Connection *connection) {
    RecommandationList list = {0};
    char *json_response = NULL;
    enum MHD_Result result;

    const char *pseudo = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Pseudo");
    if (!pseudo || pseudo[0] == '\0') pseudo = "anonyme";

    if (lister_favoris(g_db_path, pseudo, &list) != 0)
        return queue_json_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR,
            "{\"erreur\":\"Echec lecture favoris\"}");

    if (serialiser_liste_json(&list, "favoris", &json_response) != 0) {
        free_recommandation_list(&list);
        return queue_json_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR,
            "{\"erreur\":\"Echec serialisation favoris\"}");
    }

    result = queue_json_response(connection, MHD_HTTP_OK, json_response);
    cJSON_free(json_response);
    free_recommandation_list(&list);
    return result;
}

static enum MHD_Result gerer_post_favori(struct MHD_Connection *connection, const char *body) {
    cJSON *root = NULL;
    Recommandation rec;
    char timestamp[64];
    int new_id;

    const char *pseudo = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Pseudo");
    if (!pseudo || pseudo[0] == '\0') pseudo = "anonyme";

    if (!body)
        return queue_json_response(connection, MHD_HTTP_BAD_REQUEST,
            "{\"erreur\":\"Corps de requete manquant\"}");

    root = cJSON_Parse(body);
    if (!root)
        return queue_json_response(connection, MHD_HTTP_BAD_REQUEST,
            "{\"erreur\":\"JSON invalide\"}");

    memset(&rec, 0, sizeof(rec));
    snprintf(rec.pseudo, sizeof(rec.pseudo), "%s", pseudo);
    timestamp_now(timestamp, sizeof(timestamp));
    snprintf(rec.date_creation, sizeof(rec.date_creation), "%s", timestamp);

    #define EXTRACT_STR(field, key) do { \
        cJSON *_j = cJSON_GetObjectItemCaseSensitive(root, key); \
        if (cJSON_IsString(_j) && _j->valuestring) \
            snprintf(rec.field, sizeof(rec.field), "%s", _j->valuestring); \
    } while(0)
    #define EXTRACT_INT(field, key) do { \
        cJSON *_j = cJSON_GetObjectItemCaseSensitive(root, key); \
        if (cJSON_IsNumber(_j)) rec.field = _j->valueint; \
    } while(0)

    EXTRACT_STR(humeur_texte,          "humeur_texte");
    EXTRACT_STR(type,                  "type");
    EXTRACT_STR(titre,                 "titre");
    EXTRACT_STR(auteur_ou_realisateur, "auteur_ou_realisateur");
    EXTRACT_INT(annee,                 "annee");
    EXTRACT_STR(explication,           "explication");
    EXTRACT_STR(humeur_detectee,       "humeur_detectee");
    EXTRACT_STR(ton,                   "ton");
    EXTRACT_STR(duree_ou_pages,        "duree_ou_pages");
    EXTRACT_STR(pays,                  "pays");
    EXTRACT_STR(genre,                 "genre");
    EXTRACT_STR(raison_precise,        "raison_precise");
    EXTRACT_STR(fun_fact,              "fun_fact");
    EXTRACT_STR(si_tu_aimes,           "si_tu_aimes");
    EXTRACT_STR(niveau_intensite,      "niveau_intensite");

    #undef EXTRACT_STR
    #undef EXTRACT_INT

    cJSON_Delete(root);

    if (rec.titre[0] == '\0')
        return queue_json_response(connection, MHD_HTTP_BAD_REQUEST,
            "{\"erreur\":\"Champ titre manquant\"}");

    new_id = ajouter_favori(g_db_path, &rec);
    if (new_id <= 0)
        return queue_json_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR,
            "{\"erreur\":\"Echec ajout favori\"}");

    char resp[64];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"id\":%d}", new_id);
    return queue_json_response(connection, MHD_HTTP_OK, resp);
}

static enum MHD_Result gerer_post_pseudo(struct MHD_Connection *connection, const char *body) {
    cJSON *root = NULL;
    cJSON *pseudo_json = NULL;
    int est_nouveau = 0;
    char resp[256];

    if (!body)
        return queue_json_response(connection, MHD_HTTP_BAD_REQUEST,
            "{\"erreur\":\"Corps de requete manquant\"}");

    root = cJSON_Parse(body);
    if (!root)
        return queue_json_response(connection, MHD_HTTP_BAD_REQUEST,
            "{\"erreur\":\"JSON invalide\"}");

    pseudo_json = cJSON_GetObjectItemCaseSensitive(root, "pseudo");
    if (!cJSON_IsString(pseudo_json) || !pseudo_json->valuestring || pseudo_json->valuestring[0] == '\0') {
        cJSON_Delete(root);
        return queue_json_response(connection, MHD_HTTP_BAD_REQUEST,
            "{\"erreur\":\"Champ pseudo manquant ou vide\"}");
    }

    const char *pseudo = pseudo_json->valuestring;
    int rc_pseudo = creer_utilisateur(g_db_path, pseudo);
    (void)est_nouveau;

    if (rc_pseudo == 1) {
        /* Pseudo déjà pris */
        cJSON_Delete(root);
        return queue_json_response(connection, MHD_HTTP_CONFLICT,
            "{\"ok\":false,\"erreur\":\"Ce pseudo est deja pris. Choisis-en un autre.\"}");
    }
    if (rc_pseudo != 0) {
        cJSON_Delete(root);
        return queue_json_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR,
            "{\"erreur\":\"Erreur base de donnees\"}");
    }

    snprintf(resp, sizeof(resp),
        "{\"ok\":true,\"message\":\"Bienvenue %s ! Ton compte a ete cree.\"}",
        pseudo);

    cJSON_Delete(root);
    return queue_json_response(connection, MHD_HTTP_OK, resp);
}

static enum MHD_Result gerer_delete_favori(struct MHD_Connection *connection) {
    const char *id_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "id");
    const char *pseudo = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Pseudo");

    if (!id_str || !pseudo || pseudo[0] == '\0')
        return queue_json_response(connection, MHD_HTTP_BAD_REQUEST,
            "{\"erreur\":\"Parametre id ou pseudo manquant\"}");

    int favori_id = atoi(id_str);
    if (favori_id <= 0)
        return queue_json_response(connection, MHD_HTTP_BAD_REQUEST,
            "{\"erreur\":\"id invalide\"}");

    if (supprimer_favori(g_db_path, favori_id, pseudo) != 0)
        return queue_json_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR,
            "{\"erreur\":\"Echec suppression favori\"}");

    return queue_json_response(connection, MHD_HTTP_OK, "{\"ok\":true}");
}

static void cleanup_request_context(RequestContext *ctx) {
    if (!ctx) return;
    free(ctx->body);
    free(ctx);
}

static void request_completed_callback(
    void *cls, struct MHD_Connection *connection,
    void **con_cls, enum MHD_RequestTerminationCode toe
) {
    (void)cls; (void)connection; (void)toe;
    if (con_cls && *con_cls) {
        cleanup_request_context((RequestContext *)*con_cls);
        *con_cls = NULL;
    }
}

static enum MHD_Result route_request(
    void *cls, struct MHD_Connection *connection,
    const char *url, const char *method, const char *version,
    const char *upload_data, size_t *upload_data_size, void **con_cls
) {
    RequestContext *ctx;
    (void)cls; (void)version;

    if (!con_cls || !upload_data_size || !method || !url) return MHD_NO;

    if (*con_cls == NULL) {
        ctx = (RequestContext *)calloc(1, sizeof(RequestContext));
        if (!ctx) return MHD_NO;
        ctx->is_post = (strcmp(method, "POST") == 0);
        *con_cls = ctx;
        return MHD_YES;
    }

    ctx = (RequestContext *)*con_cls;

    if (strcmp(method, "OPTIONS") == 0)
        return queue_no_content_response(connection);

    /* --- POST /pseudo --- */
    if (strcmp(method, "POST") == 0 && strcmp(url, "/pseudo") == 0) {
        if (*upload_data_size > 0) {
            size_t new_len = ctx->body_len + *upload_data_size;
            if (new_len > MAX_REQUEST_BODY_LEN)
                return queue_json_response(connection, MHD_HTTP_CONTENT_TOO_LARGE,
                    "{\"erreur\":\"Body trop volumineux\"}");
            char *new_body = (char *)realloc(ctx->body, new_len + 1);
            if (!new_body) return MHD_NO;
            ctx->body = new_body;
            memcpy(ctx->body + ctx->body_len, upload_data, *upload_data_size);
            ctx->body_len = new_len;
            ctx->body[ctx->body_len] = '\0';
            *upload_data_size = 0;
            return MHD_YES;
        }
        return gerer_post_pseudo(connection, ctx->body);
    }

    /* --- POST /recommandation --- */
    if (strcmp(method, "POST") == 0 && strcmp(url, "/recommandation") == 0) {
        if (*upload_data_size > 0) {
            size_t new_len = ctx->body_len + *upload_data_size;
            if (new_len > MAX_REQUEST_BODY_LEN)
                return queue_json_response(connection, MHD_HTTP_CONTENT_TOO_LARGE,
                    "{\"erreur\":\"Body trop volumineux\"}");
            char *new_body = (char *)realloc(ctx->body, new_len + 1);
            if (!new_body) return MHD_NO;
            ctx->body = new_body;
            memcpy(ctx->body + ctx->body_len, upload_data, *upload_data_size);
            ctx->body_len = new_len;
            ctx->body[ctx->body_len] = '\0';
            *upload_data_size = 0;
            return MHD_YES;
        }
        return gerer_post_recommandation(connection, ctx->body);
    }

    /* --- GET /historique --- */
    if (strcmp(method, "GET") == 0 && strcmp(url, "/historique") == 0)
        return gerer_get_historique(connection);

    /* --- GET /favoris --- */
    if (strcmp(method, "GET") == 0 && strcmp(url, "/favoris") == 0)
        return gerer_get_favoris(connection);

    /* --- POST /favoris --- */
    if (strcmp(method, "POST") == 0 && strcmp(url, "/favoris") == 0) {
        if (*upload_data_size > 0) {
            size_t new_len = ctx->body_len + *upload_data_size;
            if (new_len > MAX_REQUEST_BODY_LEN)
                return queue_json_response(connection, MHD_HTTP_CONTENT_TOO_LARGE,
                    "{\"erreur\":\"Body trop volumineux\"}");
            char *new_body = (char *)realloc(ctx->body, new_len + 1);
            if (!new_body) return MHD_NO;
            ctx->body = new_body;
            memcpy(ctx->body + ctx->body_len, upload_data, *upload_data_size);
            ctx->body_len = new_len;
            ctx->body[ctx->body_len] = '\0';
            *upload_data_size = 0;
            return MHD_YES;
        }
        return gerer_post_favori(connection, ctx->body);
    }

    /* --- DELETE /favoris --- */
    if (strcmp(method, "DELETE") == 0 && strcmp(url, "/favoris") == 0)
        return gerer_delete_favori(connection);

    return queue_json_response(connection, MHD_HTTP_NOT_FOUND, "{\"erreur\":\"Route introuvable\"}");
}

int demarrer_serveur(unsigned short port, const char *db_path) {
    if (!db_path || db_path[0] == '\0') return -1;
    if (snprintf(g_db_path, sizeof(g_db_path), "%s", db_path) < 0) return -1;
    if (init_db(g_db_path) != 0) { fprintf(stderr, "[server] init_db failed\n"); return -1; }

    g_daemon = MHD_start_daemon(
        MHD_USE_INTERNAL_POLLING_THREAD, port, NULL, NULL,
        &route_request, NULL,
        MHD_OPTION_NOTIFY_COMPLETED, &request_completed_callback, NULL,
        MHD_OPTION_END
    );

    if (!g_daemon) { fprintf(stderr, "[server] failed to start daemon\n"); return -1; }
    return 0;
}

void arreter_serveur(void) {
    if (g_daemon) { MHD_stop_daemon(g_daemon); g_daemon = NULL; }
}
