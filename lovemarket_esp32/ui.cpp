#include "ui.h"
#include "model.h"

// ---------------------------------------------------------------------------
// Échappement HTML — équivalent des <%= %> EJS
// ---------------------------------------------------------------------------
String esc(const String& raw) {
  String out;
  out.reserve(raw.length() + 16);
  for (size_t i = 0; i < raw.length(); i++) {
    char c = raw.charAt(i);
    switch (c) {
      case '&':  out += "&amp;";  break;
      case '<':  out += "&lt;";   break;
      case '>':  out += "&gt;";   break;
      case '"':  out += "&quot;"; break;
      case '\'': out += "&#39;";  break;
      default:   out += c;        break;
    }
  }
  return out;
}

// ---------------------------------------------------------------------------
// Pastille de valeur en crédits — les crédits restent discrets et symboliques
// ---------------------------------------------------------------------------
String creditBadge(int credits) {
  if (credits <= 0) {
    return "<span class=\"credit-badge credit-badge-0\" title=\"Envie gratuite\">♥ 0</span>";
  }
  return "<span class=\"credit-badge\" title=\"Récompense du réalisateur\">♥ " + String(credits) + "</span>";
}

// ---------------------------------------------------------------------------
// Options des catégories (source : g_categories, gérées dans l'admin)
// ---------------------------------------------------------------------------
String categoryOptions(const String& selected) {
  String out;
  for (auto& c : g_categories) {
    out += "<option value=\"" + esc(c.name) + "\"";
    if (c.name == selected) out += " selected";
    out += ">" + esc(c.name) + "</option>";
  }
  return out;
}

// ---------------------------------------------------------------------------
// Layout — topbar discrète + navigation basse mobile-first (5 rubriques)
// ---------------------------------------------------------------------------
static String navClass(const String& currentPath, const char* path) {
  return (currentPath == path) ? "nav-active" : "";
}

struct NavItem {
  const char* path;
  const char* icon;
  const char* label;
  const char* rubrique; // badge de notifications (nullptr = aucun)
};

String renderPage(const String& title, const String& currentPath, const String& userId,
                  const String& userDisplayName, int userCredits, bool isAdmin,
                  const String& bodyHtml) {
  static const NavItem NAV[] = {
    {"/dashboard",     "🏠", "Accueil",    nullptr},
    {"/my-wishes",     "✨", "Mes envies", nullptr},
    {"/partner-wishes","💝", "Ses envies", RUBRIQUE_PARTNER},
    {"/exchanges",     "🔄", "Nos échanges", RUBRIQUE_EXCHANGES},
    {"/moments",       "📖", "Nos moments", RUBRIQUE_MOMENTS},
  };
  const int NAV_N = sizeof(NAV) / sizeof(NAV[0]);

  String html;
  html.reserve(bodyHtml.length() + 6144);
  html += "<!DOCTYPE html>\n<html lang=\"fr\">\n<head>\n<meta charset=\"UTF-8\"/>\n<title>";
  html += esc(title);
  html += " - Love Market</title>\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n";
  html += "<link rel=\"icon\" type=\"image/svg+xml\" href=\"/favicon.svg\"/>\n";
  html += "<link rel=\"stylesheet\" href=\"/style.css\"/>\n</head>\n<body>\n";

  // --- Topbar : logo + solde discret + admin (invisible pour Alice) ---
  html += "<header class=\"topbar\">\n<div class=\"logo\">💕 Love Market</div>\n";
  html += "<div class=\"user-info\">\n";
  html += "<span class=\"credits\" title=\"Tes crédits\">" + String(userCredits) + " ♥</span>\n";
  if (isAdmin) {
    html += "<a href=\"/admin\" class=\"admin-link\" title=\"Administration\">⚙️</a>\n";
  }
  html += "<a href=\"/logout\" class=\"logout\">" + esc(userDisplayName) + " · Déconnexion</a>\n";
  html += "</div>\n</header>\n";

  // --- Contenu ---
  html += "<main class=\"page\">\n" + bodyHtml + "\n</main>\n";

  // --- Navigation basse ---
  html += "<nav class=\"bottomnav\">\n";
  for (int i = 0; i < NAV_N; i++) {
    const NavItem& item = NAV[i];
    html += "<a href=\"" + String(item.path) + "\" class=\"" + navClass(currentPath, item.path) + "\">\n";
    html += "<span class=\"ico\">" + String(item.icon) + "</span>\n";
    html += "<span class=\"lbl\">" + String(item.label) + "</span>\n";
    if (item.rubrique) {
      int badge = countUnreadForRubrique(userId, item.rubrique);
      if (badge > 0) {
        html += "<span class=\"nav-badge\">" + String(badge) + "</span>\n";
      }
    }
    html += "</a>\n";
  }
  html += "</nav>\n</body>\n</html>";
  return html;
}

