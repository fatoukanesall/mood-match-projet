# Architecture MoodMatch

## Vue d'ensemble

MoodMatch est une application web composée de :

- un frontend statique (`frontend/`) en HTML/CSS/JS,
- un backend HTTP en C (`backend/src/`),
- une base SQLite locale (développement) / Supabase (production),
- une intégration IA via l'API Groq (LLaMA 3.3-70b-versatile).

## Diagramme logique

```
[Navigateur utilisateur]
        |
        | fetch HTTP/JSON
        v
[Frontend statique — Vercel]
        |
        | POST /pseudo, POST /recommandation
        | GET /historique, GET /favoris
        | POST /favoris, DELETE /favoris
        v
[Backend C — libmicrohttpd — Render]
        |                          \
        v                           v HTTPS REST
[SQLite local / Supabase prod]   [Groq API — LLaMA 3.3-70b]
```

## Composants backend

### `main.c`

- Lit la configuration via variables d'environnement (`GROQ_API_KEY`, `PORT`, `DB_PATH`)
- Démarre le serveur HTTP
- Gère l'arrêt propre (SIGINT/SIGTERM)

### `server.c` / `server.h`

- Expose les routes HTTP (voir tableau ci-dessous)
- Applique les headers CORS (`Access-Control-Allow-Origin: *`)
- Parse les requêtes JSON entrantes
- Orchestre appels IA + persistance DB
- Renvoie les réponses JSON

| Méthode | Route | Rôle |
|---|---|---|
| `POST` | `/pseudo` | Créer ou récupérer un utilisateur |
| `POST` | `/recommandation` | Générer une recommandation IA |
| `GET` | `/historique` | Lister l'historique utilisateur |
| `GET` | `/favoris` | Lister les favoris utilisateur |
| `POST` | `/favoris` | Ajouter un favori |
| `DELETE` | `/favoris` | Supprimer un favori |
| `OPTIONS` | `*` | Preflight CORS |

### `db.c` / `db.h`

- Initialisation des 4 tables SQLite (local) / PostgreSQL (prod via Supabase)
- CRUD : utilisateurs, recommandations, favoris, cache
- Cache des réponses IA (TTL 1h) pour limiter les appels API

Tables :

| Table | Colonnes clés |
|---|---|
| `utilisateurs` | id, pseudo (UNIQUE), date_creation |
| `recommandations` | id, pseudo, humeur_texte, type, titre, auteur, annee, explication, … |
| `favoris` | id, pseudo, titre, type, auteur, annee, … |
| `cache` | id, cache_key (UNIQUE), resultat_json, date_creation |

### `ai_client.c` / `ai_client.h`

- Construit le prompt système adapté aux catégories demandées
- Appelle `https://api.groq.com/openai/v1/chat/completions` via `libcurl`
- Parse la réponse JSON via `cJSON`
- Regroupe les résultats dans `ResultatRecherche` par catégorie (`films`, `livres`, `series`)
- Gère les erreurs HTTP (401, 404, 429, 5xx) et les timeouts

### `models.c` / `models.h`

- Définit `Recommandation` et `RecommandationList`
- Fournit `free_recommandation_list` pour la libération mémoire

## Flux d'une requête `POST /recommandation`

1. Frontend envoie `{ "pseudo": "...", "humeur": "...", "type": "film,serie", "mode": "..." }`
2. Serveur vérifie le cache (humeur + catégories + mode) — retourne directement si hit
3. Si cache miss : appel Groq API avec prompt système structuré
4. Réponse JSON parsée en `ResultatRecherche`
5. Chaque recommandation reçoit un timestamp serveur et est insérée en DB
6. Résultat mis en cache
7. Serveur retourne `{ "films": [...], "series": [...] }`

## Tests

Fichiers dans `backend/tests/` :

- `test_db.c` : init DB, insertion, lecture, ordre décroissant
- `test_ai_parser.c` : parsing JSON Groq — cas valides, champs manquants, JSON invalide
- `test_runner.c` : point d'entrée unique (CI)

CI GitHub Actions (`.github/workflows/ci.yml`) : installation des dépendances → build → tests.

## Déploiement

- Frontend : Vercel (site statique, root dir = `frontend/`)
- Backend : Render (Docker, root dir = `backend/`)
- Base production : Supabase (PostgreSQL)
- Secrets : variables d'environnement Render / Vercel (jamais en clair dans le code)
