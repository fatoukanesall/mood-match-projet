# Backend MoodMatch (C)

Serveur HTTP en C pour MoodMatch. Gère les recommandations IA, l'historique, les favoris et les utilisateurs.

## Stack

- Serveur HTTP : `libmicrohttpd`
- Base locale : `SQLite3` / Supabase en production
- Appel IA : `libcurl` → Groq REST API
- Parsing JSON : `cJSON`
- Build : `Makefile`

## Installation des dépendances (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y build-essential pkg-config \
    libsqlite3-dev libcurl4-openssl-dev libcjson-dev libmicrohttpd-dev
```

## Compilation

```bash
cd backend
make clean && make
```

## Variables d'environnement

| Variable | Obligatoire | Défaut |
|---|---|---|
| `GROQ_API_KEY` | oui | — |
| `GROQ_MODEL` | non | `llama-3.3-70b-versatile` |
| `PORT` | non | `8080` |
| `DB_PATH` | non | `./moodmatch.db` |

## Lancer le serveur

```bash
export GROQ_API_KEY="votre_cle"
make run
```

## Lancer les tests

```bash
make test
```

Tests exécutés :
- `test_db.c` : init DB, insertion, lecture historique
- `test_ai_parser.c` : parsing JSON Groq (cas valides + erreurs)

## Endpoints

| Méthode | Route | Description |
|---|---|---|
| `POST` | `/pseudo` | Créer ou récupérer un utilisateur |
| `POST` | `/recommandation` | Générer une recommandation IA |
| `GET` | `/historique` | Lister l'historique (header `X-Pseudo`) |
| `GET` | `/favoris` | Lister les favoris (header `X-Pseudo`) |
| `POST` | `/favoris` | Ajouter un favori |
| `DELETE` | `/favoris` | Supprimer un favori |
| `OPTIONS` | `*` | Preflight CORS |

Documentation complète : [`../docs/api.md`](../docs/api.md)

## Notes Windows (PowerShell)

```powershell
$env:GROQ_API_KEY = "votre_cle"
$env:PORT = "8080"
$env:DB_PATH = ".\moodmatch.db"
make clean
make
make run
```

## Déploiement

Voir [`../docs/backend-deploy.md`](../docs/backend-deploy.md).
