#include "ai_client.h"

#include <cjson/cJSON.h>
#include <curl/curl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define GROQ_API_URL "https://api.groq.com/openai/v1/chat/completions"
#define GROQ_DEFAULT_MODEL "llama-3.3-70b-versatile"

char ai_last_error[512] = {0};

typedef struct {
    char *data;
    size_t len;
} HttpBuffer;

typedef struct {
    int film;
    int livre;
    int serie;
    int anime;
} CategoriesDemandees;

typedef struct {
    Recommandation **items;
    int *count;
    const char *type_label;
    const char *categorie_json;
} CategorieMapping;

static int copy_text_field(char *dest, size_t dest_size, const char *src) {
    int written;

    if (!dest || dest_size == 0) {
        return -1;
    }

    if (!src) {
        dest[0] = '\0';
        return 0;
    }

    written = snprintf(dest, dest_size, "%s", src);
    if (written < 0 || (size_t)written >= dest_size) {
        return -1;
    }

    return 0;
}

static size_t write_callback(char *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    HttpBuffer *mem = (HttpBuffer *)userp;
    char *new_data;

    new_data = (char *)realloc(mem->data, mem->len + realsize + 1);
    if (!new_data) {
        return 0;
    }

    mem->data = new_data;
    memcpy(&(mem->data[mem->len]), contents, realsize);
    mem->len += realsize;
    mem->data[mem->len] = '\0';

    return realsize;
}

static void trim_spaces(char *text) {
    char *start;
    char *end;
    size_t len;

    if (!text) {
        return;
    }

    start = text;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') {
        start++;
    }

    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }

    len = strlen(text);
    while (len > 0) {
        end = text + len - 1;
        if (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') {
            *end = '\0';
            len--;
        } else {
            break;
        }
    }
}

static CategoriesDemandees parser_categories(const char *categories_demandees) {
    CategoriesDemandees categories = {0};
    char buffer[128];
    char *token;

    if (!categories_demandees) {
        return categories;
    }

    if (snprintf(buffer, sizeof(buffer), "%s", categories_demandees) < 0) {
        return categories;
    }

    token = strtok(buffer, ",");
    while (token) {
        trim_spaces(token);
        if (strcmp(token, "film") == 0) {
            categories.film = 1;
        } else if (strcmp(token, "livre") == 0) {
            categories.livre = 1;
        } else if (strcmp(token, "serie") == 0) {
            categories.serie = 1;
        } else if (strcmp(token, "anime") == 0) {
            categories.anime = 1;
        }
        token = strtok(NULL, ",");
    }

    return categories;
}

static int categories_validees(const CategoriesDemandees *categories) {
    if (!categories) {
        return 0;
    }
    return categories->film || categories->livre || categories->serie || categories->anime;
}

static const char *type_from_category_label(const char *label) {
    if (!label) {
        return "";
    }
    if (strcmp(label, "films") == 0) {
        return "film";
    }
    if (strcmp(label, "livres") == 0) {
        return "livre";
    }
    if (strcmp(label, "series") == 0) {
        return "serie";
    }
    if (strcmp(label, "animes") == 0) {
        return "anime";
    }
    return "";
}

static int requested_for_index(const CategoriesDemandees *categories, int index) {
    if (!categories) {
        return 0;
    }
    switch (index) {
        case 0:
            return categories->film;
        case 1:
            return categories->livre;
        case 2:
            return categories->serie;
        case 3:
            return categories->anime;
        default:
            return 0;
    }
}


