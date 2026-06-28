# Déploiement Backend — MoodMatch (C / Render + Supabase)

Le backend est un serveur HTTP en C (libmicrohttpd). Il se déploie via Docker sur **Render** et utilise **Supabase** (PostgreSQL) comme base de données en production.

---

## Pré-requis

- Dépôt GitHub public avec `backend/` et son `Dockerfile`
- Compte Render : https://render.com
- Compte Supabase : https://supabase.com
- Clé API Groq : https://console.groq.com/keys

---

## Étape 1 — Créer la base Supabase

1. Sur [supabase.com](https://supabase.com) → **New project**, choisis une région Europe
2. Dans **SQL Editor**, exécute ce script pour créer les 4 tables :

```sql
CREATE TABLE utilisateurs (
    id BIGSERIAL PRIMARY KEY,
    pseudo TEXT NOT NULL UNIQUE,
    date_creation TEXT NOT NULL
);

CREATE TABLE recommandations (
    id BIGSERIAL PRIMARY KEY,
    pseudo TEXT NOT NULL DEFAULT 'anonyme',
    humeur_texte TEXT NOT NULL,
    type TEXT NOT NULL,
    titre TEXT NOT NULL,
    auteur_ou_realisateur TEXT NOT NULL,
    annee INTEGER NOT NULL,
    explication TEXT NOT NULL,
    humeur_detectee TEXT NOT NULL,
    ton TEXT NOT NULL,
    duree_ou_pages TEXT NOT NULL,
    pays TEXT,
    genre TEXT,
    sous_genre TEXT,
    raison_precise TEXT,
    fun_fact TEXT,
    si_tu_aimes TEXT,
    message_ami TEXT,
    humeur_analysee TEXT,
    niveau_intensite TEXT,
    mode TEXT,
    date_creation TEXT NOT NULL
);

CREATE TABLE favoris (
    id BIGSERIAL PRIMARY KEY,
    pseudo TEXT NOT NULL,
    humeur_texte TEXT NOT NULL,
    type TEXT NOT NULL,
    titre TEXT NOT NULL,
    auteur_ou_realisateur TEXT NOT NULL,
    annee INTEGER NOT NULL,
    explication TEXT NOT NULL,
    humeur_detectee TEXT NOT NULL,
    ton TEXT NOT NULL,
    duree_ou_pages TEXT NOT NULL,
    pays TEXT,
    genre TEXT,
    raison_precise TEXT,
    fun_fact TEXT,
    si_tu_aimes TEXT,
    niveau_intensite TEXT,
    date_ajout TEXT NOT NULL
);

CREATE TABLE cache (
    id BIGSERIAL PRIMARY KEY,
    cache_key TEXT NOT NULL UNIQUE,
    resultat_json TEXT NOT NULL,
    date_creation TEXT NOT NULL
);
```

3. Dans **Project Settings → Database**, copie la **Connection string** (mode `URI`) :

```
postgresql://postgres:[MOT_DE_PASSE]@db.[REF].supabase.co:5432/postgres
```

---

## Étape 2 — Migrer db.c de SQLite vers libpq

Le fichier `backend/src/db.c` utilise actuellement `sqlite3`. Il faut le réécrire avec `libpq` (le driver PostgreSQL en C).

**Changements clés :**

| SQLite | libpq (PostgreSQL) |
|---|---|
| `#include <sqlite3.h>` | `#include <libpq-fe.h>` |
| `sqlite3_open(path, &db)` | `PQconnectdb(DATABASE_URL)` |
| `sqlite3_prepare_v2(...)` | `PQprepare(...)` |
| `sqlite3_bind_text(stmt, n, ...)` | `PQexecParams(...)` avec tableau de paramètres |
| `AUTOINCREMENT` | `BIGSERIAL` (déjà dans le schéma ci-dessus) |
| `datetime('now')` | `NOW()::text` |
| `ON CONFLICT ... DO UPDATE` | identique en PostgreSQL ✓ |

La variable d'environnement `DATABASE_URL` remplace le paramètre `db_path` dans toutes les fonctions de `db.c`.

Ajoute `-lpq` dans le `Makefile` :
```makefile
LDLIBS = -lmicrohttpd -lcurl -lcjson -lpq
```

---

## Étape 3 — Créer le service sur Render

1. [render.com](https://render.com) → **New → Web Service**
2. Connecte ton repo GitHub, sélectionne `project-groupeX-moodmatch`
3. Configure :

| Champ | Valeur |
|---|---|
| **Root Directory** | `backend` |
| **Environment** | `Docker` |
| **Instance Type** | Free |

4. Clique **Create Web Service**

---

## Étape 4 — Variables d'environnement sur Render

Dans l'onglet **Environment** de ton service Render :

| Variable | Valeur |
|---|---|
| `DATABASE_URL` | la connection string Supabase copiée à l'étape 1 |
| `GROQ_API_KEY` | ta clé Gemini |
| `PORT` | `8080` |

> Ne commite jamais ces valeurs dans le code source.

---

## Étape 5 — URL de production

Une fois le build terminé (~3–5 minutes), Render affiche :

```
https://moodmatch-backend.onrender.com
```

C'est cette URL que tu mets dans `frontend/script.js` (voir [frontend-deploy.md](frontend-deploy.md)).

---

## Étape 6 — Tester

```bash
curl https://moodmatch-backend.onrender.com/api/moods

curl -X POST https://moodmatch-backend.onrender.com/api/chat \
  -H "Content-Type: application/json" \
  -d '{"pseudo":"test","message":"je suis heureux"}'
```

---

## CORS

Dans `server.c`, assure-toi que le header `Access-Control-Allow-Origin` autorise l'URL Vercel du frontend. Pour la démo tu peux mettre `*`, mais restreins-le en production.

---

## Mises à jour

Chaque `git push` redéclenche automatiquement le build sur Render.
