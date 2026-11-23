#include "pages.h"
#include "ui.h"

// ---------------------------------------------------------------------------
// Helpers locaux
// ---------------------------------------------------------------------------
static String iconOr(const String& icon, const char* def) {
  return icon.isEmpty() ? String(def) : icon;
}

// ---------------------------------------------------------------------------
// GET /login — views/login.ejs
// Sélection du profil par boutons (Alice / Sylvain) puis mot de passe
// ---------------------------------------------------------------------------
String pageLogin(const String& error) {
  String body;
  body.reserve(3500);
  body += "<header class=\"topbar\"><div class=\"logo\">💕 Love Market</div></header>\n";
  body += "<main class=\"page\">\n<section class=\"center-card\">\n<div class=\"card card-login\">\n";
  body += "<h1 class=\"title\">Bienvenue sur Love Market</h1>\n";
  body += "<p class=\"subtitle\">Choisis ton profil puis entre ton mot de passe 💕</p>\n";
  if (!error.isEmpty()) {
    body += "<div class=\"error\">" + esc(error) + "</div>\n";
  }
  body += "<form method=\"post\" action=\"/login\" class=\"form\" onsubmit=\"return checkForm()\">\n";
  body += "<input type=\"hidden\" name=\"userId\" id=\"userId\" value=\"\" />\n";
  body += "<div class=\"user-select\">\n";
  body += "<button type=\"button\" class=\"user-btn\" onclick=\"pickUser('Alice', this)\">👩 Alice</button>\n";
  body += "<button type=\"button\" class=\"user-btn\" onclick=\"pickUser('Sylvain', this)\">👨 Sylvain</button>\n";
  body += "</div>\n";
  body += "<label>Mot de passe<input type=\"password\" name=\"password\" id=\"password\" placeholder=\"Ton mot de passe\" required /></label>\n";
  body += "<button type=\"submit\" class=\"btn-primary\">Se connecter</button>\n</form>\n";
  body += "<div class=\"hint\" style=\"margin-top: 20px;\"><p><em>Contacte ton partenaire pour obtenir ton mot de passe.</em></p></div>\n";
  body += "</div>\n</section>\n</main>\n";
  body += "<script>\n";
  body += "function pickUser(id, btn){\n";
  body += "  document.getElementById('userId').value = id;\n";
  body += "  document.querySelectorAll('.user-btn').forEach(function(b){ b.classList.remove('selected'); });\n";
  body += "  btn.classList.add('selected');\n";
  body += "  document.getElementById('password').focus();\n";
  body += "}\n";
  body += "function checkForm(){\n";
  body += "  if (!document.getElementById('userId').value) {\n";
  body += "    alert(\"Choisis d'abord Alice ou Sylvain\");\n";
  body += "    return false;\n";
  body += "  }\n";
  body += "  return true;\n";
  body += "}\n";
  body += "</script>\n";

  String head = "<title>Connexion - Love Market</title>\n<link rel=\"stylesheet\" href=\"/style.css\"/>\n";
  head += "<style>\n";
  head += ".user-select{display:flex;gap:12px;margin-bottom:16px;}\n";
  head += ".user-btn{flex:1;padding:16px 10px;font-size:1.05rem;font-weight:600;border:2px solid #e4d7e9;border-radius:14px;background:#fff;cursor:pointer;transition:border-color .2s, background .2s, box-shadow .2s;}\n";
  head += ".user-btn:hover{border-color:#e91e63;}\n";
  head += ".user-btn.selected{border-color:#e91e63;background:#fce4ec;box-shadow:0 2px 10px rgba(233,30,99,.25);}\n";
  head += "</style>\n";
  return renderBarePage(head, body);
}

