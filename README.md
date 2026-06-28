# MoodMatch

Application web qui recommande des contenus culturels (films, séries, livres) à partir de l'état émotionnel de l'utilisateur, via une IA générative (Groq / LLaMA 3.3).

---

## Membres et rôles

| Membre | Rôle |
|---|---|
| Fatoumata Sall | Développement complet (backend C, frontend, intégration IA, déploiement) |

---

## Architecture technique

```
[Navigateur — Vercel]
        |
        | HTTP/JSON
        v
[Backend C — libmicrohttpd — Render]
        |                        \
        v                         v
[SQLite (local)]          [Groq API — LLaMA 3.3-70b]
[Supabase (production)]
```

**Stack :**
- Backend : C11, libmicrohttpd, libcurl, cJSON, SQLite3 / Supabase (prod)
- IA : Groq REST API (`llama-3.3-70b-versatile`)
- Frontend : HTML/CSS/JS statique
- CI : GitHub Actions
- Déploiement : Render (backend) + Vercel (frontend)

---

## Installation et build (Linux)

```bash
sudo apt-get install -y build-essential pkg-config \
    libsqlite3-dev libcurl4-openssl-dev libcjson-dev libmicrohttpd-dev

cd backend
make clean && make
```

## Lancer le serveur (local)

```bash
export GROQ_API_KEY="votre_cle"
export PORT=8080
export DB_PATH="./moodmatch.db"
make run
```

## Lancer les tests

```bash
cd backend
make test
```

---

## Variables d'environnement

| Variable | Obligatoire | Défaut |
|---|---|---|
| `GROQ_API_KEY` | oui | — |
| `GROQ_MODEL` | non | `llama-3.3-70b-versatile` |
| `PORT` | non | `8080` |
| `DB_PATH` | non | `./moodmatch.db` |
| `DATABASE_URL` | oui (prod) | — (Supabase connection string) |

---

## Déploiement

- **Frontend :** [docs/frontend-deploy.md](docs/frontend-deploy.md)
- **Backend :** [docs/backend-deploy.md](docs/backend-deploy.md)

URLs de production :
- Frontend (Vercel) : `[À remplir après déploiement]`
- Backend (Render) : `[À remplir après déploiement]`

---

## Démonstration

- Vidéo de démonstration : `[À remplir]`
- Publication LinkedIn : `[À remplir]`

---

## Structure du dépôt

```
project-groupeX-moodmatch/
├── README.md
├── .gitignore
├── backend/
│   ├── src/              # code source C
│   ├── tests/            # tests unitaires
│   ├── Makefile
│   ├── Dockerfile
│   └── README.md
├── frontend/
│   ├── index.html
│   ├── style.css
│   └── script.js
├── docs/
│   ├── architecture.md
│   ├── api.md
│   ├── backend-deploy.md
│   └── frontend-deploy.md
├── deploy/
│   └── README.md
├── demo/
│   └── README.md
└── .github/
    └── workflows/
        └── ci.yml
```

---

## Licence

Projet académique — ESISA, fin de semestre 2025-2026.