static int extraire_payload_json(const char *api_response_json, char **out_payload_json) {
    cJSON *root = NULL;
    cJSON *candidates = NULL;
    cJSON *first_candidate = NULL;
    cJSON *content = NULL;
    cJSON *parts = NULL;
    cJSON *first_part = NULL;
    cJSON *text = NULL;
    cJSON *films = NULL;
    cJSON *livres = NULL;
    cJSON *series = NULL;
    cJSON *animes = NULL;

    if (!api_response_json || !out_payload_json) {
        return -1;
    }

    *out_payload_json = NULL;
    root = cJSON_Parse(api_response_json);
    if (!root) {
        fprintf(stderr, "[ai] JSON response parse error\n");
        return -1;
    }

    films = cJSON_GetObjectItemCaseSensitive(root, "films");
    livres = cJSON_GetObjectItemCaseSensitive(root, "livres");
    series = cJSON_GetObjectItemCaseSensitive(root, "series");
    animes = cJSON_GetObjectItemCaseSensitive(root, "animes");
    if (cJSON_IsArray(films) || cJSON_IsArray(livres) || cJSON_IsArray(series) || cJSON_IsArray(animes)) {
        *out_payload_json = (char *)malloc(strlen(api_response_json) + 1);
        if (!*out_payload_json) {
            cJSON_Delete(root);
            return -1;
        }
        strcpy(*out_payload_json, api_response_json);
        cJSON_Delete(root);
        return 0;
    }

    /* Groq format: choices[0].message.content */
    {
        cJSON *choices = cJSON_GetObjectItemCaseSensitive(root, "choices");
        if (cJSON_IsArray(choices) && cJSON_GetArraySize(choices) >= 1) {
            cJSON *first_choice = cJSON_GetArrayItem(choices, 0);
            cJSON *msg = first_choice ? cJSON_GetObjectItemCaseSensitive(first_choice, "message") : NULL;
            cJSON *msg_content = msg ? cJSON_GetObjectItemCaseSensitive(msg, "content") : NULL;
            if (cJSON_IsString(msg_content) && msg_content->valuestring) {
                *out_payload_json = (char *)malloc(strlen(msg_content->valuestring) + 1);
                if (!*out_payload_json) {
                    cJSON_Delete(root);
                    return -1;
                }
                strcpy(*out_payload_json, msg_content->valuestring);
                cJSON_Delete(root);
                return 0;
            }
        }
    }

    candidates = cJSON_GetObjectItemCaseSensitive(root, "candidates");
    if (!cJSON_IsArray(candidates) || cJSON_GetArraySize(candidates) < 1) {
        cJSON_Delete(root);
        fprintf(stderr, "[ai] API response without choices or candidates\n");
        return -1;
    }

    first_candidate = cJSON_GetArrayItem(candidates, 0);
    content = cJSON_GetObjectItemCaseSensitive(first_candidate, "content");
    parts = content ? cJSON_GetObjectItemCaseSensitive(content, "parts") : NULL;
    first_part = (parts && cJSON_IsArray(parts) && cJSON_GetArraySize(parts) > 0) ? cJSON_GetArrayItem(parts, 0) : NULL;
    text = first_part ? cJSON_GetObjectItemCaseSensitive(first_part, "text") : NULL;

    if (!cJSON_IsString(text) || !text->valuestring) {
        cJSON_Delete(root);
        fprintf(stderr, "[ai] Gemini response missing candidates[0].content.parts[0].text\n");
        return -1;
    }

    *out_payload_json = (char *)malloc(strlen(text->valuestring) + 1);
    if (!*out_payload_json) {
        cJSON_Delete(root);
        return -1;
    }

    strcpy(*out_payload_json, text->valuestring);
    cJSON_Delete(root);
    return 0;
}