// ---------------------------------------------------------------------------
// GET /dashboard — views/dashboard.ejs
// ---------------------------------------------------------------------------
String pageDashboard(User& user, const String& error, const String& success) {
  User* partner = findPartnerOf(user.id);
  RelData rel;
  getRelationshipData(rel);
  int notif = countNotifications(user.id);

  // Compteurs
  int myServicesCount = 0, partnerServicesCount = 0;
  for (auto& s : g_services) {
    if (s.sellerId == user.id) myServicesCount++;
    else if (partner && s.sellerId == partner->id) partnerServicesCount++;
  }

  // Achats / demandes en attente me concernant + offres en attente
  struct Enriched { Offer* o; Service* svc; User* buyer; };
  std::vector<Enriched> purchases, requests;
  std::vector<Offer*> pendingOffers;
  for (auto& o : g_offers) {
    if (o.toUserId != user.id || o.status != "pending") continue;
    if (o.type == "purchase") {
      Enriched e = {&o, o.serviceId.isEmpty() ? nullptr : findService(o.serviceId), findUser(o.fromUserId)};
      purchases.push_back(e);
    } else if (o.type == "request") {
      Enriched e = {&o, nullptr, findUser(o.fromUserId)};
      requests.push_back(e);
    }
    pendingOffers.push_back(&o);
  }

  String b;
  b.reserve(14000);

  // Alertes : achats de mes services
  if (!purchases.empty()) {
    b += "<div class=\"alert alert-success\" style=\"border-left: 5px solid #4CAF50;\">\n";
    b += "<h2 style=\"margin-top: 0;\">🎉 Nouvelles demandes d'achat de tes services !</h2>\n";
    for (auto& e : purchases) {
      b += "<div class=\"purchase-notification\">\n";
      b += "<p style=\"font-size: 1.1rem; margin: 8px 0;\"><strong>" + esc(e.buyer ? e.buyer->displayName : "?") + "</strong> a acheté : <strong>"
           + esc(e.svc ? iconOr(e.svc->icon, "💗") : String("💗")) + " "
           + esc(e.svc ? e.svc->title : String("#") + e.o->serviceId) + "</strong> pour <strong>"
           + String(e.o->offeredPrice) + " ❤️</strong></p>\n";
      b += "<p style=\"color: #666; margin: 4px 0;\">⚠️ Les crédits ont déjà été débités de "
           + esc(e.buyer ? e.buyer->displayName : "?") + ".</p>\n";
      b += "<form method=\"post\" action=\"/offers/" + e.o->id + "/respond\" style=\"margin-top: 8px;\">\n";
      b += "<button type=\"submit\" name=\"action\" value=\"accept\" class=\"btn-primary\">✅ Accepter et recevoir "
           + String(e.o->offeredPrice) + " ❤️</button>\n";
      b += "<button type=\"submit\" name=\"action\" value=\"reject\" class=\"btn-secondary\" style=\"margin-left: 8px;\">❌ Refuser (remboursement automatique)</button>\n";
      b += "</form>\n</div>\n";
    }
    b += "</div>\n";
  }

  // Alertes : demandes personnalisées
  if (!requests.empty()) {
    b += "<div class=\"alert alert-info\" style=\"border-left: 5px solid #2196F3;\">\n";
    b += "<h2 style=\"margin-top: 0;\">✨ Nouvelles demandes personnalisées !</h2>\n";
    b += "<p style=\"color: #1976D2; font-weight: bold; margin: 8px 0;\">📥 On te demande de faire quelque chose</p>\n";
    for (auto& e : requests) {
      b += "<div class=\"purchase-notification\">\n";
      b += "<p style=\"font-size: 1.1rem; margin: 8px 0;\"><strong>" + esc(e.buyer ? e.buyer->displayName : "?") + "</strong> demande : <strong>"
           + esc(iconOr(e.o->serviceIcon, "💗")) + " " + esc(e.o->serviceTitle) + "</strong> pour <strong>"
           + String(e.o->offeredPrice) + " ❤️</strong></p>\n";
      if (!e.o->comment.isEmpty()) b += "<p class=\"comment\" style=\"margin: 4px 0;\">« " + esc(e.o->comment) + " »</p>\n";
      b += "<form method=\"post\" action=\"/offers/" + e.o->id + "/respond\" style=\"margin-top: 8px;\">\n";
      b += "<button type=\"submit\" name=\"action\" value=\"accept\" class=\"btn-primary\">✅ Accepter et recevoir "
           + String(e.o->offeredPrice) + " ❤️</button>\n";
      b += "<button type=\"submit\" name=\"action\" value=\"reject\" class=\"btn-secondary\">❌ Refuser</button>\n";
      b += "<a href=\"/negotiations\" class=\"btn-secondary\">🔄 Faire une contre-proposition</a>\n";
      b += "</form>\n</div>\n";
    }
    b += "</div>\n";
  }

  // Grille principale
  b += "<section class=\"grid\">\n";
  // Carte niveau de couple (cliquable vers l'historique)
  b += "<a href=\"/relationship-history\" style=\"text-decoration: none; color: inherit;\">\n";
  b += "<div class=\"card\" style=\"grid-column: span 2; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white;\">\n";
  b += "<h2 class=\"card-title\" style=\"color: white;\">" + esc(g_levels[rel.levelIndex].icon) + " Niveau de couple : "
       + esc(g_levels[rel.levelIndex].name) + "</h2>\n";
  b += "<p style=\"font-size: 1.2rem; margin: 10px 0;\"><strong>" + String(rel.totalCredits)
       + " crédits</strong> échangés sur les 30 derniers jours</p>\n";
  if (rel.nextIndex >= 0) {
    b += "<div style=\"background: rgba(255,255,255,0.2); padding: 10px; border-radius: 4px; margin-top: 10px;\">\n";
    b += "<p style=\"margin: 0; font-size: 0.9rem;\">Prochain niveau : <strong>" + esc(g_levels[rel.nextIndex].icon) + " "
         + esc(g_levels[rel.nextIndex].name) + "</strong></p>\n";
    b += "<p style=\"margin: 5px 0 0 0; font-size: 0.9rem;\">Encore <strong>"
         + String(g_levels[rel.nextIndex].minCredits - rel.totalCredits) + " crédits</strong> à échanger</p>\n";
    b += "</div>\n";
  } else {
    b += "<p style=\"margin-top: 10px; font-size: 0.9rem;\">🎉 Niveau maximum atteint !</p>\n";
  }
  b += "</div>\n</a>\n";

  // Carte bienvenue
  b += "<div class=\"card\">\n<h2 class=\"card-title\">Bonjour " + esc(user.displayName) + " 💕</h2>\n";
  b += "<p>Tu as <strong>" + String(user.credits) + " ❤️ crédits</strong>.</p>\n";
  if (partner) b += "<p>Partenaire : <strong>" + String(partner->credits) + " ❤️</strong></p>\n";
  b += "</div>\n";

  // Carte "Votre univers"
  b += "<div class=\"card\">\n<h3 class=\"card-title\">Votre univers</h3>\n<ul class=\"list\">\n";
  b += "<li>Services que tu proposes : <strong>" + String(myServicesCount) + "</strong></li>\n";
  b += "<li>Services que ton partenaire propose : <strong>" + String(partnerServicesCount) + "</strong></li>\n";
  b += "</ul>\n<a class=\"btn-secondary\" href=\"/shop\">Voir la boutique de ton partenaire</a>\n";
  b += "<a class=\"btn-primary\" href=\"/request-service\">✨ Demander un service</a>\n";
  b += "<a class=\"btn-primary\" href=\"/offer-service\">💡 Proposer un service</a>\n</div>\n";

  // Offres en attente
  b += "<div class=\"card\">\n<h3 class=\"card-title\">Offres en attente</h3>\n";
  if (pendingOffers.empty()) {
    b += "<p>Aucune offre en attente. Tu peux créer un service ou en acheter un !</p>\n";
  } else {
    b += "<ul class=\"list offers\">\n";
    for (auto* o : pendingOffers) {
      b += "<li>Offre n°" + o->id + " – proposition de <strong>" + String(o->offeredPrice)
           + " ❤️</strong> pour le service <strong>#" + o->serviceId + "</strong></li>\n";
    }
    b += "</ul>\n<a href=\"/negotiations\" class=\"btn-secondary\">Voir les négociations</a>\n";
  }
  b += "</div>\n</section>\n";

  // Don de crédits
  b += "<section class=\"section\">\n<div class=\"card\">\n<h3 class=\"card-title\">💝 Faire un don de crédits</h3>\n";
  if (partner) {
    b += "<p>Offrir des crédits à <strong>" + esc(partner->displayName) + "</strong> (il/elle a actuellement <strong>"
         + String(partner->credits) + " ❤️</strong>)</p>\n";
  }
  b += "<form method=\"post\" action=\"/donate-credits\" class=\"form\">\n";
  b += "<label>Montant à offrir<input type=\"number\" name=\"amount\" min=\"1\" max=\"" + String(user.credits)
       + "\" placeholder=\"Ex: 20\" required /></label>\n";
  b += "<label>Message (optionnel)<textarea name=\"message\" rows=\"2\" placeholder=\"Un petit mot d'accompagnement...\"></textarea></label>\n";
  b += "<button type=\"submit\" class=\"btn-primary\">Faire le don</button>\n</form>\n</div>\n</section>\n";

  // Utilisation d'un bon cadeau (la version Node affichait cette section en double)
  b += R"HTMLEND(<section class="section" id="voucher-section">
<div class="card">
<h2 class="card-title">🎁 Utiliser un bon cadeau</h2>
)HTMLEND";
  if (!error.isEmpty()) b += "<div class=\"error\">" + esc(error) + "</div>\n";
  if (!success.isEmpty()) b += "<div class=\"alert alert-success\"><p style=\"margin: 0;\">" + esc(success) + "</p></div>\n";
  b += R"HTMLEND(<form method="post" action="/redeem-voucher" class="form">
