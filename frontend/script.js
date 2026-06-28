const API_BASE_URL = "https://mood-match-projet.onrender.com";

/* ===== IDENTITE UTILISATEUR =====
   Le pseudo EST l'identifiant unique en base (colonne UNIQUE dans la table utilisateurs).
   Deux personnes ne peuvent pas avoir le même pseudo.
   Entrer un pseudo existant → reconnexion automatique (on récupère ses données).
   Entrer un pseudo inconnu → création de compte.
*/
let currentPseudo = localStorage.getItem("moodmatch_pseudo") || "";

const pseudoModal      = document.getElementById("pseudoModal");
const pseudoInput      = document.getElementById("pseudoInput");
const pseudoConfirmBtn = document.getElementById("pseudoConfirmBtn");
const pseudoDisplay    = document.getElementById("pseudoDisplay");
const heroPseudo       = document.getElementById("heroPseudo");
const changePseudoBtn  = document.getElementById("changePseudoBtn");

function applyPseudo(name, message) {
    currentPseudo = name.trim();
    localStorage.setItem("moodmatch_pseudo", currentPseudo);
    if (pseudoDisplay) pseudoDisplay.textContent = currentPseudo;
    if (heroPseudo)    heroPseudo.style.display = "block";
    pseudoModal.classList.add("hidden");
    if (message) setStatus(message);
}

function showPseudoModal() {
    pseudoModal.classList.remove("hidden");
    pseudoInput.value = "";
    pseudoConfirmBtn.disabled = true;
    pseudoInput.focus();
}

async function confirmerPseudo() {
    const name = pseudoInput.value.trim();
    if (!name) return;

    pseudoConfirmBtn.disabled = true;
    pseudoConfirmBtn.textContent = "Vérification...";

    const erreurEl = document.getElementById("pseudoErreur");

    try {
        const res  = await fetch(`${API_BASE_URL}/pseudo`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ pseudo: name }),
        });
        const data = await res.json();

        if (data.ok) {
            if (erreurEl) erreurEl.textContent = "";
            applyPseudo(name, data.message || "");
        } else {
            /* Pseudo déjà pris */
            if (erreurEl) erreurEl.textContent = data.erreur || "Ce pseudo est déjà pris.";
            pseudoInput.select();
            pseudoConfirmBtn.textContent = "C'est parti →";
            pseudoConfirmBtn.disabled = false;
        }
    } catch {
        /* Serveur inaccessible : on laisse quand même passer (mode offline) */
        if (erreurEl) erreurEl.textContent = "";
        applyPseudo(name, "");
    }
}

if (currentPseudo) {
    applyPseudo(currentPseudo);
} else {
    showPseudoModal();
}

pseudoInput.addEventListener("input", () => {
    pseudoConfirmBtn.disabled = pseudoInput.value.trim().length === 0;
});

pseudoInput.addEventListener("keydown", (e) => {
    if (e.key === "Enter" && pseudoInput.value.trim()) confirmerPseudo();
});

pseudoConfirmBtn.addEventListener("click", confirmerPseudo);

if (changePseudoBtn) {
    changePseudoBtn.addEventListener("click", showPseudoModal);
}

/* ===== ELEMENTS DOM ===== */
const moodInput         = document.getElementById("humeurInput");
const categoryInputs    = [...document.querySelectorAll('input[name="category"]')];
const typeButtons       = [...document.querySelectorAll('.type-btn')];
const typeButtonsWrapper = document.getElementById("type-buttons");
const submitBtn         = document.getElementById("submit-btn");
const loadingEl         = document.getElementById("loading");
const statusMsg         = document.getElementById("statusMsg");
const resultsEl         = document.getElementById("results");
const historyEl         = document.getElementById("history");
const favoritesEl       = document.getElementById("favorites");

const CATEGORY_ORDER = [
    { key: "film",  responseKey: "films",  title: "Films",  icon: "🎬" },
    { key: "livre", responseKey: "livres", title: "Livres", icon: "📚" },
    { key: "serie", responseKey: "series", title: "Séries", icon: "📺" },
    { key: "anime", responseKey: "animes", title: "Anime",  icon: "🌸" },
];

/* Set local des favoris (titre|type) pour affichage instantané du coeur */
let favoritesSet = new Set();