static int parser_categorie_json(
    cJSON *root,
    const char *humeur,
    const char *categorie_label,
    int categorie_demandee,
    Recommandation **out_items,
    int *out_count
) {
    cJSON *array = NULL;
    int count = 0;
    int i;
    Recommandation *items = NULL;
    const char *type_label;

    if (!root || !humeur || !categorie_label || !out_items || !out_count) {
        return -1;
    }

    *out_items = NULL;
    *out_count = 0;

    array = cJSON_GetObjectItemCaseSensitive(root, categorie_label);
    if (!array) {
        if (categorie_demandee) {
            fprintf(stderr, "[ai] category '%s' requested but missing\n", categorie_label);
            return -1;
        }
        return 0;
    }

    if (!cJSON_IsArray(array)) {
        fprintf(stderr, "[ai] category '%s' is not an array\n", categorie_label);
        return -1;
    }

    count = cJSON_GetArraySize(array);
    if (count <= 0) {
        *out_items = NULL;
        *out_count = 0;
        return 0;
    }

    items = (Recommandation *)calloc((size_t)count, sizeof(Recommandation));
    if (!items) {
        return -1;
    }

    type_label = type_from_category_label(categorie_label);
    for (i = 0; i < count; i++) {
        cJSON *obj = cJSON_GetArrayItem(array, i);
        cJSON *titre = NULL;
        cJSON *auteur = NULL;
        cJSON *annee = NULL;
        cJSON *explication = NULL;
        cJSON *humeur_detectee = NULL;
        cJSON *ton = NULL;
        cJSON *duree = NULL;
        Recommandation *item = &items[i];

        if (!cJSON_IsObject(obj)) {
            free(items);
            return -1;
        }

        titre = cJSON_GetObjectItemCaseSensitive(obj, "titre");
        auteur = cJSON_GetObjectItemCaseSensitive(obj, "auteur_ou_realisateur");
        annee = cJSON_GetObjectItemCaseSensitive(obj, "annee");
        explication = cJSON_GetObjectItemCaseSensitive(obj, "explication");
        humeur_detectee = cJSON_GetObjectItemCaseSensitive(obj, "humeur_detectee");
        ton = cJSON_GetObjectItemCaseSensitive(obj, "ton");
        duree = cJSON_GetObjectItemCaseSensitive(obj, "duree_ou_pages");

        if (!cJSON_IsString(titre) || !cJSON_IsString(auteur) || !cJSON_IsNumber(annee) ||
            !cJSON_IsString(explication) || !cJSON_IsString(humeur_detectee) || !cJSON_IsString(ton) ||
            !cJSON_IsString(duree)) {
            free(items);
            return -1;
        }

        memset(item, 0, sizeof(*item));
        if (copy_text_field(item->humeur_texte, sizeof(item->humeur_texte), humeur) != 0 ||
            copy_text_field(item->type, sizeof(item->type), type_label) != 0 ||
            copy_text_field(item->titre, sizeof(item->titre), titre->valuestring) != 0 ||
            copy_text_field(item->auteur_ou_realisateur, sizeof(item->auteur_ou_realisateur), auteur->valuestring) != 0 ||
            copy_text_field(item->explication, sizeof(item->explication), explication->valuestring) != 0 ||
            copy_text_field(item->humeur_detectee, sizeof(item->humeur_detectee), humeur_detectee->valuestring) != 0 ||
            copy_text_field(item->ton, sizeof(item->ton), ton->valuestring) != 0 ||
            copy_text_field(item->duree_ou_pages, sizeof(item->duree_ou_pages), duree->valuestring) != 0) {
            free(items);
            return -1;
        }

        item->id = 0;
        item->annee = annee->valueint;
    }

    *out_items = items;
    *out_count = count;
    return 0;
}

