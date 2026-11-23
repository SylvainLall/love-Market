#include "ui.h"

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
// Liste des catégories et icônes (identiques aux vues EJS)
// ---------------------------------------------------------------------------
static const char* CATEGORIES[] = {
  "Romantique", "Détente", "Fun", "Maison", "Sortie",
  "Famille", "Enfants", "Cuisine", "Voyage", "Bien-être"
};

struct IconOption { const char* value; const char* label; };
static const IconOption ICONS[] = {
  {"\xF0\x9F\x9B\x81",              "\xF0\x9F\x9B\x81 Salle de bain / ménage"},    // 🛁
  {"\xF0\x9F\xA5\x90",              "\xF0\x9F\xA5\x90 Petit déjeuner / cuisine"},  // 🥐
  {"\xF0\x9F\x8D\xBD\xEF\xB8\x8F",  "\xF0\x9F\x8D\xBD\xEF\xB8\x8F Restaurant / sortie"}, // 🍽️
  {"\xF0\x9F\x92\x86",              "\xF0\x9F\x92\x86 Massage / détente"},         // 💆
  {"\xF0\x9F\x8E\xB2",              "\xF0\x9F\x8E\xB2 Soirée fun / jeux"},         // 🎲
  {"\xF0\x9F\x93\xBA",              "\xF0\x9F\x93\xBA Séries / film"},             // 📺
  {"\xF0\x9F\x91\xB6",              "\xF0\x9F\x91\xB6 Garde enfants"},             // 👶
  {"\xF0\x9F\x8C\xB9",              "\xF0\x9F\x8C\xB9 Geste romantique"},          // 🌹
  {"\xF0\x9F\x8E\x89",              "\xF0\x9F\x8E\x89 Fête / anniversaire"},       // 🎉
  {"\xF0\x9F\x9A\x97",              "\xF0\x9F\x9A\x97 Sortie / balade"},           // 🚗
  {"\xF0\x9F\x92\x97",              "\xF0\x9F\x92\x97 Autre"}                      // 💗
};

String categoryOptions(const String& selected) {
  String out;
  for (const char* cat : CATEGORIES) {
    String v(cat);
    out += "<option value=\"" + esc(v) + "\"";
    if (v == selected) out += " selected";
    out += ">" + esc(v) + "</option>";
  }
  return out;
}

String iconOptions(const String& selected) {
  String out;
  size_t n = sizeof(ICONS) / sizeof(ICONS[0]);
  for (size_t i = 0; i < n; i++) {
    const IconOption& io = ICONS[i];
    String v(io.value);
    out += "<option value=\"" + esc(v) + "\"";
    // Comme edit-service.ejs : 💗 est sélectionné aussi si aucune icône
    if (v == selected || (v == "\xF0\x9F\x92\x97" && selected.isEmpty())) out += " selected";
    out += ">" + esc(io.label) + "</option>";
  }
  return out;
}

// ---------------------------------------------------------------------------
// Layout — équivalent layout.ejs (topbar, nav, user-info)
// ---------------------------------------------------------------------------
static String navClass(const String& currentPath, const char* path) {
  return (currentPath == path) ? "nav-active" : "";
}

