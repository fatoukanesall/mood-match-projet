#include "ai_client.h"

#include <assert.h>
#include <string.h>

static const char *json_film_only =
    "{"
    "\"films\":["
    "{"
    "\"titre\":\"Whiplash\"," 
    "\"auteur_ou_realisateur\":\"Damien Chazelle\"," 
    "\"annee\":2014," 
    "\"explication\":\"Tu vas retrouver du courage.\"," 
    "\"humeur_detectee\":\"Pression\"," 
    "\"ton\":\"intense motivant\"," 
    "\"duree_ou_pages\":\"1h47\""
    "}"
    "]"
    "}";

static const char *json_films_series =
    "{"
    "\"films\":["
    "{"
    "\"titre\":\"Whiplash\"," 
    "\"auteur_ou_realisateur\":\"Damien Chazelle\"," 
    "\"annee\":2014," 
    "\"explication\":\"Tu vas retrouver du courage.\"," 
    "\"humeur_detectee\":\"Pression\"," 
    "\"ton\":\"intense motivant\"," 
    "\"duree_ou_pages\":\"1h47\""
    "}"
    "],"
    "\"series\":["
    "{"
    "\"titre\":\"Chernobyl\"," 
    "\"auteur_ou_realisateur\":\"Craig Mazin\"," 
    "\"annee\":2019," 
    "\"explication\":\"Une tension qui te porte.\"," 
    "\"humeur_detectee\":\"Stress lucide\"," 
    "\"ton\":\"sombre prenant\"," 
    "\"duree_ou_pages\":\"1 saison, ~60 min/episode\""
    "}"
    "]"
    "}";

static const char *json_trois_categories =
    "{"
    "\"films\":["
    "{"
    "\"titre\":\"Whiplash\"," 
    "\"auteur_ou_realisateur\":\"Damien Chazelle\"," 
    "\"annee\":2014," 
    "\"explication\":\"Tu vas retrouver du courage.\"," 
    "\"humeur_detectee\":\"Pression\"," 
    "\"ton\":\"intense motivant\"," 
    "\"duree_ou_pages\":\"1h47\""
    "}"
    "],"
    "\"livres\":["
    "{"
    "\"titre\":\"L'Alchimiste\"," 
    "\"auteur_ou_realisateur\":\"Paulo Coelho\"," 
    "\"annee\":1988," 
    "\"explication\":\"Un souffle d'espoir.\"," 
    "\"humeur_detectee\":\"Quete de sens\"," 
    "\"ton\":\"doux inspirant\"," 
    "\"duree_ou_pages\":\"210 pages\""
    "}"
    "],"
    "\"series\":["
    "{"
    "\"titre\":\"Chernobyl\"," 
    "\"auteur_ou_realisateur\":\"Craig Mazin\"," 
    "\"annee\":2019," 
    "\"explication\":\"Une tension qui te porte.\"," 
    "\"humeur_detectee\":\"Stress lucide\"," 
    "\"ton\":\"sombre prenant\"," 
    "\"duree_ou_pages\":\"1 saison, ~60 min/episode\""
    "}"
    "]"
    "}";

int test_ai_parser_run(void) {
    ResultatRecherche resultat;

    init_resultat_recherche(&resultat);

    assert(parser_recommandations_json(json_film_only, "Je suis stresse", "film", &resultat) == 0);
    assert(resultat.nb_films == 1);
    assert(resultat.nb_livres == 0);
    assert(resultat.nb_series == 0);
    assert(strcmp(resultat.films[0].titre, "Whiplash") == 0);
    free_resultat_recherche(&resultat);

    init_resultat_recherche(&resultat);
    assert(parser_recommandations_json(json_films_series, "Je veux de la tension", "film,serie", &resultat) == 0);
    assert(resultat.nb_films == 1);
    assert(resultat.nb_livres == 0);
    assert(resultat.nb_series == 1);
    assert(strcmp(resultat.films[0].type, "film") == 0);
    assert(strcmp(resultat.series[0].type, "serie") == 0);
    free_resultat_recherche(&resultat);

    init_resultat_recherche(&resultat);
    assert(parser_recommandations_json(json_trois_categories, "Je veux tout", "film,livre,serie", &resultat) == 0);
    assert(resultat.nb_films == 1);
    assert(resultat.nb_livres == 1);
    assert(resultat.nb_series == 1);
    assert(strcmp(resultat.films[0].titre, "Whiplash") == 0);
    assert(strcmp(resultat.livres[0].titre, "L'Alchimiste") == 0);
    assert(strcmp(resultat.series[0].titre, "Chernobyl") == 0);
    free_resultat_recherche(&resultat);

    return 0;
}