static void build_prompt(const char *humeur,
                         const char *type_demande,
                         const char *mode,
                         char *out,
                         size_t out_size) {

  srand((unsigned int)time(NULL) ^
        (unsigned int)getpid() ^
        (unsigned int)(size_t)out);

  int seed     = rand() % 999983;
  int pays_idx = seed % 5;
  int epoque   = seed % 4;

  const char *pays_liste[] = {
    "France, Belgique, Suisse, Espagne, Italie",
    "Etats-Unis, Royaume-Uni, Canada, Australie",
    "Japon, Coree du Sud, Chine, Taiwan",
    "Senegal, Nigeria, Afrique du Sud, Maroc",
    "Bresil, Mexique, Inde, Turquie, Argentine"
  };

  const char *epoques_liste[] = {
    "oeuvres d avant 1990",
    "oeuvres entre 1990 et 2005",
    "oeuvres entre 2005 et 2015",
    "oeuvres recentes entre 2015 et 2024"
  };

  /* CORRECTION CRITIQUE : construire le prompt
     en 2 étapes pour éviter les erreurs
     de correspondance des arguments */

  char intro[2048];
  snprintf(intro, sizeof(intro),
    "Tu es Fatou Sall, experte culturelle "
    "chaleureuse et directe.\n\n"
    "SEED UNIQUE : %d\n"
    "PAYS POUR CET APPEL : %s\n"
    "EPOQUE POUR CET APPEL : %s\n\n"
    "HUMEUR EXACTE : \"%s\"\n"
    "MODE SOUHAITE : \"%s\"\n"
    "TYPES DEMANDES : \"%s\"\n\n",
    seed,
    pays_liste[pays_idx],
    epoques_liste[epoque],
    humeur,
    mode,
    type_demande
  );

  char regles[4096];
  snprintf(regles, sizeof(regles),
    "=== INSTRUCTION PRINCIPALE ===\n"
    "Lis et comprends l humeur : \"%s\"\n"
    "Tu dois DEVINER l emotion meme si "
    "les mots exacts ne correspondent pas.\n"
    "Exemples de comprehension :\n"
    "- 'epuise par soutenance' = stress + "
    "besoin de courage\n"
    "- 'coeur brise' = chagrin amour\n"
    "- 'envie de rien' = depression legere\n"
    "- 'journee de merde' = frustration\n"
    "- 'je kiffe la vie' = bonheur\n"
    "- 'nul en cours' = manque confiance\n"
    "- 'personne me comprend' = solitude\n"
    "- 'dispute avec famille' = tension\n\n"

    "=== ADAPTATION PAR HUMEUR ===\n"
    "Stress/soutenance/examen/pression :\n"
    "  films/series/livres/animes sur le "
    "depassement de soi et la victoire\n"
    "Triste/deprime/vide/melancolique :\n"
    "  oeuvres consolantes ou pour pleurer\n"
    "Amour/rupture/coeur/manque :\n"
    "  oeuvres romantiques ou sur guerison\n"
    "Colere/frustre/injustice :\n"
    "  oeuvres intenses ou comiques\n"
    "Malade/fatigue/burnout :\n"
    "  oeuvres douces, legeres, reposantes\n"
    "Heureux/euphorique/fier :\n"
    "  oeuvres feel-good et celebrantes\n"
    "Confiance/timide/peur public :\n"
    "  biopics inspirants et histoires\n"
    "  de depassement de soi\n"
    "Solitude/isolement :\n"
    "  oeuvres sur la connexion humaine\n"
    "Apprendre/curiosite/culture :\n"
    "  documentaires, essais, biopics\n"
    "Famille/amis/relations :\n"
    "  oeuvres sur les liens humains\n\n"

    "=== ANIMES PAR HUMEUR ===\n"
    "Stress/soutenance :\n"
    "  Haikyuu, Your Lie in April,\n"
    "  Spirited Away, Shokugeki no Soma\n"
    "Triste/deprime :\n"
    "  A Silent Voice, Clannad,\n"
    "  Grave of the Fireflies, Wolf Children\n"
    "Heureux/joie :\n"
    "  Totoro, One Piece, Kiki Delivery\n"
    "Amour/rupture :\n"
    "  Your Name, Weathering with You,\n"
    "  Toradora, Clannad After Story\n"
    "Confiance/timide :\n"
    "  Haikyuu, My Hero Academia,\n"
    "  Whisper of the Heart\n"
    "Malade/fatigue :\n"
    "  Totoro, Mushishi, Natsume Yuujinchou\n"
    "Colere/frustre :\n"
    "  FMA Brotherhood, Demon Slayer,\n"
    "  Attack on Titan\n"
    "Apprendre/culture :\n"
    "  Dr Stone, Cells at Work,\n"
    "  Princess Kaguya\n\n"

    "=== REGLES STRICTES ===\n"
    "1. Exactement 3 oeuvres par type\n"
    "2. JAMAIS Whiplash deux fois de suite\n"
    "3. JAMAIS L Alchimiste deux fois\n"
    "4. Varies les pays : au moins 2 pays "
    "differents dans la liste\n"
    "5. Explication OBLIGATOIREMENT liee "
    "a l humeur donnee\n"
    "6. Tutoiement partout\n\n",
    humeur
  );

  char format_json[1024];
  snprintf(format_json, sizeof(format_json),
    "=== REPONSE JSON UNIQUEMENT ===\n"
    "Zero markdown. Zero texte avant/apres.\n"
    "{\n"
    "  \"message_ami\": \"message SPECIFIQUE "
    "pour humeur: %s. 2 phrases. Tutoiement.\",\n"
    "  \"humeur_analysee\": \"3 mots max\",\n"
    "  \"recommandations\": [\n"
    "    {\n"
    "      \"type_oeuvre\": \"film|serie|"
    "livre|anime\",\n"
    "      \"titre\": \"titre reel\",\n"
    "      \"auteur_ou_realisateur\": \"nom\",\n"
    "      \"annee\": 2020,\n"
    "      \"pays\": \"pays\",\n"
    "      \"genre\": \"genre\",\n"
    "      \"explication\": \"lien direct "
    "avec humeur\",\n"
    "      \"raison_precise\": \"Parce que\",\n"
    "      \"humeur_detectee\": \"emotion\",\n"
    "      \"ton\": \"2-3 mots\",\n"
    "      \"duree_ou_pages\": \"duree\",\n"
    "      \"fun_fact\": \"fait surprenant\",\n"
    "      \"si_tu_aimes\": \"essaie aussi\",\n"
    "      \"niveau_intensite\": \"legere\"\n"
    "    }\n"
    "  ]\n"
    "}\n",
    humeur
  );

  /* Assemblage final */
  snprintf(out, out_size, "%s%s%s",
           intro, regles, format_json);
}