/* ===== ONGLETS ===== */
const tabBtns   = [...document.querySelectorAll(".tab-btn")];
const tabPanels = {
    results:   document.getElementById("tab-results"),
    history:   document.getElementById("tab-history"),
    favorites: document.getElementById("tab-favorites"),
};

function switchTab(name) {
    tabBtns.forEach((btn) => {
        const isActive = btn.dataset.tab === name;
        btn.classList.toggle("active", isActive);
        btn.setAttribute("aria-selected", isActive ? "true" : "false");
    });
    Object.entries(tabPanels).forEach(([key, panel]) => {
        panel.classList.toggle("hidden", key !== name);
    });
    if (name === "history")   loadHistory();
    if (name === "favorites") loadFavorites();
}

tabBtns.forEach((btn) => {
    btn.addEventListener("click", () => switchTab(btn.dataset.tab));
});

/* ===== HELPERS ===== */
const messagesParDefaut = {
    "soutenance": "Ok, respire. T'as bossé dur. Ce soir on te recharge.",
    "examen":     "La pression que tu ressens, c'est ton cerveau qui se prépare. On t'aide.",
    "stress":     "Respire. On a exactement ce qu'il te faut pour décompresser.",
    "travail":    "Tu bosses trop. Ce soir, tu poses tout et tu te ressources.",
    "deadline":   "T'as survécu à toutes tes pires journées. Ce soir, pause.",
    "confiance":  "Tout le monde doute. Même les meilleurs. Ce soir on te le prouve.",
    "timide":     "Ta voix mérite d'être entendue. Ces œuvres vont t'en convaincre.",
    "triste":     "Pas besoin de raison pour être triste. On est là ce soir.",
    "deprime":    "Les jours sombres font partie du voyage. On reste avec toi.",
    "colere":     "Ok tu es en colère. Bonne nouvelle : on a de quoi transformer ça.",
    "anxieux":    "L'anxiété ment. Elle dit que tout va mal alors que non.",
    "amour":      "Ah l'amour... On a exactement ce qu'il faut pour nourrir ça.",
    "rupture":    "Oui, pleure un bon coup. C'est fait pour ça. Après tu vas t'en sortir.",
    "seul":       "Tu es seul ce soir mais ces œuvres vont te faire sentir compris.",
    "malade":     "T'as pas besoin de réfléchir. Juste regarder et te laisser soigner.",
    "fatigue":    "Ton corps te demande de souffler. Ces œuvres sont faites pour ça.",
    "burnout":    "T'as donné trop longtemps. Ce soir c'est ton tour de recevoir.",
    "heureux":    "Belle énergie ! On va nourrir ça avec quelque chose à ta hauteur.",
    "fier":       "Tu as le droit d'être fier. Ces œuvres vont célébrer ça avec toi.",
    "apprendre":  "Parfait timing. Ce que tu vas découvrir va changer ta vision du monde.",
    "default":    "Fatou Sall a trouvé quelque chose de parfait pour toi ce soir.",
};

function getMessageParDefaut(humeur) {
    const h = (humeur || "").toLowerCase();
    for (const mot in messagesParDefaut) {
        if (mot !== "default" && h.includes(mot)) return messagesParDefaut[mot];
    }
    return messagesParDefaut["default"];
}

function esc(text) {
    const div = document.createElement("div");
    div.textContent = text ?? "";
    return div.innerHTML;
}

function iconForType(type) {
    if (type === "film")  return "🎬";
    if (type === "livre") return "📚";
    if (type === "serie") return "📺";
    if (type === "anime") return "🌸";
    return "✨";
}

function favKey(item) {
    return `${item.titre}|${item.type}`;
}

function setStatus(message, isError = false) {
    statusMsg.textContent = message;
    statusMsg.style.color = isError ? "#8f2914" : "#5f696d";
}

function setLoading(isLoading) {
    loadingEl.classList.toggle("hidden", !isLoading);
    if (submitBtn) submitBtn.disabled = isLoading;
    categoryInputs.forEach((input) => { input.disabled = isLoading; });
}

/* ===== MODES ===== */
const MODE_INPUTS = [...document.querySelectorAll('input[name="mode"]')];
MODE_INPUTS.forEach((input) => {
    const pill = input.closest(".mode-pill");
    if (pill) pill.classList.toggle("is-selected", input.checked);
    input.addEventListener("change", () => {
        MODE_INPUTS.forEach((i) => {
            const p = i.closest(".mode-pill");
            if (p) p.classList.toggle("is-selected", i.checked);
        });
    });
});