String renderBarePage(const String& headHtml, const String& bodyHtml) {
  String html;
  html.reserve(bodyHtml.length() + 1024);
  html += "<!DOCTYPE html>\n<html lang=\"fr\">\n<head>\n<meta charset=\"UTF-8\"/>\n"
          "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n"
          "<link rel=\"icon\" type=\"image/svg+xml\" href=\"/favicon.svg\"/>\n";
  html += headHtml;
  html += "</head>\n<body>\n" + bodyHtml + "\n</body>\n</html>";
  return html;
}

// ---------------------------------------------------------------------------
// CSS complet V1 — palette ivoire / rose poudré / vieux rose / bordeaux doux /
// beige chaud / gris chaud. Mobile-first, embarqué en PROGMEM.
// ---------------------------------------------------------------------------
const char LM_STYLE_CSS[] PROGMEM = R"CSSRAW(
:root {
  --ivoire: #faf6f0;
  --rose-poudre: #f6e4e5;
  --vieux-rose: #c48b8f;
  --bordeaux: #7a3b44;
  --bordeaux-clair: #9c5560;
  --beige: #f1e8db;
  --gris: #8a7c72;
  --texte: #4a3f3a;
  --card: #fffdfb;
  --ligne: #eadfd6;
}
* { box-sizing: border-box; }
body {
  margin: 0;
  font-family: system-ui, -apple-system, 'Segoe UI', Roboto, sans-serif;
  background: var(--ivoire);
  color: var(--texte);
  font-size: 16px;
  line-height: 1.5;
}
h1, h2, h3 { font-family: Georgia, 'Times New Roman', serif; color: var(--bordeaux); font-weight: 600; }
a { color: var(--bordeaux); text-decoration: none; }
a:hover { text-decoration: underline; }

/* --- Topbar --- */
.topbar {
  display: flex; align-items: center; justify-content: space-between; gap: 8px;
  padding: 10px 14px;
  background: linear-gradient(135deg, var(--bordeaux) 0%, var(--vieux-rose) 100%);
  color: #fff; position: sticky; top: 0; z-index: 20;
}
.logo { font-family: Georgia, serif; font-weight: 700; font-size: 1.15rem; letter-spacing: .3px; }
.user-info { display: flex; align-items: center; gap: 8px; }
.credits {
  background: rgba(255, 255, 255, .18); padding: 3px 10px; border-radius: 999px;
  font-size: .85rem; font-variant-numeric: tabular-nums;
}
.admin-link { font-size: 1.05rem; }
.logout {
  color: #fff; font-size: .78rem; padding: 4px 10px; border-radius: 999px;
  background: rgba(255, 255, 255, .14);
}

/* --- Navigation basse mobile --- */
.bottomnav {
  position: fixed; bottom: 0; left: 0; right: 0; z-index: 30;
  display: flex; background: var(--card); border-top: 1px solid var(--ligne);
  box-shadow: 0 -4px 16px rgba(122, 59, 68, .08);
}
.bottomnav a {
  flex: 1; display: flex; flex-direction: column; align-items: center; justify-content: center;
  gap: 1px; padding: 7px 2px 9px; min-height: 44px;
  font-size: .66rem; color: var(--gris); text-decoration: none; position: relative;
}
.bottomnav a .ico { font-size: 1.25rem; line-height: 1; }
.bottomnav a.nav-active { color: var(--bordeaux); font-weight: 600; }
.bottomnav a.nav-active::before {
  content: ""; position: absolute; top: 0; left: 22%; right: 22%; height: 3px;
  background: var(--vieux-rose); border-radius: 0 0 3px 3px;
}
.nav-badge {
  position: absolute; top: 3px; right: calc(50% - 24px);
  background: var(--vieux-rose); color: #fff;
  font-size: .62rem; font-weight: 700; border-radius: 999px;
  min-width: 16px; height: 16px; padding: 0 4px;
  display: flex; align-items: center; justify-content: center;
}
@media (min-width: 720px) {
  .bottomnav { max-width: 720px; left: 50%; transform: translateX(-50%); border-radius: 18px 18px 0 0; }
  .bottomnav a { font-size: .72rem; }
}