static int parser_resultat_grouped_json(
    const char *json_payload,
    const char *humeur,
    const CategoriesDemandees *categories,
    ResultatRecherche *out_resultat
) {
    cJSON *root = NULL;
    CategorieMapping mappings[] = {
        { &out_resultat->films, &out_resultat->nb_films, "film", "films" },
        { &out_resultat->livres, &out_resultat->nb_livres, "livre", "livres" },
        { &out_resultat->series, &out_resultat->nb_series, "serie", "series" },
        { &out_resultat->animes, &out_resultat->nb_animes, "anime", "animes" }
    };
    int i;

    if (!json_payload || !humeur || !categories || !out_resultat) {
        return -1;
    }

    init_resultat_recherche(out_resultat);

    root = cJSON_Parse(json_payload);
    if (!root) {
        fprintf(stderr, "[ai] invalid grouped JSON payload\n");
        return -1;
    }

    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return -1;
    }

    for (i = 0; i < (int)(sizeof(mappings) / sizeof(mappings[0])); i++) {
        cJSON *array = cJSON_GetObjectItemCaseSensitive(root, mappings[i].categorie_json);
        int requested = requested_for_index(categories, i);
        Recommandation *items = NULL;
        int count = 0;

        if (!array) {
            if (requested) {
                fprintf(stderr, "[ai] missing requested category '%s'\n", mappings[i].categorie_json);
                cJSON_Delete(root);
                free_resultat_recherche(out_resultat);
                return -1;
            }
            continue;
        }

        if (!cJSON_IsArray(array)) {
            fprintf(stderr, "[ai] category '%s' is not an array\n", mappings[i].categorie_json);
            cJSON_Delete(root);
            free_resultat_recherche(out_resultat);
            return -1;
        }

        if (parser_categorie_json(root, humeur, mappings[i].categorie_json, requested, &items, &count) != 0) {
            cJSON_Delete(root);
            free_resultat_recherche(out_resultat);
            return -1;
        }

        *mappings[i].items = items;
        *mappings[i].count = count;
    }

    cJSON_Delete(root);
    return 0;
}