function getSelectedMode() {
    const sel = MODE_INPUTS.find((i) => i.checked);
    return sel ? sel.value : "courage";
}

/* ===== CATEGORIES ===== */
function updateCategoryPills() {
    categoryInputs.forEach((input) => {
        const typeBtn = document.querySelector(`.type-btn[data-type="${input.value}"]`);
        if (typeBtn) {
            typeBtn.classList.toggle("is-selected", input.checked);
            typeBtn.setAttribute("aria-pressed", input.checked ? "true" : "false");
        }
    });
}

function getSelectedCategories() {
    return CATEGORY_ORDER
        .filter(({ key }) => categoryInputs.find((i) => i.value === key && i.checked))
        .map(({ key }) => key);
}

function updateSubmitButtonState() {
    if (!submitBtn) return;
    const hasMood     = moodInput.value.trim().length > 0;
    const hasCategory = getSelectedCategories().length > 0;
    submitBtn.disabled = !hasMood || !hasCategory;
}

categoryInputs.forEach((input) => {
    input.addEventListener("change", () => {
        updateCategoryPills();
        updateSubmitButtonState();
    });
});

if (typeButtonsWrapper) {
    typeButtonsWrapper.addEventListener("click", (e) => {
        const button = e.target.closest(".type-btn");
        if (!button) return;
        const type = button.getAttribute("data-type");
        const matching = categoryInputs.find((i) => i.value === type);
        if (!matching) return;
        matching.checked = !matching.checked;
        updateCategoryPills();
        updateSubmitButtonState();
    });
}

moodInput.addEventListener("input", updateSubmitButtonState);

/* ===== RENDU DES CARTES ===== */
function renderEmpty(container, message) {
    container.innerHTML = `<div class="empty">${esc(message)}</div>`;
}

function renderError(container, error) {
    const msg  = error.message || "Erreur inconnue.";
    const detail = error.detail || null;
    const isNetwork = msg.includes("serveur") || msg.includes("backend");
    let hint = "";
    if (isNetwork) {
        hint = "Lance le serveur C depuis le dossier backend/ avec <code>run.bat</code> puis réessaie.";
    } else if (detail) {
        hint = esc(detail);
    }
    container.innerHTML = `
        <div style="background:#fff0f0;border:1.5px solid #c0392b;border-radius:12px;padding:20px 24px;margin:16px 0;color:#7b1c1c;">
            <p style="font-weight:700;font-size:1.05em;margin:0 0 8px 0;">⚠ ${esc(msg)}</p>
            ${hint ? `<p style="margin:0;font-size:0.9em;line-height:1.5;">${hint}</p>` : ""}
        </div>`;
}

