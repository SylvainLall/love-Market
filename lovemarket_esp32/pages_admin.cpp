#include "pages.h"
#include "ui.h"

// Date seule jj/mm/aaaa (équivalent toLocaleDateString('fr-FR'))
static String formatDateFR(time_t t) {
  struct tm lt;
  localtime_r(&t, &lt);
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d/%02d/%04d", lt.tm_mday, lt.tm_mon + 1, lt.tm_year + 1900);
  return String(buf);
}

// ---------------------------------------------------------------------------
// GET /admin — views/admin.ejs (+ export/import data.json)
// ---------------------------------------------------------------------------
String pageAdmin(User& user, const String& importStatus) {
  int notif = countNotifications(user.id);

  String b;
  b.reserve(12000);
  b += "<section class=\"section\">\n<h1 class=\"section-title\">Panneau d'administration 🛡️</h1>\n";

  if (importStatus == "ok") {
    b += "<div class=\"alert alert-success\"><p style=\"margin: 0;\">✅ Données importées avec succès.</p></div>\n";
  } else if (importStatus == "failed") {
    b += "<div class=\"error\">❌ Import impossible : JSON invalide ou fichier incompatible (clé \"users\" requise).</div>\n";
  }

  // Utilisateurs & crédits
  b += "<div class=\"card\">\n<h2 class=\"card-title\">Utilisateurs & crédits</h2>\n<table class=\"table\">\n<thead>\n<tr>\n";
  b += "<th>Utilisateur</th><th>Rôle</th><th>Crédits</th><th>Définir les crédits</th>\n</tr>\n</thead>\n<tbody>\n";
  for (auto& u : g_users) {
    b += "<tr>\n<td>" + esc(u.displayName) + " (" + esc(u.id) + ")</td>\n<td>" + esc(u.role) + "</td>\n";
    b += "<td>" + String(u.credits) + " ❤️</td>\n<td>\n";
    b += "<form method=\"post\" action=\"/admin/credits\" class=\"inline-form\">\n";
    b += "<input type=\"hidden\" name=\"targetId\" value=\"" + esc(u.id) + "\" />\n";
    b += "<input type=\"number\" name=\"credits\" min=\"0\" step=\"1\" value=\"" + String(u.credits) + "\" />\n";
    b += "<button type=\"submit\" class=\"btn-secondary\">Mettre à jour</button>\n</form>\n</td>\n</tr>\n";
  }
  b += "</tbody>\n</table>\n</div>\n";

  // Créer un bon cadeau
  b += "<div class=\"card\">\n<h2 class=\"card-title\">Créer un bon cadeau 🎁</h2>\n";
  b += "<form method=\"post\" action=\"/admin/gift-voucher\" class=\"form\">\n";
  b += "<label>Crédits à offrir<input type=\"number\" name=\"credits\" min=\"1\" step=\"1\" placeholder=\"Ex: 50\" required /></label>\n";
  b += "<label>Commentaire (optionnel)<textarea name=\"comment\" rows=\"2\" placeholder=\"Un petit mot d'accompagnement...\"></textarea></label>\n";
  b += "<button type=\"submit\" class=\"btn-primary\">Générer le bon cadeau</button>\n</form>\n</div>\n";

  // Bons cadeaux existants
  b += "<div class=\"card\">\n<h2 class=\"card-title\">Bons cadeaux existants 🎟️</h2>\n";
  if (g_vouchers.empty()) {
    b += "<p>Aucun bon cadeau n'a été créé pour le moment.</p>\n";
  } else {
    b += "<table class=\"table\">\n<thead>\n<tr>\n";
    b += "<th>Code</th><th>Crédits</th><th>Commentaire</th><th>Statut</th><th>Date de création</th><th>Action</th>\n</tr>\n</thead>\n<tbody>\n";
    for (auto& v : g_vouchers) {
      b += "<tr>\n<td>" + esc(v.code) + "</td>\n<td>" + String(v.credits) + " ❤️</td>\n";
      b += "<td>" + (v.comment.isEmpty() ? String("-") : esc(v.comment)) + "</td>\n<td>";
      if (v.used) b += "<span class=\"badge status-rejected\">Utilisé</span>";
      else b += "<span class=\"badge status-accepted\">Disponible</span>";
      b += "</td>\n";
      b += "<td>" + (v.hasCreatedAt ? formatDateTimeFR(v.createdAt, true) : String("-")) + "</td>\n<td>";
      if (!v.used) b += "<a href=\"/gift-voucher/" + esc(v.code) + "\" target=\"_blank\" class=\"btn-secondary\">Imprimer</a>";
      else b += "<span>Utilisé</span>";
      b += "</td>\n</tr>\n";
    }
    b += "</tbody>\n</table>\n";
  }
  b += "</div>\n";

  // Gestion des événements spéciaux
  b += "<div class=\"card\">\n<h2 class=\"card-title\">🎫 Gestion des événements spéciaux</h2>\n";
  b += "<form method=\"post\" action=\"/admin/special-event\" class=\"form\">\n";
  b += "<label>Titre de l'événement<input type=\"text\" name=\"title\" placeholder=\"Ex: Faire une surprise à ton partenaire\" required /></label>\n";
  b += "<label>Description<textarea name=\"description\" rows=\"3\" placeholder=\"Décris ce que l'utilisateur doit faire...\" required></textarea></label>\n";
  b += "<label>Crédits à gagner<input type=\"number\" name=\"credits\" min=\"1\" placeholder=\"Ex: 20\" required /></label>\n";
  b += "<label>Destinataire<select name=\"targetUser\">\n";
  b += "<option value=\"both\">Les deux utilisateurs</option>\n";
  for (auto& u : g_users) {
    b += "<option value=\"" + esc(u.id) + "\">" + esc(u.displayName) + " uniquement</option>\n";
  }
  b += "</select></label>\n";
  b += "<button type=\"submit\" class=\"btn-primary\">Créer l'événement</button>\n</form>\n";

  if (!g_events.empty()) {
    b += "<h3>Événements actifs</h3>\n<div class=\"table-container\">\n<table class=\"admin-table table\">\n<thead>\n<tr>\n";
    b += "<th>Titre</th><th>Description</th><th>Crédits</th><th>Destinataire</th><th>Statut</th><th>Créé le</th>\n</tr>\n</thead>\n<tbody>\n";
    for (auto& e : g_events) {
      b += "<tr>\n<td>" + esc(e.title) + "</td>\n<td>" + esc(e.description) + "</td>\n";
      b += "<td>" + String(e.credits) + " ❤️</td>\n<td>";
      if (e.targetUser == "both") b += "Les deux";
      else b += esc(e.targetUser);
      b += "</td>\n<td>";
      if (e.status == "active") b += "<span class=\"status-active\">Actif</span>";
      else if (e.status == "completed") b += "<span class=\"status-completed\">Complété</span>";
      else b += "<span class=\"status-cancelled\">Annulé</span>";
      b += "</td>\n";
      b += "<td>" + (e.hasCreatedAt ? formatDateFR(e.createdAt) : String("-")) + "</td>\n</tr>\n";
    }
    b += "</tbody>\n</table>\n</div>\n";
  } else {
    b += "<p>Aucun événement spécial actif.</p>\n";
  }
  b += "</div>\n";

  // Gestion des niveaux de couple
  b += "<div class=\"card\">\n<h2 class=\"card-title\">💕 Gestion des niveaux de couple</h2>\n";
  b += "<p class=\"hint\">Configurez les seuils de crédits pour chaque niveau de relation (calculé sur les 30 derniers jours glissants)</p>\n";
  b += "<table class=\"table\">\n<thead>\n<tr>\n";
  b += "<th>Niveau</th><th>Icône</th><th>Seuil minimum (crédits)</th><th>Action</th>\n</tr>\n</thead>\n<tbody>\n";
  int idx = 0;
  for (auto& l : g_levels) {
    b += "<tr>\n<td><strong>" + esc(l.name) + "</strong></td>\n<td>" + esc(l.icon) + "</td>\n<td>\n";
    b += "<form method=\"post\" action=\"/admin/relationship-levels\" class=\"inline-form\">\n";
    b += "<input type=\"hidden\" name=\"levelIndex\" value=\"" + String(idx) + "\" />\n";
    b += "<input type=\"number\" name=\"minCredits\" min=\"0\" step=\"1\" value=\"" + String(l.minCredits) + "\" style=\"width: 100px;\" />\n";
    b += "<button type=\"submit\" class=\"btn-secondary\">Mettre à jour</button>\n</form>\n</td>\n</tr>\n";
    idx++;
  }
  b += "</tbody>\n</table>\n</div>\n";

  // Sauvegarde / restauration des données (data.json compatible version Node)
  b += "<div class=\"card\">\n<h2 class=\"card-title\">💾 Sauvegarde des données</h2>\n";
  b += "<p class=\"hint\">Exporte ou restaure le contenu de data.json (format identique à la version Node.js).</p>\n";
  b += "<p><a href=\"/admin/export\" target=\"_blank\" class=\"btn-secondary\">⬇️ Exporter les données</a></p>\n";
  b += "<form method=\"post\" action=\"/admin/import\" class=\"form\">\n";
  b += "<label>Restaurer depuis un JSON (contenu data.json)<textarea name=\"data\" rows=\"6\" placeholder='{ \"users\": [...] }' required></textarea></label>\n";
  b += "<button type=\"submit\" class=\"btn-primary\">⬆️ Importer</button>\n</form>\n</div>\n";

  b += "</section>";

  return renderPage("Admin", "/admin", false, b, user.displayName, user.credits,
                    user.role == "admin", notif);
}