/* --- Contenu --- */
.page { max-width: 680px; margin: 0 auto; padding: 18px 16px 92px; }
.section { margin-bottom: 22px; }
.section-title { margin: 0 0 4px; font-size: 1.5rem; }
.subtitle { margin: 0 0 14px; color: var(--gris); font-size: .92rem; }
.card {
  background: var(--card); border: 1px solid var(--ligne); border-radius: 16px;
  padding: 16px 18px; margin-bottom: 14px;
  box-shadow: 0 4px 14px rgba(122, 59, 68, .06);
}
.wish-card { animation: fadein .35s ease-out both; }
@keyframes fadein {
  from { opacity: 0; transform: translateY(6px); }
  to { opacity: 1; transform: none; }
}
.wish-title { margin: 0 0 6px; font-size: 1.12rem; }
.wish-desc { margin: 6px 0; white-space: pre-line; }
.wish-meta {
  display: flex; flex-wrap: wrap; gap: 6px; align-items: center;
  margin: 8px 0; font-size: .78rem; color: var(--gris);
}
.chip {
  background: var(--beige); color: var(--bordeaux);
  border-radius: 999px; padding: 2px 10px; font-size: .74rem;
}
.credit-badge {
  background: var(--rose-poudre); color: var(--bordeaux);
  border-radius: 999px; padding: 2px 10px; font-weight: 600;
}
.credit-badge-0 { background: var(--beige); color: var(--gris); font-weight: 500; }
.status { border-radius: 999px; padding: 2px 10px; font-size: .74rem; }
.status-open { background: var(--beige); color: var(--gris); }
.status-accepted { background: var(--rose-poudre); color: var(--bordeaux); }
.status-completed { background: #e3efe4; color: #4a6b50; }
.status-cancelled { background: #eee7e1; color: var(--gris); }
.empty {
  background: var(--beige); border-radius: 16px; padding: 18px;
  text-align: center; color: var(--gris); font-style: italic;
}
.actions { display: flex; flex-wrap: wrap; gap: 8px; margin-top: 12px; }

/* --- Boutons --- */
button { font-family: inherit; }
.btn-primary, .btn-secondary, .btn-danger {
  display: inline-block; border-radius: 12px; border: none; cursor: pointer;
  padding: 11px 18px; min-height: 44px; font-size: .95rem; font-weight: 600;
  text-align: center; transition: transform .15s ease, box-shadow .15s ease, background .15s ease;
}
.btn-primary {
  background: var(--bordeaux); color: #fff;
  box-shadow: 0 4px 12px rgba(122, 59, 68, .3);
}
.btn-primary:hover { background: var(--bordeaux-clair); transform: translateY(-1px); }
.btn-secondary {
  background: #fff; color: var(--bordeaux); border: 1px solid var(--vieux-rose);
}
.btn-secondary:hover { background: var(--rose-poudre); }
.btn-danger {
  background: #fff; color: #9c3d3d; border: 1px solid #d8a0a0;
}
.btn-danger:hover { background: #fbeeec; }
.btn-full { display: block; width: 100%; }
@media (max-width: 600px) {
  .card .btn-primary, .card .btn-secondary, .card .btn-danger { display: block; width: 100%; }
}

/* --- Formulaires --- */
.form label { display: block; font-size: .9rem; margin-bottom: 12px; color: var(--gris); }
.form-block { display: block; font-size: .9rem; margin-bottom: 12px; color: var(--gris); }
.label-text { display: block; }
input, textarea, select {
  width: 100%; padding: 10px 12px; border-radius: 10px; border: 1px solid var(--ligne);
  margin-top: 4px; font-family: inherit; font-size: 1rem; background: #fff; color: var(--texte);
}
textarea { resize: vertical; }
input:focus, textarea:focus, select:focus {
  outline: none; border-color: var(--vieux-rose);
  box-shadow: 0 0 0 3px rgba(196, 139, 143, .2);
}
.credit-choices { display: flex; flex-wrap: wrap; gap: 8px; margin-top: 6px; }
.credit-choices label {
  margin: 0; display: inline-flex; align-items: center; gap: 6px;
  background: var(--beige); border: 1px solid var(--ligne); border-radius: 999px;
  padding: 8px 14px; cursor: pointer; color: var(--bordeaux); font-size: .9rem;
}
.credit-choices input { display: none; width: auto; margin: 0; }
.credit-choices input:checked + span {
  font-weight: 700; color: var(--bordeaux);
}
.credit-choices label:has(input:checked) { border-color: var(--vieux-rose); background: var(--rose-poudre); }

/* --- Messages --- */
.error {
  background: #fbeeec; color: #9c3d3d; padding: 10px 12px;
  border-radius: 10px; margin-bottom: 12px; border-left: 4px solid #c86b6b;
}
.success {
  background: #e6f2e7; color: #4a6b50; padding: 10px 12px;
  border-radius: 10px; margin-bottom: 12px; border-left: 4px solid #7aa883;
}
.hint { margin-top: 12px; font-size: .85rem; color: var(--gris); }

/* --- Connexion --- */
.center-card { display: flex; justify-content: center; align-items: center; min-height: 82vh; padding: 16px; }
.card-login { max-width: 420px; width: 100%; }
.title { margin: 0 0 6px; text-align: center; font-size: 1.6rem; }
.user-select { display: flex; gap: 12px; margin: 14px 0 16px; }
.user-btn {
  flex: 1; padding: 16px 10px; font-size: 1.05rem; font-weight: 600;
  border: 2px solid var(--ligne); border-radius: 14px; background: #fff;
  color: var(--bordeaux); cursor: pointer;
  transition: border-color .2s, background .2s, box-shadow .2s;
}
.user-btn:hover { border-color: var(--vieux-rose); }
.user-btn.selected { border-color: var(--vieux-rose); background: var(--rose-poudre); box-shadow: 0 2px 10px rgba(196, 139, 143, .3); }

/* --- Accueil --- */
.home-hero { text-align: center; margin: 10px 0 18px; }
.home-hero h1 { margin: 0 0 6px; font-size: 1.7rem; }
.home-hero p { margin: 0 0 16px; color: var(--gris); }
.mini-list { display: grid; gap: 10px; }
@media (min-width: 640px) { .mini-list { grid-template-columns: repeat(2, minmax(0, 1fr)); } }
.mini-card {
  background: var(--card); border: 1px solid var(--ligne); border-radius: 14px;
  padding: 12px 14px; font-size: .92rem;
}
.mini-card .m-title { font-weight: 600; color: var(--bordeaux); }
.footer-quiet {
  text-align: center; color: var(--gris); font-size: .8rem; margin-top: 18px;
}
.progress {
  background: var(--beige); border-radius: 999px; height: 10px; overflow: hidden; margin: 8px 0;
}
.progress-fill {
  background: linear-gradient(90deg, var(--vieux-rose), var(--bordeaux));
  height: 100%; border-radius: 999px;
}

/* --- Admin --- */
.table { width: 100%; border-collapse: collapse; font-size: .85rem; }
.table th, .table td { border-bottom: 1px solid var(--ligne); padding: 7px 6px; text-align: left; }
.table th { background: var(--beige); color: var(--bordeaux); }
.inline-form { display: inline-block; }
.inline-form input, .inline-form select { width: auto; margin: 0; }

/* --- Fonds décoratifs discrets --- */
.page::before {
  content: "\2766"; position: fixed; right: 4%; top: 18%;
  font-size: 54px; opacity: .05; color: var(--vieux-rose);
  pointer-events: none;
}
)CSSRAW";

// ---------------------------------------------------------------------------
// Favicon
// ---------------------------------------------------------------------------
const char LM_FAVICON_SVG[] PROGMEM =
  "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 100\">\n"
  "  <text y=\".9em\" font-size=\"90\">\xF0\x9F\x92\x95</text>\n"
  "</svg>\n";