<label>Code du bon cadeau (4 chiffres)<input type="text" name="code" placeholder="1234" pattern="\d{4}" title="Entrez exactement 4 chiffres" required /></label>
<button type="submit" class="btn-primary">Valider le code</button>
</form>
</div>
</section>
</main>
<footer style="height: 100px; display: flex; justify-content: center; align-items: center;">
<div class="gift-icon-container"><a href="#voucher-section" title="Bons cadeaux" style="display: block;"><span style="font-size: 2rem;">🎁</span></a></div>
</footer>
<script>
document.addEventListener('DOMContentLoaded', function() {
  var successAlert = document.querySelector('.alert-success');
  if (successAlert) {
    successAlert.style.animation = 'pulse 2s infinite';
    var hearts = ['💖', '💕', '💗', '💝'];
    for (var i = 0; i < 10; i++) {
      var heart = document.createElement('div');
      heart.innerHTML = hearts[Math.floor(Math.random() * hearts.length)];
      heart.style.position = 'fixed'; heart.style.fontSize = '2rem';
      heart.style.left = (Math.random() * 100) + 'vw'; heart.style.top = '100vh';
      heart.style.zIndex = '1000'; heart.style.userSelect = 'none';
      document.body.appendChild(heart);
      var animation = heart.animate([
        { transform: 'translateY(0)', opacity: 1 },
        { transform: 'translateY(-100vh)', opacity: 0 }
      ], { duration: Math.random() * 3000 + 2000, easing: 'ease-out' });
      animation.onfinish = function() { heart.remove(); };
    }
  }
});
</script>
</body>
</html>)HTMLEND";

  // Shell : head + topbar/nav puis corps (b contient la fermeture complète du document)
  String html;
  html.reserve(b.length() + 2048);
  html += "<!DOCTYPE html>\n<html lang=\"fr\">\n<head>\n<meta charset=\"UTF-8\"/>\n<title>Tableau de bord - Love Market</title>\n";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n";
  html += "<link rel=\"icon\" type=\"image/svg+xml\" href=\"/favicon.svg\"/>\n<link rel=\"stylesheet\" href=\"/style.css\"/>\n</head>\n<body>\n";
  html += "<header class=\"topbar\">\n<div class=\"logo\">💕 Love Market</div>\n<nav class=\"nav\">\n";
  html += "<a href=\"/dashboard\" title=\"Accueil\" class=\"nav-active\">🏠<span class=\"nav-text\"> Accueil</span></a>\n";
  html += "<a href=\"/shop\" title=\"Boutique\">🛍️<span class=\"nav-text\"> Boutique</span></a>\n";
  html += "<a href=\"/my-services\" title=\"Mes services\">🤝<span class=\"nav-text\"> Mes services</span></a>\n";
  html += "<a href=\"/special-events\" title=\"Événements spéciaux\">🎯<span class=\"nav-text\"> Événements</span></a>\n";
  html += "<a href=\"/negotiations\" title=\"Négociations\" class=\"nav-link-with-badge\">💬<span class=\"nav-text\"> Négociations</span>";
  if (notif > 0) html += "<span class=\"notification-badge\">" + String(notif) + "</span>";
  html += "</a>\n";
  if (user.role == "admin") html += "<a href=\"/admin\" class=\"admin-link\" title=\"Admin\">⚙️<span class=\"nav-text\"> Admin</span></a>\n";
  html += "</nav>\n<div class=\"user-info\">\n<span class=\"credits\">" + String(user.credits) + " ❤️</span>\n";
  html += "<span class=\"user-name\">" + esc(user.displayName) + "</span>\n";
  html += "<a href=\"/logout\" class=\"logout\">Déconnexion</a>\n</div>\n</header>\n";
  html += "<main class=\"page\">\n";
  html += b;
  return html;
}