// ---------------------------------------------------------------------------
// GET /special-events — views/special-events.ejs
// ---------------------------------------------------------------------------
String pageSpecialEvents(User& user, const String& error, const String& success) {
  int notif = countNotifications(user.id);

  // Événements actifs destinés à cet utilisateur (ou aux deux)
  std::vector<SpecialEvent*> events;
  for (auto& e : g_events) {
    if (e.status == "active" && (e.targetUser == "both" || e.targetUser == user.id)) events.push_back(&e);
  }

  String b;
  b.reserve(6000);
  b += "<section class=\"section\">\n<h1 class=\"section-title\">🎯 Événements Spéciaux</h1>\n";
  if (!error.isEmpty()) b += "<div class=\"error\">" + esc(error) + "</div>\n";
  if (!success.isEmpty()) b += "<div class=\"alert alert-success\"><p style=\"margin: 0;\">" + esc(success) + "</p></div>\n";

  if (events.empty()) {
    b += "<div class=\"card\"><p>Aucun événement spécial disponible pour le moment.</p></div>\n";
  } else {
    b += "<div class=\"cards\">\n";
    for (auto* e : events) {
      b += "<div class=\"card\">\n";
      b += "<h2 class=\"card-title\"><span class=\"service-icon\">🎯</span> " + esc(e->title) + "</h2>\n";
      b += "<p class=\"category\">Événement spécial</p>\n";
      b += "<p>" + esc(e->description) + "</p>\n";
      b += "<p class=\"price\">" + String(e->credits) + " ❤️ crédits</p>\n";
      if (!e->assignedTo.isEmpty()) {
        if (e->assignedTo == user.id) {
          if (!e->completedBy.isEmpty()) {
            b += "<p class=\"info\">✅ Vous avez complété cet événement</p>\n";
          } else {
            b += "<p class=\"info\">vous avez accepté cet événement</p>\n";
            b += "<form method=\"post\" action=\"/special-events/" + e->id + "/complete\" class=\"inline-form\">\n";
            b += "<button type=\"submit\" class=\"btn-primary\">J'ai complété l'événement</button>\n</form>\n";
          }
        } else {
          b += "<p class=\"info\">assigné à un autre utilisateur</p>\n";
        }
      } else {
        b += "<form method=\"post\" action=\"/special-events/" + e->id + "/accept\" class=\"inline-form\">\n";
        b += "<button type=\"submit\" class=\"btn-primary\">Accepter l'événement</button>\n</form>\n";
      }
      b += "</div>\n";
    }
    b += "</div>\n";
  }
  b += "</section>";

  return renderPage("Événements Spéciaux", "/special-events", true, b, user.displayName,
                    user.credits, user.role == "admin", notif);
}

