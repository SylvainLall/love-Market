#include "pages.h"
#include "ui.h"

// ---------------------------------------------------------------------------
// GET /admin — espace d'administration de Sylvain uniquement
// (côté routes : 403 si l'utilisateur n'est pas admin)
// ---------------------------------------------------------------------------
static String statusBadge(const Wish& w) {
  if (w.status == "open") return "<span class=\"status status-open\">disponible</span>";
  if (w.status == "accepted") return "<span class=\"status status-accepted\">acceptée</span>";
  if (w.status == "completed") return "<span class=\"status status-completed\">réalisée</span>";
  return "<span class=\"status status-cancelled\">annulée</span>";
}

String pageAdmin(User& user, const String& importStatus) {
  RelData rel;
  getRelationshipData(rel);

  String b;
  b.reserve(14000);
  b += "<section class=\"section\">\n<h1 class=\"section-title\">Administration</h1>\n";
  b += "<p class=\"subtitle\">Espace réservé à Sylvain — données, crédits et configuration.</p>\n";
  if (importStatus == "ok") b += "<div class=\"success\">Import réussi : les données ont été remplacées.</div>\n";
  if (importStatus == "failed") b += "<div class=\"error\">Import refusé : JSON invalide ou format non compatible (format V1 attendu, avec « users » et « wishes »).</div>\n";
  b += "</section>\n";

  // --- Utilisateurs et crédits (tout ajustement est journalisé) ---
  b += "<section class=\"section\">\n<h2 class=\"section-title\">Utilisateurs et crédits</h2>\n<div class=\"card\">\n";
  b += "<table class=\"table\">\n<tr><th>Utilisateur</th><th>Rôle</th><th>Crédits</th></tr>\n";
  for (auto& u : g_users) {
    b += "<tr><td>" + esc(u.displayName) + "</td><td>" + esc(u.role) + "</td><td>" + String(u.credits) + " ♥</td></tr>\n";
  }
  b += "</table>\n";
  b += "<h3 style=\"margin-bottom: 4px;\">Ajustement journalisé</h3>\n";
  b += "<p class=\"hint\" style=\"margin-top:0;\">Montant signé (+/-). Le motif est obligatoire et inscrit au journal.</p>\n";
  b += "<form method=\"post\" action=\"/admin/credits\" class=\"form\">\n";
  b += "<label>Utilisateur<select name=\"targetId\">\n";
  for (auto& u : g_users) b += "<option value=\"" + esc(u.id) + "\">" + esc(u.displayName) + "</option>\n";
  b += "</select></label>\n";
  b += "<label>Montant (+/-)<input type=\"number\" name=\"amount\" step=\"1\" placeholder=\"Ex : -5 ou 20\" required /></label>\n";
  b += "<label>Motif *<input type=\"text\" name=\"motif\" placeholder=\"Ex : correction d'un oubli\" required /></label>\n";
  b += "<button type=\"submit\" class=\"btn-primary\">Appliquer</button>\n</form>\n</div>\n</section>\n";

  // --- Journal des crédits ---
  b += "<section class=\"section\">\n<h2 class=\"section-title\">Journal des crédits</h2>\n<div class=\"card\">\n";
  if (g_transactions.empty()) {
    b += "<p class=\"hint\" style=\"margin:0;\">Aucun mouvement de crédits enregistré.</p>\n";
  } else {
    b += "<table class=\"table\">\n<tr><th>Date</th><th>Qui</th><th>Montant</th><th>Type</th><th>Détail</th></tr>\n";
    int shown = 0;
    for (size_t i = g_transactions.size(); i > 0 && shown < 10; i--) {
      CreditTransaction& t = g_transactions[i - 1];
      shown++;
      User* u = findUser(t.userId);
      b += "<tr><td>" + esc(t.hasCreatedAt ? formatDateTimeFR(t.createdAt) : String("-")) + "</td>" +
           "<td>" + esc(u ? u->displayName : String("?")) + "</td>" +
           "<td>" + (t.amount >= 0 ? String("+") : String("")) + String(t.amount) + " ♥</td>" +
           "<td>" + esc(t.type) + "</td>" +
           "<td>" + esc(t.description) + "</td></tr>\n";
    }
    b += "</table>\n";
  }
  b += "</div>\n</section>\n";

  // --- Envies et statuts ---
  b += "<section class=\"section\">\n<h2 class=\"section-title\">Envies et statuts</h2>\n<div class=\"card\">\n";
  if (g_wishes.empty()) {
    b += "<p class=\"hint\" style=\"margin:0;\">Aucune envie dans la base.</p>\n";
  } else {
    b += "<table class=\"table\">\n<tr><th>Envie</th><th>Auteur</th><th>Valeur</th><th>Statut</th><th>Réalisateur</th></tr>\n";
    for (size_t i = g_wishes.size(); i > 0; i--) {
      Wish& w = g_wishes[i - 1];
      User* a = findUser(w.userId);
      User* m = w.acceptedBy.isEmpty() ? nullptr : findUser(w.acceptedBy);
      b += "<tr><td>" + esc(w.title) + "</td>" +
           "<td>" + esc(a ? a->displayName : String("?")) + "</td>" +
           "<td>" + String(w.credits) + " ♥</td>" +
           "<td>" + statusBadge(w) + "</td>" +
           "<td>" + esc(m ? m->displayName : String("-")) + "</td></tr>\n";
    }
    b += "</table>\n";
  }
  b += "</div>\n</section>\n";

  // --- Niveaux du couple (progression = envies confirmées réalisées) ---
  b += "<section class=\"section\">\n<h2 class=\"section-title\">Niveaux du couple</h2>\n<div class=\"card\">\n";
  b += "<p class=\"hint\" style=\"margin-top:0;\">Progression actuelle : <strong>" + String(rel.completedCount) +
       "</strong> envie" + (rel.completedCount > 1 ? "s" : "") + " confirmée" + (rel.completedCount > 1 ? "s" : "") +
       " comme réalisée" + (rel.completedCount > 1 ? "s" : "") +
       " · niveau " + esc(g_levels[rel.levelIndex].icon) + " " + esc(g_levels[rel.levelIndex].name) + ".</p>\n";
  for (size_t i = 0; i < g_levels.size(); i++) {
    b += "<form method=\"post\" action=\"/admin/levels\" class=\"inline-form\" style=\"margin:6px 0;\">\n";
    b += "<input type=\"hidden\" name=\"levelIndex\" value=\"" + String((int)i) + "\" />\n";
    b += "<label style=\"display:inline-flex;align-items:center;gap:6px;\">" + esc(g_levels[i].icon) + " " +
         esc(g_levels[i].name) + " — seuil : " +
         "<input type=\"number\" name=\"threshold\" min=\"0\" step=\"1\" value=\"" + String(g_levels[i].threshold) +
         "\" style=\"width:90px;\" /></label>\n";
    b += "<button type=\"submit\" class=\"btn-secondary\">Enregistrer</button>\n</form>\n";
  }
  b += "<p class=\"hint\">Règle simple : le compteur augmente d'un point à chaque envie confirmée comme réalisée par son bénéficiaire.</p>\n";
  b += "</div>\n</section>\n";

  // --- Catégories d'envies ---
  b += "<section class=\"section\">\n<h2 class=\"section-title\">Catégories d'envies</h2>\n<div class=\"card\">\n";
  for (size_t i = 0; i < g_categories.size(); i++) {
    b += "<div class=\"inline-form\" style=\"margin:4px 0;\">\n";
    b += "<span>" + esc(g_categories[i].icon) + " " + esc(g_categories[i].name) + "</span>\n";
    b += "<form method=\"post\" action=\"/admin/categories\" class=\"inline-form\" "
         "onsubmit=\"return confirm('Supprimer cette catégorie ? Les envies déjà publiées la conservent.')\">"
         "<input type=\"hidden\" name=\"action\" value=\"delete\" />"
         "<input type=\"hidden\" name=\"index\" value=\"" + String((int)i) + "\" />"
         "<button type=\"submit\" class=\"btn-danger\">Supprimer</button></form>\n";
    b += "</div>\n";
  }
  b += "<form method=\"post\" action=\"/admin/categories\" class=\"form\">\n";
  b += "<input type=\"hidden\" name=\"action\" value=\"add\" />\n";
  b += "<label>Nouvelle catégorie<input type=\"text\" name=\"name\" placeholder=\"Ex : Voyages\" required /></label>\n";
  b += "<label>Icône (emoji)<input type=\"text\" name=\"icon\" placeholder=\"Ex : ✈️\" maxlength=\"8\" /></label>\n";
  b += "<button type=\"submit\" class=\"btn-primary\">Ajouter</button>\n</form>\n</div>\n</section>\n";

  // --- Données : export / import / archive V0 ---
  b += "<section class=\"section\">\n<h2 class=\"section-title\">Données</h2>\n<div class=\"card\">\n";
  b += "<div class=\"actions\">\n";
  b += "<a class=\"btn-secondary\" href=\"/admin/export\" download=\"data.json\">⬇ Exporter data.json</a>\n";
  if (hasLegacyArchive()) {
    b += "<a class=\"btn-secondary\" href=\"/admin/legacy\" download=\"legacy.json\">⬇ Exporter l'archive V0</a>\n";
  }
  b += "</div>\n";
  b += "<form method=\"post\" action=\"/admin/import\" class=\"form\">\n";
  b += "<label>Import (JSON complet du data.json)<textarea name=\"data\" rows=\"5\" "
       "placeholder=\"Coller ici le contenu d'un data.json exporté\" required ></textarea></label>\n";
  b += "<button type=\"submit\" class=\"btn-primary\">Importer</button>\n</form>\n";
  b += "<p class=\"hint\">Avant chaque import, une sauvegarde de sécurité est conservée (/data.bak).</p>\n";
  b += "</div>\n</section>\n";

  // --- Réinitialisations (confirmation explicite obligatoire) ---
  b += "<section class=\"section\">\n<h2 class=\"section-title\">Réinitialisations</h2>\n<div class=\"card\">\n";
  b += "<form method=\"post\" action=\"/admin/reset\" class=\"form\">\n";
  b += "<label>Portée<select name=\"scope\">\n";
  b += "<option value=\"notifications\">Notifications uniquement</option>\n";
  b += "<option value=\"wishes\">Envies disponibles (historique conservé)</option>\n";
  b += "<option value=\"all\">Toutes les données d'activité (envies, journal, notifications)</option>\n";
  b += "</select></label>\n";
  b += "<label style=\"color:#9c3d3d;\"><input type=\"checkbox\" name=\"confirm\" value=\"yes\" style=\"width:auto;margin-right:8px;\" /> "
       "Je confirme explicitement cette réinitialisation (irréversible)</label>\n";
  b += "<button type=\"submit\" class=\"btn-danger\">Réinitialiser</button>\n</form>\n</div>\n</section>\n";

  return renderPage("Administration", "/admin", user.id, user.displayName,
                    user.credits, user.role == "admin", b);
}
