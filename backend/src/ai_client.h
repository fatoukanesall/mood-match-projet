#ifndef AI_CLIENT_H
#define AI_CLIENT_H

#include "models.h"

extern char ai_last_error[512];

int parser_recommandations_json(
	const char *json_payload,
	const char *humeur,
	const char *categories_demandees,
	ResultatRecherche *out_resultat
);

int obtenir_recommandation(const char *humeur, const char *categories_demandees, const char *mode, ResultatRecherche *out_resultat);

#endif