// ---------------------------------------------------------------------------
// GET /gift-voucher/:code — views/gift-voucher.ejs (page imprimable autonome)
// ---------------------------------------------------------------------------
String pageGiftVoucher(const Voucher& voucher) {
  String b;
  b.reserve(4000);
  b += R"VH(<div class="voucher">
<h1 class="title">🎁 Bon Cadeau</h1>
<p class="subtitle">Love Market</p>
<div class="decoration"><span>💕</span><span>💝</span><span>💖</span></div>
<div class="heart">💖</div>
<p class="subtitle">Pour vous faire plaisir...</p>
)VH";
  b += "<div class=\"amount\">" + String(voucher.credits) + " ❤️ crédits</div>\n";
  if (!voucher.comment.isEmpty()) b += "<div class=\"comment\">\"" + esc(voucher.comment) + "\"</div>\n";
  b += "<div class=\"code\">" + esc(voucher.code) + "</div>\n";
  b += R"VF(<div class="instructions">
<p>Pour utiliser ce bon, connectez-vous à Love Market et rendez-vous sur la page d'accueil.</p>
<p>Cliquez sur l'icône 🎁 en bas de la page et entrez le code à 4 chiffres.</p>
</div>
<div class="decoration"><span>💗</span><span>💞</span><span>💕</span></div>
</div>)VF";

  String head = R"VHEAD(<title>Bon Cadeau - Love Market</title>
