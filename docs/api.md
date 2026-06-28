# API MoodMatch

## Base URL

- Local : `http://localhost:8080`
- Production : URL Render (voir [backend-deploy.md](backend-deploy.md))

## Format

- Encodage : UTF-8
- Corps requête/réponse : JSON
- Header client : `Content-Type: application/json`
- Header utilisateur : `X-Pseudo: <pseudo>` (pour les routes historique/favoris)

## CORS

```
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS
Access-Control-Allow-Headers: Content-Type, X-Pseudo
```

---

## POST `/pseudo`

Crée un compte si le pseudo est inconnu, le récupère sinon.

**Body :**
```json
{ "pseudo": "fatou" }
```

**Réponse 200 (nouveau) :**
```json
{ "statut": "cree", "pseudo": "fatou" }
```

**Réponse 200 (existant) :**
```json
{ "statut": "existant", "pseudo": "fatou" }
```

**Erreurs :** `400` champ manquant · `500` erreur DB

---

## POST `/recommandation`

Génère des recommandations via l'IA Groq et les enregistre en base.

**Body :**
```json
{
    "pseudo": "fatou",
    "humeur": "Je suis stressée par ma soutenance",
    "type": "film,serie",
    "mode": "classique"
}
```

- `type` : une ou plusieurs valeurs parmi `film`, `livre`, `serie`, `anime`
- `mode` : optionnel

**Réponse 200 :**
```json
{
    "films": [
        {
            "titre": "Whiplash",
            "auteur_ou_realisateur": "Damien Chazelle",
            "annee": 2014,
            "explication": "...",
            "humeur_detectee": "Pression / besoin de dépassement",
            "ton": "Intense, motivant",
            "duree_ou_pages": "1h47",
            "pays": "États-Unis",
            "genre": "Drame",
            "raison_precise": "...",
            "fun_fact": "...",
            "si_tu_aimes": "...",
            "message_ami": "..."
        }
    ],
    "series": [ { ... } ]
}
```

Seules les clés correspondant aux catégories demandées sont présentes.

**Erreurs :** `400` JSON invalide · `413` body trop volumineux · `502` erreur Groq API · `500` erreur DB

---

## GET `/historique`

Retourne l'historique des recommandations de l'utilisateur.

**Header :** `X-Pseudo: fatou`

**Réponse 200 :**
```json
{
    "historique": [
        {
            "id": 12,
            "pseudo": "fatou",
            "humeur_texte": "...",
            "type": "film",
            "titre": "Whiplash",
            "auteur_ou_realisateur": "Damien Chazelle",
            "annee": 2014,
            "explication": "...",
            "humeur_detectee": "Pression",
            "ton": "Intense",
            "duree_ou_pages": "1h47",
            "date_creation": "2026-06-20T12:00:00Z"
        }
    ]
}
```

Trié par `date_creation` décroissant.

**Erreurs :** `500` erreur DB

---

## GET `/favoris`

Retourne les favoris de l'utilisateur.

**Header :** `X-Pseudo: fatou`

**Réponse 200 :**
```json
{
    "favoris": [ { ... } ]
}
```

**Erreurs :** `400` pseudo manquant · `500` erreur DB

---

## POST `/favoris`

Ajoute une recommandation aux favoris.

**Body :** objet recommandation complet (même structure que les items de `/recommandation`)

**Réponse 200 :**
```json
{ "id": 5 }
```

**Erreurs :** `400` · `500`

---

## DELETE `/favoris`

Supprime un favori.

**Header :** `X-Pseudo: fatou`  
**Query param :** `?id=5`

**Réponse 200 :**
```json
{ "statut": "supprime" }
```

**Erreurs :** `400` · `500`

---

## OPTIONS `*`

Preflight CORS — retourne `204 No Content` avec les headers CORS.

---

## Exemples curl

```bash
# Créer un pseudo
curl -X POST http://localhost:8080/pseudo \
  -H "Content-Type: application/json" \
  -d '{"pseudo":"fatou"}'

# Obtenir une recommandation
curl -X POST http://localhost:8080/recommandation \
  -H "Content-Type: application/json" \
  -d '{"pseudo":"fatou","humeur":"Je suis fatiguée","type":"film,livre"}'

# Lister l'historique
curl http://localhost:8080/historique -H "X-Pseudo: fatou"

# Lister les favoris
curl http://localhost:8080/favoris -H "X-Pseudo: fatou"

# Supprimer un favori
curl -X DELETE "http://localhost:8080/favoris?id=5" -H "X-Pseudo: fatou"
```