String renderPage(const String& title, const String& currentPath, bool showEventsLink,
                  const String& bodyHtml, const String& userDisplayName,
                  int userCredits, bool isAdmin, int notificationCount) {
  String html;
  html.reserve(bodyHtml.length() + 4096);
  html += "<!DOCTYPE html>\n<html lang=\"fr\">\n<head>\n<meta charset=\"UTF-8\"/>\n<title>";
  html += esc(title);
  html += " - Love Market</title>\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n";
  html += "<link rel=\"icon\" type=\"image/svg+xml\" href=\"/favicon.svg\"/>\n";
  html += "<link rel=\"stylesheet\" href=\"/style.css\"/>\n</head>\n<body>\n";
  html += "<header class=\"topbar\">\n<div class=\"logo\">\xF0\x9F\x92\x95 Love Market</div>\n";

  if (!userDisplayName.isEmpty()) {
    html += "<nav class=\"nav\">\n";
    html += "<a href=\"/dashboard\" title=\"Accueil\" class=\"" + navClass(currentPath, "/dashboard") + "\">"
            "\xF0\x9F\x8F\xA0<span class=\"nav-text\"> Accueil</span></a>\n";
    html += "<a href=\"/shop\" title=\"Boutique\" class=\"" + navClass(currentPath, "/shop") + "\">"
            "\xF0\x9F\x9B\x8D\xEF\xB8\x8F<span class=\"nav-text\"> Boutique</span></a>\n";
    html += "<a href=\"/my-services\" title=\"Mes services\" class=\"" + navClass(currentPath, "/my-services") + "\">"
            "\xF0\x9F\xA4\x9D<span class=\"nav-text\"> Mes services</span></a>\n";
    if (showEventsLink) {
      html += "<a href=\"/special-events\" title=\"\xC3\x89v" "\xC3\xA9nements sp" "\xC3\xA9ciaux\" class=\""
              + navClass(currentPath, "/special-events") + "\">"
              "\xF0\x9F\x8E\xAF<span class=\"nav-text\"> \xC3\x89v" "\xC3\xA9nements</span></a>\n";
    }
    html += "<a href=\"/negotiations\" title=\"N" "\xC3\xA9gociations\" class=\"nav-link-with-badge "
            + navClass(currentPath, "/negotiations") + "\">"
            "\xF0\x9F\x92\xAC<span class=\"nav-text\"> N" "\xC3\xA9gociations</span>";
    if (notificationCount > 0) {
      html += "<span class=\"notification-badge\">" + String(notificationCount) + "</span>";
    }
    html += "</a>\n";
    if (isAdmin) {
      html += "<a href=\"/admin\" class=\"admin-link " + navClass(currentPath, "/admin") + "\" title=\"Admin\">"
              "\xE2\x9A\x99\xEF\xB8\x8F<span class=\"nav-text\"> Admin</span></a>\n";
    }
    html += "</nav>\n";
    html += "<div class=\"user-info\">\n<span class=\"credits\">";
    html += String(userCredits);
    html += " \xE2\x9D\xA4\xEF\xB8\x8F</span>\n<span class=\"user-name\">" + esc(userDisplayName) + "</span>\n";
    html += "<a href=\"/logout\" class=\"logout\">D" "\xC3\xA9connexion</a>\n</div>\n";
  }

  html += "</header>\n<main class=\"page\">\n" + bodyHtml + "\n</main>\n</body>\n</html>";
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
// CSS complet (copie de public/style.css) — stocké en PROGMEM, servi en statique
// ---------------------------------------------------------------------------
const char LM_STYLE_CSS[] PROGMEM = R"CSSRAW(
:root {
  --bg: radial-gradient(circle at top, #ffe6f0 0, #fff7fb 40%, #f5f5ff 100%);
  --card-bg: rgba(255, 255, 255, 0.96);
  --primary: #ff4f8b;
  --primary-dark: #e63872;
  --secondary: #b39ddb;
  --accent: #ffcf66;
  --text: #2a1233;
  --muted: #8b6f8c;
  --border-radius: 22px;
}
* { box-sizing: border-box; }
body {
  margin: 0;
  font-family: system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
  background: var(--bg);
  color: var(--text);
}
a { color: var(--primary-dark); text-decoration: none; }
a:hover { text-decoration: underline; }
.topbar {
  display: flex; align-items: center; justify-content: space-between;
  padding: 8px 12px;
  background: linear-gradient(90deg, #ff85b7, #b39ddb);
  color: white;
  box-shadow: 0 8px 18px rgba(0, 0, 0, 0.18);
  position: sticky; top: 0; z-index: 20;
  gap: 8px; flex-wrap: wrap;
}
.logo {
  font-weight: 700; letter-spacing: 0.5px; font-size: 1.3rem;
  display: flex; align-items: center; gap: 6px;
  animation: logo-pop 1.8s ease-in-out infinite alternate;
}
.nav { display: flex; align-items: center; gap: 4px; }
.nav a {
  padding: 6px; font-weight: 500; display: flex; align-items: center; justify-content: center;
  font-size: 1.2rem; transition: transform 0.2s ease;
}
.nav a:hover { transform: scale(1.15); text-decoration: none; }
.nav a.nav-active {
  transform: scale(1.25);
  background: rgba(255, 255, 255, 0.18);
  border-radius: 999px;
}
.nav-link-with-badge { position: relative; }
.notification-badge {
  position: absolute; top: -4px; right: -4px;
  background: #ff4f8b; color: white;
  font-size: 0.7rem; font-weight: 700; border-radius: 50%;
  width: 18px; height: 18px; display: flex; align-items: center; justify-content: center;
  box-shadow: 0 2px 6px rgba(255, 79, 139, 0.5);
  animation: pulse 2s ease-in-out infinite;
}
@keyframes pulse {
  0%, 100% { transform: scale(1); opacity: 1; }
  50% { transform: scale(1.1); opacity: 0.9; }
}
.nav a .nav-text { display: none; }
.admin-link { border-bottom: 2px dotted rgba(255, 255, 255, 0.8); }
.user-info { display: flex; align-items: center; gap: 6px; flex-wrap: wrap; }
.credits {
  padding: 4px 8px; border-radius: 999px;
  background: rgba(255, 255, 255, 0.2); font-size: 0.9rem;
}
@media (max-width: 640px) {
  .topbar { align-items: flex-start; }
  .logo { flex: 1 1 50%; }
  .user-info { flex: 1 1 50%; justify-content: flex-end; }
  .credits { font-size: 1.4rem; padding: 4px 12px; }
  .nav { width: 100%; justify-content: space-between; margin-top: 6px; }
  .nav a { flex: 1 1 0; }
}
.user-name { font-weight: 600; font-size: 0.9rem; display: none; }
.logout {
  padding: 4px 8px; border-radius: 999px;
  background: rgba(255, 255, 255, 0.2); color: white; font-size: 0.85rem;
}
@media (min-width: 640px) {
  .topbar { padding: 12px 24px; }
  .nav { gap: 12px; }
  .nav a { font-size: 0.95rem; }
  .nav a .nav-text { display: inline; }
  .user-name { display: inline; }
  .credits, .logout { font-size: 1rem; }
}
.page { max-width: 1040px; margin: 32px auto; padding: 0 20px 40px; position: relative; }
.center-card { display: flex; justify-content: center; align-items: center; min-height: 80vh; }
.card {
  background: var(--card-bg);
  border-radius: var(--border-radius);
  padding: 20px 24px;
  box-shadow: 0 8px 24px rgba(0, 0, 0, 0.06);
  margin-bottom: 16px;
  transition: transform 0.2s ease, box-shadow 0.2s ease;
}
.card-login { max-width: 420px; width: 100%; }
.grid { display: grid; gap: 16px; }
@media (min-width: 720px) { .grid { grid-template-columns: repeat(3, minmax(0, 1fr)); } }
.cards { display: grid; gap: 16px; }
@media (min-width: 720px) { .cards { grid-template-columns: repeat(2, minmax(0, 1fr)); } }
.card-title { margin-top: 0; margin-bottom: 8px; }
.service-icon { font-size: 1.4rem; margin-right: 6px; }
.title { margin-top: 0; text-align: center; }
.subtitle { text-align: center; margin-top: 0; color: var(--muted); }
.form label { display: block; font-size: 0.9rem; margin-bottom: 12px; }
input, textarea, select {
  width: 100%; padding: 8px 10px; border-radius: 10px;
  border: 1px solid #e0e0e0; margin-top: 4px; font-family: inherit;
}
button { font-family: inherit; }
.btn-primary {
  background: var(--primary); color: white; border: none; border-radius: 999px;
  padding: 10px 22px; cursor: pointer; font-weight: 600;
  box-shadow: 0 6px 18px rgba(255, 79, 139, 0.35);
  transition: transform 0.15s ease, box-shadow 0.15s ease, background 0.15s ease;
}
.btn-primary:hover {
  background: var(--primary-dark);
  transform: translateY(-1px) scale(1.02);
  box-shadow: 0 10px 24px rgba(255, 79, 139, 0.45);
}
.btn-secondary {
  background: white; color: var(--primary-dark); border-radius: 999px;
  padding: 6px 16px; border: 1px solid var(--primary); cursor: pointer; font-weight: 500;
  transition: background 0.15s ease, color 0.15s ease, transform 0.15s ease, box-shadow 0.15s ease;
}
@media (max-width: 600px) {
  .card .btn-primary, .card .btn-secondary {
    display: block; width: 100%; text-align: center; margin: 6px 0 !important;
  }
}
.section-title {
  margin-top: 0; font-size: 1.6rem;
  background: linear-gradient(90deg, #ff4f8b, #ffcf66);
  -webkit-background-clip: text; color: transparent;
}
.section { margin-bottom: 24px; }
.list { padding-left: 20px; }
.offers li { margin-bottom: 4px; }
.error {
  background: #ffebee; color: #c62828; padding: 8px 10px;
  border-radius: 10px; margin-bottom: 12px;
}
.hint { margin-top: 12px; font-size: 0.9rem; color: var(--muted); }
.inline-form { display: inline-block; margin-right: 8px; }
.offer-form { margin-top: 8px; }
.price { font-weight: 700; margin-bottom: 8px; }
.category { font-size: 0.85rem; color: var(--muted); }
.comment { font-style: italic; }
.info {
  background: #e3f2fd; color: #1565c0; padding: 8px 10px;
  border-radius: 10px; font-size: 0.9rem; margin: 8px 0;
}
.alert {
  padding: 20px; border-radius: var(--border-radius); margin-bottom: 24px;
  box-shadow: 0 8px 24px rgba(0, 0, 0, 0.1);
  animation: slideIn 0.3s ease-out;
}
.alert-success { background: linear-gradient(135deg, #c8e6c9 0%, #a5d6a7 100%); border-left: 5px solid #4caf50; }
.alert-info { background: linear-gradient(135deg, #fff9c4 0%, #fff59d 100%); border-left: 5px solid #ffc107; }
.purchase-notification {
  background: rgba(255, 255, 255, 0.6); padding: 16px;
  border-radius: 12px; margin-top: 12px;
}
.purchase-notification:first-child { margin-top: 16px; }
@keyframes slideIn {
  from { opacity: 0; transform: translateY(-10px); }
  to { opacity: 1; transform: translateY(0); }
}
.badge { display: inline-block; padding: 2px 8px; border-radius: 999px; font-size: 0.8rem; }
.status-pending { background: #fff3cd; }
.status-accepted { background: #c8e6c9; }
.status-rejected { background: #ffcdd2; }
.status-countered { background: #fff9c4; }
.status-realized { background: #e0e0e0; }
.counter-offer-form { margin-top: 12px; }
.counter-offer-form summary { cursor: pointer; display: inline-block; margin-top: 8px; }
.counter-offer-form summary:hover { opacity: 0.8; }
.negotiation-group { position: relative; }
.counter-offer-item { background: rgba(255, 235, 245, 0.3); padding: 12px; border-radius: 8px; }
.table { width: 100%; border-collapse: collapse; font-size: 0.9rem; }
.table th, .table td { border-bottom: 1px solid #eee; padding: 8px; text-align: left; }
.table th { background: #fce4ec; }
@keyframes gift-float {
  0% { transform: translateY(0px) rotate(0deg); text-shadow: 0 0 10px rgba(255, 79, 139, 0.5); }
  50% { transform: translateY(-10px) rotate(5deg); text-shadow: 0 0 20px rgba(255, 79, 139, 0.8); }
  100% { transform: translateY(0px) rotate(0deg); text-shadow: 0 0 10px rgba(255, 79, 139, 0.5); }
}
.gift-icon-container { animation: gift-float 3s ease-in-out infinite; }
.page::before {
  content: "\2764";
  position: fixed; right: 5%; top: 22%;
  font-size: 52px; opacity: 0.07; color: #ff4f8b;
  animation: float-heart 8s ease-in-out infinite;
  pointer-events: none;
}
.page::after {
  content: "\2726";
  position: fixed; left: 6%; bottom: 14%;
  font-size: 40px; opacity: 0.06; color: #ffcf66;
  animation: float-heart 9s ease-in-out infinite reverse;
  pointer-events: none;
}
.card:hover {
  transform: translateY(-4px) scale(1.01);
  box-shadow: 0 12px 30px rgba(0, 0, 0, 0.12);
}
.btn-secondary:hover {
  background: rgba(255, 255, 255, 0.9);
  transform: translateY(-1px);
  box-shadow: 0 4px 12px rgba(244, 143, 177, 0.3);
}
@keyframes float-heart {
  0% { transform: translateY(0); opacity: 0.04; }
  50% { transform: translateY(-20px); opacity: 0.14; }
  100% { transform: translateY(0); opacity: 0.04; }
}
@keyframes logo-pop {
  0% { transform: translateY(0); }
  100% { transform: translateY(-2px); }
}
)CSSRAW";

// ---------------------------------------------------------------------------
// Favicon (copie de public/favicon.svg)
// ---------------------------------------------------------------------------
const char LM_FAVICON_SVG[] PROGMEM =
  "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 100\">\n"
  "  <text y=\".9em\" font-size=\"90\">\xF0\x9F\x92\x95</text>\n"
  "</svg>\n";
