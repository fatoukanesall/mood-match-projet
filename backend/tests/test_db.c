#include "db.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void remplir_rec(
    Recommandation *rec,
    const char *humeur,
    const char *type,
    const char *titre,
    const char *auteur,
    int annee,
    const char *explication,
    const char *humeur_detectee,
    const char *ton,
    const char *duree,
    const char *date_creation
) {
    memset(rec, 0, sizeof(*rec));
    snprintf(rec->humeur_texte, sizeof(rec->humeur_texte), "%s", humeur);
    snprintf(rec->type, sizeof(rec->type), "%s", type);
    snprintf(rec->titre, sizeof(rec->titre), "%s", titre);
    snprintf(rec->auteur_ou_realisateur, sizeof(rec->auteur_ou_realisateur), "%s", auteur);
    rec->annee = annee;
    snprintf(rec->explication, sizeof(rec->explication), "%s", explication);
    snprintf(rec->humeur_detectee, sizeof(rec->humeur_detectee), "%s", humeur_detectee);
    snprintf(rec->ton, sizeof(rec->ton), "%s", ton);
    snprintf(rec->duree_ou_pages, sizeof(rec->duree_ou_pages), "%s", duree);
    snprintf(rec->date_creation, sizeof(rec->date_creation), "%s", date_creation);
}

int test_db_run(void) {
    const char *db_path = "./test_moodmatch.db";
    Recommandation rec1;
    Recommandation rec2;
    RecommandationList list = {0};

    remove(db_path);

    assert(init_db(db_path) == 0);

    remplir_rec(
        &rec1,
        "Je suis fatigue",
        "film",
        "Whiplash",
        "Damien Chazelle",
        2014,
        "Une dose d'intensite.",
        "Pression",
        "Intense motivant",
        "1h47",
        "2026-06-20T10:00:00Z"
    );
    assert(inserer_recommandation(db_path, &rec1) == 0);

    remplir_rec(
        &rec2,
        "Je veux ralentir",
        "livre",
        "L'Alchimiste",
        "Paulo Coelho",
        1988,
        "Un souffle d'espoir.",
        "Quete de sens",
        "Doux inspirant",
        "210 pages",
        "2026-06-20T12:00:00Z"
    );
    assert(inserer_recommandation(db_path, &rec2) == 0);

    assert(lister_historique(db_path, NULL, &list) == 0);
    assert(list.count == 2);

    /* Doit etre trie par date_creation DESC */
    assert(strcmp(list.items[0].titre, "L'Alchimiste") == 0);
    assert(strcmp(list.items[1].titre, "Whiplash") == 0);

    free_recommandation_list(&list);
    remove(db_path);
    return 0;
}