// ---------------------------------------------------------------------------
// GET /shop — views/shop.ejs
// ---------------------------------------------------------------------------
String pageShop(User& user) {
  User* partner = findPartnerOf(user.id);
  int notif = countNotifications(user.id);

  String b;
  b.reserve(9000);
  b += "<section class=\"section\">\n<div class=\"shop-header\">\n";
  b += "<h1 class=\"section-title\"> Boutique de <span style=\"color: #e91e63;\">"
       + esc(partner ? partner->displayName : String("partenaire")) + "</span> 💝</h1>\n";
  if (partner) {
    b += "<div class=\"user-credits\"><p>Tes crédits : <strong>" + String(user.credits) + " ❤️</strong> — Crédits de ton/ta partenaire <strong>"
         + String(partner->credits) + " ❤️</strong></p></div>\n";
  }
  b += "</div>\n";

  // Services du partenaire
  std::vector<Service*> services;
  if (partner) for (auto& s : g_services) if (s.sellerId == partner->id) services.push_back(&s);

  if (services.empty()) {
    b += "<div class=\"empty-shop\"><p>Ton partenaire n'a encore rien mis en vente. Tu peux lui suggérer une idée douce.</p></div>\n";
  } else {
    b += "<div class=\"product-list\">\n";
    for (auto* s : services) {
      b += "<div class=\"product-item\">\n<div class=\"product-main\">\n<div class=\"product-icon\">\n";
      b += "<span class=\"service-icon\" style=\"font-size: 4rem;\">" + esc(iconOr(s->icon, "💗")) + "</span>\n";
      b += "</div>\n<div class=\"product-details\">\n<h3 class=\"product-title\">" + esc(s->title) + "</h3>\n";
      b += "<p class=\"product-category\">" + esc(s->category) + "</p>\n";
      if (!s->description.isEmpty()) b += "<p class=\"product-description\">" + esc(s->description) + "</p>\n";
      b += "</div>\n</div>\n<div class=\"product-footer\">\n<div class=\"product-price\">\n";
      b += "<span class=\"price-amount\">" + String(s->price) + " ❤️</span>\n</div>\n";
      b += "<div class=\"product-actions\">\n";
      b += "<form method=\"post\" action=\"/services/" + s->id + "/buy\" class=\"inline-form\" onsubmit=\"return confirmPurchase(" + String(s->price) + ", " + String(user.credits) + ")\">\n";
      b += "<button type=\"submit\" class=\"btn-primary btn-buy\">Acheter</button>\n</form>\n";
      b += "<form method=\"post\" action=\"/services/" + s->id + "/offer\" class=\"offer-form\">\n";
      b += "<input type=\"number\" name=\"offeredPrice\" min=\"0\" step=\"1\" placeholder=\"Ta proposition\" required />\n";
      b += "<input type=\"text\" name=\"comment\" placeholder=\"Ajoute un petit mot\" />\n";
      b += "<button type=\"submit\" class=\"btn-secondary btn-negotiate\">Négocier</button>\n</form>\n";
      b += "</div>\n</div>\n</div>\n";
    }
    b += "</div>\n";
  }
  b += "</section>\n";
  b += R"JSSCRIPT(<script>
function confirmPurchase(price, currentCredits) {
  if (currentCredits < price) {
    alert("Crédits insuffisants !\n\nPrix : " + price + " ❤️\nVotre solde : " + currentCredits + " ❤️\nIl vous manque " + (price - currentCredits) + " ❤️");
    return false;
  }
  var newBalance = currentCredits - price;
  var message =
    "Êtes-vous sûr de vouloir acheter ce service ?\n\n" +
    "Solde actuel : " + currentCredits + " ❤️\n" +
    "Prix : " + price + " ❤️\n" +
    "Nouveau solde : " + newBalance + " ❤️\n\n" +
    "Les crédits seront débités immédiatement.\n" +
    "Votre partenaire devra accepter la demande.\n" +
    "Si refusé, vous serez remboursé.";
  return confirm(message);
}
</script>
</body>
</html>)JSSCRIPT";

  String html;
  html.reserve(b.length() + 2048);
  html += "<!DOCTYPE html>\n<html lang=\"fr\">\n<head>\n<meta charset=\"UTF-8\"/>\n<title>Boutique - Love Market</title>\n";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n";
  html += "<link rel=\"icon\" type=\"image/svg+xml\" href=\"/favicon.svg\"/>\n<link rel=\"stylesheet\" href=\"/style.css\"/>\n</head>\n<body>\n";
  html += "<header class=\"topbar\">\n<div class=\"logo\">💕 Love Market</div>\n<nav class=\"nav\">\n";
  html += "<a href=\"/dashboard\" title=\"Accueil\">🏠<span class=\"nav-text\"> Accueil</span></a>\n";
  html += "<a href=\"/shop\" title=\"Boutique\" class=\"nav-active\">🛍️<span class=\"nav-text\"> Boutique</span></a>\n";
  html += "<a href=\"/my-services\" title=\"Mes services\">🤝<span class=\"nav-text\"> Mes services</span></a>\n";
  html += "<a href=\"/negotiations\" title=\"Négociations\" class=\"nav-link-with-badge\">💬<span class=\"nav-text\"> Négociations</span>";
  if (notif > 0) html += "<span class=\"notification-badge\">" + String(notif) + "</span>";
  html += "</a>\n";
  if (user.role == "admin") html += "<a href=\"/admin\" class=\"admin-link\" title=\"Admin\">⚙️<span class=\"nav-text\"> Admin</span></a>\n";
  html += "</nav>\n<div class=\"user-info\">\n<span class=\"credits\">" + String(user.credits) + " ❤️</span>\n";
  html += "<span class=\"user-name\">" + esc(user.displayName) + "</span>\n";
  html += "<a href=\"/logout\" class=\"logout\">Déconnexion</a>\n</div>\n</header>\n<main class=\"page\">\n";
  String body = b;
  int idx = body.lastIndexOf("</body>");
  if (idx > 0) body = body.substring(0, idx);
  html += body;
  html += "</main>\n</body>\n</html>";
  return html;
}