<style>
body {
  font-family: 'Arial', sans-serif;
  background: linear-gradient(135deg, #ffafbd, #ffc3a0);
  margin: 0; padding: 20px;
  display: flex; justify-content: center; align-items: center; min-height: 100vh;
}
.voucher {
  background: white; border-radius: 20px; padding: 40px;
  width: 90%; max-width: 500px; text-align: center;
  box-shadow: 0 10px 30px rgba(0,0,0,0.15);
  position: relative; overflow: hidden;
}
.voucher::before {
  content: ""; position: absolute; top: -50px; right: -50px;
  width: 150px; height: 150px;
  background: rgba(255, 79, 139, 0.1); border-radius: 50%;
}
.voucher::after {
  content: ""; position: absolute; bottom: -50px; left: -50px;
  width: 150px; height: 150px;
  background: rgba(255, 195, 160, 0.1); border-radius: 50%;
}
.title {
  color: #ff4f8b; font-size: 2.5rem; margin: 0 0 20px 0;
  text-shadow: 2px 2px 4px rgba(0,0,0,0.1);
}
.subtitle { color: #666; font-size: 1.2rem; margin-bottom: 30px; }
.heart { font-size: 3rem; margin: 20px 0; color: #ff4f8b; animation: pulse 2s infinite; }
.code {
  background: #fff0f5; padding: 20px; border-radius: 15px;
  font-size: 1.8rem; font-weight: bold; color: #ff4f8b;
  letter-spacing: 2px; margin: 20px 0; border: 2px dashed #ff4f8b;
}
.amount { font-size: 2rem; font-weight: bold; color: #ff4f8b; margin: 20px 0; }
.comment {
  font-style: italic; color: #666; margin: 20px 0;
  padding: 10px; border-left: 3px solid #ff4f8b;
}
.instructions {
  background: #f5f5f5; padding: 15px; border-radius: 10px;
  margin-top: 20px; font-size: 0.9rem; color: #666;
}
.decoration { display: flex; justify-content: space-around; margin: 20px 0; }
.decoration span { font-size: 1.5rem; animation: float 3s ease-in-out infinite; }
.decoration span:nth-child(2) { animation-delay: 0.5s; }
.decoration span:nth-child(3) { animation-delay: 1s; }
@keyframes pulse {
  0% { transform: scale(1); } 50% { transform: scale(1.1); } 100% { transform: scale(1); }
}
@keyframes float {
  0% { transform: translateY(0px); } 50% { transform: translateY(-10px); } 100% { transform: translateY(0px); }
}
@media print {
  body { background: white; padding: 0; }
  .voucher { box-shadow: none; width: 100%; max-width: 100%; border: 1px solid #ccc; }
}
</style>
)VHEAD";

  return renderBarePage(head, b);
}