int parser_recommandations_json(
    const char *json_payload,
    const char *humeur,
    const char *categories_demandees,
    ResultatRecherche *out_resultat
) {
    cJSON *root = NULL;
    cJSON *reco_array = NULL;
    int i;
    CategoriesDemandees categories;

    if (!json_payload || !humeur || !categories_demandees || !out_resultat) {
        return -1;
    }

    categories = parser_categories(categories_demandees);
    if (!categories_validees(&categories)) {
        return -1;
    }

    init_resultat_recherche(out_resultat);

    root = cJSON_Parse(json_payload);
    if (!root) {
        fprintf(stderr, "[ai] parser_recommandations_json: invalid JSON payload\n");
        return -1;
    }

    /* Try new schema: root.humeur_analysee, root.message_ami, root.recommandations[] */
    cJSON *humeur_analysee = cJSON_GetObjectItemCaseSensitive(root, "humeur_analysee");
    cJSON *message_ami = cJSON_GetObjectItemCaseSensitive(root, "message_ami");
    if (cJSON_IsString(humeur_analysee) && humeur_analysee->valuestring) {
        copy_text_field(out_resultat->humeur_analysee, sizeof(out_resultat->humeur_analysee), humeur_analysee->valuestring);
    }
    if (cJSON_IsString(message_ami) && message_ami->valuestring) {
        copy_text_field(out_resultat->message_ami, sizeof(out_resultat->message_ami), message_ami->valuestring);
    }

    reco_array = cJSON_GetObjectItemCaseSensitive(root, "recommandations");
    if (cJSON_IsArray(reco_array)) {
        int total = cJSON_GetArraySize(reco_array);
        /* First pass: counts */
        int cnt_f = 0, cnt_l = 0, cnt_s = 0, cnt_a = 0;
        for (i = 0; i < total; i++) {
            cJSON *obj = cJSON_GetArrayItem(reco_array, i);
            if (!cJSON_IsObject(obj)) continue;
            cJSON *type = cJSON_GetObjectItemCaseSensitive(obj, "type_oeuvre");
            const char *t = cJSON_IsString(type) && type->valuestring ? type->valuestring : NULL;
            if (!t) {
                cJSON *t2 = cJSON_GetObjectItemCaseSensitive(obj, "type");
                t = cJSON_IsString(t2) && t2->valuestring ? t2->valuestring : NULL;
            }
            if (!t) continue;
            if (strcmp(t, "film") == 0) cnt_f++;
            else if (strcmp(t, "livre") == 0) cnt_l++;
            else if (strcmp(t, "serie") == 0) cnt_s++;
            else if (strcmp(t, "anime") == 0) cnt_a++;
        }

        /* Cap at 3 per type */
        if (cnt_f > 3) cnt_f = 3;
        if (cnt_l > 3) cnt_l = 3;
        if (cnt_s > 3) cnt_s = 3;
        if (cnt_a > 3) cnt_a = 3;

        if (cnt_f > 0) out_resultat->films = (Recommandation *)calloc((size_t)cnt_f, sizeof(Recommandation));
        if (cnt_l > 0) out_resultat->livres = (Recommandation *)calloc((size_t)cnt_l, sizeof(Recommandation));
        if (cnt_s > 0) out_resultat->series = (Recommandation *)calloc((size_t)cnt_s, sizeof(Recommandation));
        if (cnt_a > 0) out_resultat->animes = (Recommandation *)calloc((size_t)cnt_a, sizeof(Recommandation));
        out_resultat->nb_films = cnt_f;
        out_resultat->nb_livres = cnt_l;
        out_resultat->nb_series = cnt_s;
        out_resultat->nb_animes = cnt_a;

        /* Second pass: fill arrays */
        int idx_f = 0, idx_l = 0, idx_s = 0, idx_a = 0;
        for (i = 0; i < total; i++) {
            cJSON *obj = cJSON_GetArrayItem(reco_array, i);
            if (!cJSON_IsObject(obj)) continue;
            cJSON *type = cJSON_GetObjectItemCaseSensitive(obj, "type_oeuvre");
            const char *t = cJSON_IsString(type) && type->valuestring ? type->valuestring : NULL;
            if (!t) {
                cJSON *t2 = cJSON_GetObjectItemCaseSensitive(obj, "type");
                t = cJSON_IsString(t2) && t2->valuestring ? t2->valuestring : NULL;
            }
            if (!t) continue;

            Recommandation tmp;
            memset(&tmp, 0, sizeof(tmp));

            cJSON *titre = cJSON_GetObjectItemCaseSensitive(obj, "titre");
            cJSON *auteur = cJSON_GetObjectItemCaseSensitive(obj, "auteur_ou_realisateur");
            cJSON *annee = cJSON_GetObjectItemCaseSensitive(obj, "annee");
            cJSON *explication = cJSON_GetObjectItemCaseSensitive(obj, "explication");
            cJSON *h_detect = cJSON_GetObjectItemCaseSensitive(obj, "humeur_detectee");
            cJSON *ton = cJSON_GetObjectItemCaseSensitive(obj, "ton");
            cJSON *duree = cJSON_GetObjectItemCaseSensitive(obj, "duree_ou_pages");
            cJSON *pays = cJSON_GetObjectItemCaseSensitive(obj, "pays");
            cJSON *genre = cJSON_GetObjectItemCaseSensitive(obj, "genre");
            cJSON *sous_genre = cJSON_GetObjectItemCaseSensitive(obj, "sous_genre");
            cJSON *raison = cJSON_GetObjectItemCaseSensitive(obj, "raison_precise");
            cJSON *fun = cJSON_GetObjectItemCaseSensitive(obj, "fun_fact");
            cJSON *si = cJSON_GetObjectItemCaseSensitive(obj, "si_tu_aimes");
            cJSON *niv = cJSON_GetObjectItemCaseSensitive(obj, "niveau_intensite");

            if (cJSON_IsString(titre) && titre->valuestring) copy_text_field(tmp.titre, sizeof(tmp.titre), titre->valuestring);
            if (cJSON_IsString(auteur) && auteur->valuestring) copy_text_field(tmp.auteur_ou_realisateur, sizeof(tmp.auteur_ou_realisateur), auteur->valuestring);
            if (cJSON_IsNumber(annee)) tmp.annee = annee->valueint;
            if (cJSON_IsString(explication) && explication->valuestring) copy_text_field(tmp.explication, sizeof(tmp.explication), explication->valuestring);
            if (cJSON_IsString(h_detect) && h_detect->valuestring) copy_text_field(tmp.humeur_detectee, sizeof(tmp.humeur_detectee), h_detect->valuestring);
            if (cJSON_IsString(ton) && ton->valuestring) copy_text_field(tmp.ton, sizeof(tmp.ton), ton->valuestring);
            if (cJSON_IsString(duree) && duree->valuestring) copy_text_field(tmp.duree_ou_pages, sizeof(tmp.duree_ou_pages), duree->valuestring);
            if (cJSON_IsString(pays) && pays->valuestring) copy_text_field(tmp.pays, sizeof(tmp.pays), pays->valuestring);
            if (cJSON_IsString(genre) && genre->valuestring) copy_text_field(tmp.genre, sizeof(tmp.genre), genre->valuestring);
            if (cJSON_IsString(sous_genre) && sous_genre->valuestring) copy_text_field(tmp.sous_genre, sizeof(tmp.sous_genre), sous_genre->valuestring);
            if (cJSON_IsString(raison) && raison->valuestring) copy_text_field(tmp.raison_precise, sizeof(tmp.raison_precise), raison->valuestring);
            if (cJSON_IsString(fun) && fun->valuestring) copy_text_field(tmp.fun_fact, sizeof(tmp.fun_fact), fun->valuestring);
            if (cJSON_IsString(si) && si->valuestring) copy_text_field(tmp.si_tu_aimes, sizeof(tmp.si_tu_aimes), si->valuestring);
            if (cJSON_IsString(niv) && niv->valuestring) copy_text_field(tmp.niveau_intensite, sizeof(tmp.niveau_intensite), niv->valuestring);

            copy_text_field(tmp.type, sizeof(tmp.type), t);
            copy_text_field(tmp.humeur_texte, sizeof(tmp.humeur_texte), humeur);
            if (out_resultat->message_ami[0]) copy_text_field(tmp.message_ami, sizeof(tmp.message_ami), out_resultat->message_ami);
            if (out_resultat->humeur_analysee[0]) copy_text_field(tmp.humeur_analysee, sizeof(tmp.humeur_analysee), out_resultat->humeur_analysee);

            if (strcmp(t, "film") == 0) {
                if (idx_f < out_resultat->nb_films) out_resultat->films[idx_f++] = tmp;
            } else if (strcmp(t, "livre") == 0) {
                if (idx_l < out_resultat->nb_livres) out_resultat->livres[idx_l++] = tmp;
            } else if (strcmp(t, "serie") == 0) {
                if (idx_s < out_resultat->nb_series) out_resultat->series[idx_s++] = tmp;
            } else if (strcmp(t, "anime") == 0) {
                if (idx_a < out_resultat->nb_animes) out_resultat->animes[idx_a++] = tmp;
            }
        }

        cJSON_Delete(root);
        return 0;
    }

    /* Fallback: try grouped payload as before */
    cJSON_Delete(root);
    return parser_resultat_grouped_json(json_payload, humeur, &categories, out_resultat);
}