// ---------------------------------------------------------------------------
// GET /my-services — views/my-services.ejs
// ---------------------------------------------------------------------------
String pageMyServices(User& user) {
  int notif = countNotifications(user.id);

  String b;
  b.reserve(9000);
  b += "<section class=\"section\">\n<h1 class=\"section-title\">Mes services 🌸</h1>\n";
  b += "<div class=\"card\">\n<h2 class=\"card-title\">Créer un nouveau service</h2>\n";
  b += "<form method=\"post\" action=\"/services\" class=\"form\">\n";
  b += "<label>Titre<input type=\"text\" name=\"title\" placeholder=\"Petit déjeuner au lit\" required /></label>\n";
  b += "<label>Description<textarea name=\"description\" rows=\"2\" placeholder=\"Décris ton service\"></textarea></label>\n";
  b += "<label>Catégorie<select name=\"category\">" + categoryOptions("Romantique") + "</select></label>\n";
  b += "<label>Icône<select name=\"icon\">" + iconOptions("💗") + "</select></label>\n";
  b += "<label>Prix (en crédits ❤️)<input type=\"number\" name=\"price\" min=\"0\" step=\"1\" value=\"20\" /></label>\n";
  b += "<button type=\"submit\" class=\"btn-primary\">Créer le service</button>\n</form>\n</div>\n";
  b += "<div class=\"cards\">\n";

  std::vector<Service*> mine;
  for (auto& s : g_services) if (s.sellerId == user.id) mine.push_back(&s);

  if (mine.empty()) b += "<p>Tu ne proposes encore rien. Crée ton premier service !</p>\n";
  for (auto* s : mine) {
    b += "<div class=\"card\">\n<h2 class=\"card-title\"><span class=\"service-icon\">" + esc(iconOr(s->icon, "💗")) + "</span> " + esc(s->title) + "</h2>\n";
    b += "<p class=\"category\">" + esc(s->category) + "</p>\n";
    if (!s->description.isEmpty()) b += "<p>" + esc(s->description) + "</p>\n";
    b += "<p class=\"price\">" + String(s->price) + " ❤️</p>\n";
    b += "<form method=\"get\" action=\"/services/" + s->id + "/edit\" class=\"inline-form\"><button type=\"submit\" class=\"btn-secondary\">Modifier</button></form>\n";
    b += "<form method=\"post\" action=\"/services/" + s->id + "/delete\" class=\"inline-form\"><button type=\"submit\" class=\"btn-secondary\">Supprimer</button></form>\n";
    b += "</div>\n";
  }
  b += "</div>\n</section>\n</main>\n</body>\n</html>";

  String html;
  html.reserve(b.length() + 2048);
  html += "<!DOCTYPE html>\n<html lang=\"fr\">\n<head>\n<meta charset=\"UTF-8\"/>\n<title>Mes services - Love Market</title>\n";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n";
  html += "<link rel=\"icon\" type=\"image/svg+xml\" href=\"/favicon.svg\"/>\n<link rel=\"stylesheet\" href=\"/style.css\"/>\n</head>\n<body>\n";
  html += "<header class=\"topbar\">\n<div class=\"logo\">💕 Love Market</div>\n<nav class=\"nav\">\n";
  html += "<a href=\"/dashboard\" title=\"Accueil\">🏠<span class=\"nav-text\"> Accueil</span></a>\n";
  html += "<a href=\"/shop\" title=\"Boutique\">🛍️<span class=\"nav-text\"> Boutique</span></a>\n";
  html += "<a href=\"/my-services\" title=\"Mes services\" class=\"nav-active\">🤝<span class=\"nav-text\"> Mes services</span></a>\n";
  html += "<a href=\"/negotiations\" title=\"Négociations\" class=\"nav-link-with-badge\">💬<span class=\"nav-text\"> Négociations</span>";
  if (notif > 0) html += "<span class=\"notification-badge\">" + String(notif) + "</span>";
  html += "</a>\n";
  if (user.role == "admin") html += "<a href=\"/admin\" class=\"admin-link\" title=\"Admin\">⚙️<span class=\"nav-text\"> Admin</span></a>\n";
  html += "</nav>\n<div class=\"user-info\">\n<span class=\"credits\">" + String(user.credits) + " ❤️</span>\n";
  html += "<span class=\"user-name\">" + esc(user.displayName) + "</span>\n";
  html += "<a href=\"/logout\" class=\"logout\">Déconnexion</a>\n</div>\n</header>\n<main class=\"page\">\n";
  String body = b;
  int idx = body.lastIndexOf("</body>");
  if (idx > 0) body = body.substring(0, idx);
  html += body;
  html += "</body>\n</html>";
  return html;
}

