# Déploiement

## Backend (Render via Docker)

Le Dockerfile se trouve dans [`../backend/Dockerfile`](../backend/Dockerfile).

Render utilise ce fichier pour builder et lancer le serveur C automatiquement à chaque push.

Instructions complètes : [`../docs/backend-deploy.md`](../docs/backend-deploy.md)

```bash
# Build local (Docker)
cd backend
docker build -t moodmatch-backend .
docker run -e GROQ_API_KEY=votre_cle -e PORT=8080 -p 8080:8080 moodmatch-backend
```

## Frontend (Vercel)

Site statique dans `../frontend/`. Aucune commande de build requise.

Instructions complètes : [`../docs/frontend-deploy.md`](../docs/frontend-deploy.md)

## Variables d'environnement requises en production

| Variable | Service |
|---|---|
| `GROQ_API_KEY` | Render |
| `DATABASE_URL` | Render (Supabase connection string) |
| `PORT` | Render (valeur : `8080`) |