function renderCard(item, options = {}) {
    const { showFavBtn = false, showDeleteBtn = false } = options;
    const typeClass = item.type ? `card--${esc(item.type)}` : "card--film";
    const isFav     = favoritesSet.has(favKey(item));

    const recAttr = JSON.stringify(item).replace(/&/g, "&amp;").replace(/"/g, "&quot;");
    const favBtnHtml = showFavBtn
        ? `<button class="fav-btn ${isFav ? "is-fav" : ""}"
               data-rec="${recAttr}"
               title="${isFav ? "Retirer des favoris" : "Ajouter aux favoris"}">
               ${isFav ? "❤" : "♡"}
           </button>`
        : "";

    const deleteBtnHtml = showDeleteBtn
        ? `<button class="fav-delete-btn" data-id="${item.id}" title="Retirer des favoris">
               ✕ Retirer
           </button>`
        : "";

    return `
        <article class="card ${typeClass}">
            <div class="card-top">
                <span class="chip">${iconForType(item.type)} ${esc(item.type || "recommandation")}</span>
                <span class="chip">${esc(String(item.annee ?? ""))}</span>
            </div>
            <p class="title">${esc(item.titre || "Titre indisponible")}</p>
            <p class="author">${esc(item.auteur_ou_realisateur || "")}</p>
            <p class="explanation">${esc(item.explication || "")}</p>
            ${item.raison_precise ? `<p class="reason">${esc(item.raison_precise)}</p>` : ""}
            <div class="card-actions">
                <button class="anecdote-btn">✦ Anecdote</button>
                ${favBtnHtml}
                ${deleteBtnHtml}
            </div>
            ${item.fun_fact ? `<div class="fun-fact" style="display:none">${esc(item.fun_fact)}</div>` : ""}
            <div class="meta">
                <span class="chip niveau-tag">${esc(item.niveau_intensite || "-")}</span>
                <span class="chip">Humeur: ${esc(item.humeur_detectee || "-")}</span>
                <span class="chip">Ton: ${esc(item.ton || "-")}</span>
            </div>
            ${item.si_tu_aimes ? `<div class="si-tu-aimes">${esc(item.si_tu_aimes)}</div>` : ""}
        </article>`;
}

function renderCards(items, options = {}) {
    if (!items || items.length === 0) return "";
    return items.map((item) => renderCard(item, options)).join("");
}

/* Délégation des clics sur les cartes (anecdote, fav, delete) */
function attachCardEvents(container, options = {}) {
    container.addEventListener("click", async (e) => {
        /* Anecdote */
        const anecdoteBtn = e.target.closest(".anecdote-btn");
        if (anecdoteBtn) {
            const card = anecdoteBtn.closest(".card");
            const fun  = card && card.querySelector(".fun-fact");
            if (fun) {
                const hidden = fun.style.display === "none" || fun.style.display === "";
                fun.style.display = hidden ? "block" : "none";
                anecdoteBtn.textContent = hidden ? "✦ Cacher" : "✦ Anecdote";
            }
            return;
        }

        /* Ajouter aux favoris */
        const favBtn = e.target.closest(".fav-btn");
        if (favBtn && options.showFavBtn) {
            const recJson = favBtn.dataset.rec;
            if (!recJson) return;
            let rec;
            try { rec = JSON.parse(recJson); } catch { return; }
            const key = favKey(rec);
            if (favoritesSet.has(key)) {
                return; /* déjà en favoris (le bouton delete est dans l'onglet favoris) */
            }
            favBtn.disabled = true;
            const ok = await addFavorite(rec);
            if (ok) {
                favoritesSet.add(key);
                favBtn.classList.add("is-fav");
                favBtn.innerHTML = "❤";
                favBtn.title = "Dans tes favoris";
            }
            favBtn.disabled = false;
            return;
        }

        /* Supprimer des favoris */
        const deleteBtn = e.target.closest(".fav-delete-btn");
        if (deleteBtn && options.showDeleteBtn) {
            const favId = parseInt(deleteBtn.dataset.id, 10);
            if (!favId) return;
            deleteBtn.disabled = true;
            const ok = await removeFavorite(favId);
            if (ok) {
                const card = deleteBtn.closest(".card");
                if (card) card.remove();
                if (!favoritesEl.querySelector(".card")) {
                    renderEmpty(favoritesEl, "Aucun favori pour le moment. Ajoutes-en depuis les résultats ❤");
                }
            }
            deleteBtn.disabled = false;
        }
    });
}

attachCardEvents(resultsEl,   { showFavBtn:    true });
attachCardEvents(historyEl,   { showFavBtn: true });
attachCardEvents(favoritesEl, { showDeleteBtn: true });

/* ===== RENDU RÉSULTATS GROUPÉS ===== */
function renderGroupedRecommendations(container, payload) {
    const bannerEl = document.querySelector(".fatou-banner");
    const msgBox   = document.getElementById("message-ami");
    const messageAmi = payload?.message_ami
        ? payload.message_ami
        : getMessageParDefaut(moodInput.value || "");

    if (msgBox) {
        msgBox.innerHTML = `"${esc(messageAmi)}" <strong>— Fatou Sall</strong>`;
        msgBox.classList.remove("hidden");
    }
    if (bannerEl) {
        bannerEl.querySelector(".fatou-banner__text").textContent = `${messageAmi} — Fatou Sall`;
        bannerEl.style.display = "flex";
    }

    const sections = CATEGORY_ORDER
        .map(({ responseKey, title, icon }) => {
            const items = payload?.[responseKey] || [];
            if (!items.length) return "";
            return `
                <section class="result-group">
                    <div class="result-group-header">
                        <span class="result-group-icon">${icon}</span>
                        <h3>${title}</h3>
                    </div>
                    <div class="cards">
                        ${renderCards(items, { showFavBtn: true })}
                    </div>
                </section>`;
        })
        .filter(Boolean)
        .join("");

    container.innerHTML = sections || `<div class="empty">Aucune recommandation retournée pour les catégories choisies.</div>`;
}

/* ===== API ===== */
function apiHeaders() {
    return {
        "Content-Type": "application/json",
        "X-Pseudo": currentPseudo,
    };
}

async function fetchJSON(path, options = {}) {
    let response;
    try {
        response = await fetch(`${API_BASE_URL}${path}`, {
            ...options,
            headers: { ...apiHeaders(), ...(options.headers || {}) },
        });
    } catch {
        throw new Error(`Impossible de joindre le serveur (${API_BASE_URL}). Le backend est-il lancé ?`);
    }

    let data = null;
    try { data = await response.json(); } catch { data = null; }

    if (!response.ok) {
        const msg    = data?.erreur || `Erreur HTTP ${response.status}`;
        const detail = data?.detail || null;
        const err    = new Error(msg);
        err.detail   = detail;
        err.status   = response.status;
        throw err;
    }
    return data;
}

/* ===== SOUMISSION ===== */
async function submitRecommendation() {
    const humeur     = moodInput.value.trim();
    const categories = getSelectedCategories();

    if (!humeur) {
        setStatus("Écris d'abord ton humeur en une phrase.", true);
        moodInput.focus();
        return;
    }
    if (categories.length === 0) {
        setStatus("Choisis au moins une catégorie.", true);
        return;
    }
    if (!currentPseudo) {
        showPseudoModal();
        return;
    }

    setLoading(true);
    setStatus("Demande en cours...");

    try {
        const data = await fetchJSON("/recommandation", {
            method: "POST",
            body: JSON.stringify({ humeur, type: categories.join(","), mode: getSelectedMode() }),
        });

        renderGroupedRecommendations(resultsEl, data);
        setStatus("Recommandations générées avec succès.");
        switchTab("results");
    } catch (error) {
        renderError(resultsEl, error);
        setStatus(error.message || "Erreur inconnue.", true);
    } finally {
        setLoading(false);
    }
}

/* ===== HISTORIQUE ===== */
async function loadHistory() {
    try {
        const data  = await fetchJSON("/historique");
        const items = data.historique || [];
        if (items.length === 0) {
            renderEmpty(historyEl, "Aucun historique pour le moment. Fais une première recherche !");
            return;
        }
        historyEl.innerHTML = renderCards(items, { showFavBtn: true });
    } catch (error) {
        renderEmpty(historyEl, "Historique indisponible pour le moment.");
    }
}

/* ===== FAVORIS ===== */
async function loadFavorites() {
    try {
        const data  = await fetchJSON("/favoris");
        const items = data.favoris || [];
        favoritesSet = new Set(items.map(favKey));
        if (items.length === 0) {
            renderEmpty(favoritesEl, "Aucun favori pour le moment. Ajoutes-en depuis les résultats ❤");
            return;
        }
        favoritesEl.innerHTML = renderCards(items, { showDeleteBtn: true });
    } catch {
        renderEmpty(favoritesEl, "Favoris indisponibles pour le moment.");
    }
}

async function addFavorite(rec) {
    try {
        await fetchJSON("/favoris", {
            method: "POST",
            body: JSON.stringify(rec),
        });
        return true;
    } catch {
        return false;
    }
}

async function removeFavorite(favId) {
    try {
        await fetchJSON(`/favoris?id=${favId}`, { method: "DELETE" });
        return true;
    } catch {
        return false;
    }
}

/* ===== EVENEMENTS GLOBAUX ===== */
if (submitBtn) submitBtn.addEventListener("click", submitRecommendation);

window.addEventListener("keydown", (e) => {
    if ((e.ctrlKey || e.metaKey) && e.key === "Enter") submitRecommendation();
});

/* ===== INIT ===== */
updateCategoryPills();
updateSubmitButtonState();
renderEmpty(resultsEl, "Tes recommandations apparaîtront ici.");
renderEmpty(historyEl, "Chargement de l'historique...");
renderEmpty(favoritesEl, "Chargement des favoris...");
