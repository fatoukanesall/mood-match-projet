# Déploiement Frontend — MoodMatch (Vercel)

Le frontend est un site statique (HTML/CSS/JS). Il se déploie sur **Vercel** en quelques clics.

---

## Pré-requis

- Backend déployé sur Render et URL connue (ex. `https://moodmatch-backend.onrender.com`)
- Compte Vercel : https://vercel.com (inscription gratuite via GitHub)

---

## Étape 1 — Mettre à jour l'URL du backend

Dans [frontend/script.js](../frontend/script.js), ligne 1, remplace l'URL locale par l'URL Render :

```js
// Avant
const API_BASE_URL = "http://localhost:8080";

// Après
const API_BASE_URL = "https://moodmatch-backend.onrender.com";
```

Commite et pousse ce changement avant de déployer.

---

## Étape 2 — Déployer sur Vercel

1. Connecte-toi sur [vercel.com](https://vercel.com) → **Add New → Project**
2. Importe le dépôt GitHub `project-groupeX-moodmatch`
3. Configure ainsi :

| Champ | Valeur |
|---|---|
| **Root Directory** | `frontend` |
| **Framework Preset** | Other (pas de framework) |
| **Build Command** | *(laisser vide)* |
| **Output Directory** | *(laisser vide)* |

4. Clique **Deploy**

Vercel détecte automatiquement les fichiers statiques et les sert sans configuration supplémentaire.

---

## Étape 3 — Récupérer l'URL de production

Une fois le déploiement terminé (~30 secondes), Vercel affiche :

```
https://moodmatch-xxxx.vercel.app
```

C'est cette URL que tu mets dans le `README.md` comme lien de déploiement.

---

## Étape 4 — Domaine personnalisé (optionnel)

Dans **Settings → Domains**, tu peux ajouter un domaine custom ou utiliser l'alias `moodmatch.vercel.app` si disponible.

---

## CI/CD automatique

Chaque `git push` sur la branche principale redéclenche un déploiement Vercel automatiquement. Les pull requests génèrent des **preview URLs** séparées.

---

## Vérification finale

Ouvre l'URL Vercel dans le navigateur et vérifie :
- Le modal de pseudo s'affiche bien au premier chargement
- Les appels API partent vers l'URL Render (ouvre les DevTools → onglet Network)
- Aucune erreur `CORS` ou `ERR_CONNECTION_REFUSED` dans la console