// ---------------------------------------------------------------------------
// GET /services/:id/edit — views/edit-service.ejs
// ---------------------------------------------------------------------------
String pageEditService(User& user, Service& service) {
  int notif = countNotifications(user.id);

  String b;
  b.reserve(5000);
  b += "<section class=\"section\">\n<h1 class=\"section-title\">Modifier le service</h1>\n";
  b += "<div class=\"card\">\n<form method=\"post\" action=\"/services/" + service.id + "/edit\" class=\"form\">\n";
  b += "<label>Titre<input type=\"text\" name=\"title\" value=\"" + esc(service.title) + "\" required /></label>\n";
  b += "<label>Description<textarea name=\"description\" rows=\"2\">" + esc(service.description) + "</textarea></label>\n";
  b += "<label>Catégorie<select name=\"category\">" + categoryOptions(service.category) + "</select></label>\n";
  b += "<label>Icône<select name=\"icon\">" + iconOptions(service.icon) + "</select></label>\n";
  b += "<label>Prix (en crédits ❤️)<input type=\"number\" name=\"price\" min=\"0\" step=\"1\" value=\"" + String(service.price) + "\" /></label>\n";
  b += "<button type=\"submit\" class=\"btn-primary\">Enregistrer</button>\n";
  b += "<a href=\"/my-services\" class=\"btn-secondary\">Annuler</a>\n</form>\n</div>\n</section>";

  return renderPage("Modifier un service", "/my-services", false, b, user.displayName,
                    user.credits, user.role == "admin", notif);
}

String page404() {
  String head = "<title>Page introuvable - Love Market</title>\n<link rel=\"stylesheet\" href=\"/style.css\"/>\n";
  String body = "<main class=\"page\"><section class=\"center-card\"><div class=\"card card-login\">"
                "<h1 class=\"title\">Page introuvable</h1>"
                "<p class=\"subtitle\">Le lien demandé n'existe pas sur ce serveur Love Market.</p>"
                "<a class=\"btn-primary\" href=\"/\">Retour à l'accueil</a></div></section></main>";
  return renderBarePage(head, body);
}