int obtenir_recommandation(const char *humeur, const char *categories_demandees, const char *mode, ResultatRecherche *out_resultat) {
    const char *api_key = getenv("GROQ_API_KEY");
    const char *model = getenv("GROQ_MODEL");
    char auth_header[512];
    CategoriesDemandees categories;
    CURL *curl = NULL;
    struct curl_slist *headers = NULL;
    HttpBuffer response = {0};
    char *prompt = NULL;
    char *endpoint = NULL;
    char *request_body = NULL;
    char *payload_json = NULL;
    cJSON *request_root = NULL;
    cJSON *contents = NULL;
    cJSON *content_obj = NULL;
    cJSON *parts = NULL;
    cJSON *part_text = NULL;
    CURLcode curl_rc;
    long http_code = 0;
    int rc = -1;

    if (!humeur || !categories_demandees || !out_resultat) {
        return -1;
    }

    if (!api_key || api_key[0] == '\0') {
        fprintf(stderr, "[ai] GROQ_API_KEY is not set\n");
        snprintf(ai_last_error, sizeof(ai_last_error), "Variable GROQ_API_KEY manquante. Insere-la dans config.bat et relance run.bat.");
        return -1;
    }

    if (!model || model[0] == '\0') {
        model = GROQ_DEFAULT_MODEL;
    }

    categories = parser_categories(categories_demandees);
    if (!categories_validees(&categories)) {
        return -1;
    }

    prompt = (char *)malloc(32768);
    if (!prompt) {
        goto cleanup;
    }

    build_prompt(humeur, categories_demandees, mode, prompt, 32768);

    request_root = cJSON_CreateObject();
    contents = cJSON_CreateArray();
    content_obj = cJSON_CreateObject();

    if (!request_root || !contents || !content_obj) {
        goto cleanup;
    }

    cJSON_AddStringToObject(request_root, "model", model);
    cJSON_AddNumberToObject(request_root, "temperature", 0.7);
    cJSON_AddNumberToObject(request_root, "max_tokens", 4096);

    cJSON_AddStringToObject(content_obj, "role", "user");
    cJSON_AddStringToObject(content_obj, "content", prompt);
    cJSON_AddItemToArray(contents, content_obj);
    content_obj = NULL;

    cJSON_AddItemToObject(request_root, "messages", contents);
    contents = NULL;

    request_body = cJSON_PrintUnformatted(request_root);
    if (!request_body) {
        goto cleanup;
    }

    endpoint = (char *)malloc(strlen(GROQ_API_URL) + 1);
    if (!endpoint) {
        goto cleanup;
    }
    strcpy(endpoint, GROQ_API_URL);

    curl = curl_easy_init();
    if (!curl) {
        goto cleanup;
    }

    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL, endpoint);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 25L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

    curl_rc = curl_easy_perform(curl);
    if (curl_rc != CURLE_OK) {
        fprintf(stderr, "[ai] curl_easy_perform failed: %s\n", curl_easy_strerror(curl_rc));
        snprintf(ai_last_error, sizeof(ai_last_error), "Connexion a Groq impossible: %s", curl_easy_strerror(curl_rc));
        goto cleanup;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code < 200 || http_code >= 300) {
        fprintf(stderr, "[ai] Gemini API HTTP error: %ld\n", http_code);
        if (response.data) {
            fprintf(stderr, "[ai] Response: %s\n", response.data);
        }
        if (http_code == 429) {
            snprintf(ai_last_error, sizeof(ai_last_error), "Quota Groq epuise (HTTP 429). Attends quelques secondes et reessaie.");
        } else if (http_code == 401 || http_code == 403) {
            snprintf(ai_last_error, sizeof(ai_last_error), "Cle API invalide ou acces refuse (HTTP %ld). Verifie GROQ_API_KEY dans config.bat.", http_code);
        } else if (http_code == 404) {
            snprintf(ai_last_error, sizeof(ai_last_error), "Modele Groq introuvable (HTTP 404). Verifie GROQ_MODEL (utilise llama-3.3-70b-versatile).");
        } else {
            snprintf(ai_last_error, sizeof(ai_last_error), "Erreur HTTP Groq: %ld.", http_code);
        }
        goto cleanup;
    }

    if (!response.data || response.len == 0) {
        fprintf(stderr, "[ai] empty response from Gemini API\n");
        snprintf(ai_last_error, sizeof(ai_last_error), "Gemini a retourne une reponse vide.");
        goto cleanup;
    }

    if (extraire_payload_json(response.data, &payload_json) != 0) {
        snprintf(ai_last_error, sizeof(ai_last_error), "Impossible d extraire le JSON de la reponse Gemini. Le modele n a peut-etre pas suivi le format demande.");
        goto cleanup;
    }

    if (parser_recommandations_json(payload_json, humeur, categories_demandees, out_resultat) != 0) {
        snprintf(ai_last_error, sizeof(ai_last_error), "Impossible de parser les recommandations retournees par Gemini.");
        goto cleanup;
    }

    rc = 0;

cleanup:
    if (rc != 0) {
        free_resultat_recherche(out_resultat);
    }
    free(prompt);
    free(endpoint);
    free(payload_json);
    free(response.data);
    if (headers) {
        curl_slist_free_all(headers);
    }
    if (curl) {
        curl_easy_cleanup(curl);
    }
    cJSON_free(request_body);
    cJSON_Delete(request_root);
    cJSON_Delete(contents);
    cJSON_Delete(content_obj);
    cJSON_Delete(parts);
    cJSON_Delete(part_text);

    return rc;
}
