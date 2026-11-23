#include "pages.h"
#include "ui.h"

// ---------------------------------------------------------------------------
// Helpers locaux
// ---------------------------------------------------------------------------
static String chipCategory(const Wish& w) {
  return w.category.isEmpty() ? String("") : String("<span class=\"chip\">") + esc(w.category) + "</span>";
}

static String chipDate(const Wish& w) {
  return w.desiredDate.isEmpty() ? String("") : String("<span class=\"chip\">🗓 " ) + esc(w.desiredDate) + "</span>";
}

// ---------------------------------------------------------------------------
// GET /login — choix du profil + mot de passe
// ---------------------------------------------------------------------------
String pageLogin(const String& error) {
  String body;
  body.reserve(3500);
  body += "<main class=\"page\">\n<section class=\"center-card\">\n<div class=\"card card-login\">\n";
  body += "<h1 class=\"title\">Love Market</h1>\n";
  body += "<p class=\"subtitle\" style=\"text-align:center;\">Tes envies. Mes envies. Nos plaisirs.</p>\n";
  if (!error.isEmpty()) {
    body += "<div class=\"error\">" + esc(error) + "</div>\n";
  }
  body += "<form method=\"post\" action=\"/login\" class=\"form\" onsubmit=\"return checkForm()\">\n";
  body += "<input type=\"hidden\" name=\"userId\" id=\"userId\" value=\"\" />\n";
  body += "<div class=\"user-select\">\n";
  body += "<button type=\"button\" class=\"user-btn\" onclick=\"pickUser('Alice', this)\">👩 Alice</button>\n";
  body += "<button type=\"button\" class=\"user-btn\" onclick=\"pickUser('Sylvain', this)\">🧑 Sylvain</button>\n";
  body += "</div>\n";
  body += "<label>Mot de passe<input type=\"password\" name=\"password\" id=\"password\" placeholder=\"Ton mot de passe\" required /></label>\n";
  body += "<button type=\"submit\" class=\"btn-primary btn-full\">Se connecter</button>\n</form>\n";
  body += "</div>\n</section>\n</main>\n";
  body += R"JSEND(<script>
function pickUser(id, btn){
  document.getElementById('userId').value = id;
  document.querySelectorAll('.user-btn').forEach(function(b){ b.classList.remove('selected'); });
  btn.classList.add('selected');
  document.getElementById('password').focus();
}
function checkForm(){
  if (!document.getElementById('userId').value) {
    alert("Choisis d'abord Alice ou Sylvain");
    return false;
  }
  return true;
}
</script>
)JSEND";

  String head = "<title>Connexion - Love Market</title>\n<link rel=\"stylesheet\" href=\"/style.css\"/>\n";
  return renderBarePage(head, body);
}

// ---------------------------------------------------------------------------
// GET /dashboard — accueil chaleureux (pas un tableau de bord administratif)
// ---------------------------------------------------------------------------
String pageHome(User& user, const String& error, const String& success) {
  User* partner = findPartnerOf(user.id);
  RelData rel;
  getRelationshipData(rel);

  // Dernières envies publiées par le partenaire (les plus récentes d'abord)
  std::vector<Wish*> partnerOpen;
  for (size_t i = g_wishes.size(); i > 0; i--) {
    Wish& w = g_wishes[i - 1];
    if (partner && w.userId == partner->id && w.status == "open") partnerOpen.push_back(&w);
  }

  // Engagements en cours me concernant (auteur ou réalisateur)
  std::vector<Wish*> engagements;
  for (size_t i = g_wishes.size(); i > 0; i--) {
    Wish& w = g_wishes[i - 1];
    if (w.status != "accepted") continue;
    if (w.userId == user.id || w.acceptedBy == user.id) engagements.push_back(&w);
  }

  // Derniers moments réalisés
  std::vector<Wish*> moments;
  for (size_t i = g_wishes.size(); i > 0; i--) {
    Wish& w = g_wishes[i - 1];
    if (w.status == "completed") moments.push_back(&w);
  }

  // Message d'accueil contextuel, sans pression
  String message;
  if (!partnerOpen.empty()) {
    message = String(partner ? partner->displayName : "Ton partenaire") + " a peut-être une envie à te faire découvrir.";
  } else if (!engagements.empty()) {
    message = "Un engagement est en cours — doucement, sans pression.";
  } else if (!moments.empty()) {
    message = "Un joli moment vient d'être réalisé !";
  } else {
    message = "Qu'est-ce qui te ferait plaisir en ce moment ?";
  }

  String b;
  b.reserve(11000);

  if (!error.isEmpty()) b += "<div class=\"error\">" + esc(error) + "</div>\n";
  if (!success.isEmpty()) b += "<div class=\"success\">" + esc(success) + "</div>\n";

  b += "<div class=\"home-hero\">\n<h1>Bonjour " + esc(user.displayName) + " 💕</h1>\n";
  b += "<p>" + esc(message) + "</p>\n";
  b += "<a class=\"btn-primary\" href=\"/wishes/new\">✨ Ajouter une envie</a>\n</div>\n";

  // Dernières envies du partenaire
  b += "<section class=\"section\">\n<h2 class=\"section-title\">Les envies de " +
       esc(partner ? partner->displayName : String("ton partenaire")) + "</h2>\n";
  if (partnerOpen.empty()) {
    b += "<div class=\"empty\">Aucune envie publiée pour le moment — c'est le bon moment pour en parler ce soir.</div>\n";
  } else {
    b += "<div class=\"mini-list\">\n";
    size_t shown = 0;
    for (Wish* w : partnerOpen) {
      if (shown >= 3) break;
      shown++;
      b += "<a class=\"mini-card\" href=\"/partner-wishes\">\n<span class=\"m-title\">" + esc(w->title) + "</span><br/>\n";
      b += creditBadge(w->credits);
      if (!w->desiredDate.isEmpty()) b += " <span class=\"chip\">🗓 " + esc(w->desiredDate) + "</span>";
      b += "\n</a>\n";
    }
    b += "</div>\n<p class=\"hint\"><a href=\"/partner-wishes\">Voir toutes ses envies →</a></p>\n";
  }
  b += "</section>\n";

  // Engagements en cours
  b += "<section class=\"section\">\n<h2 class=\"section-title\">Nos engagements en cours</h2>\n";
  if (engagements.empty()) {
    b += "<div class=\"empty\">Aucun engagement en cours. Chaque chose en son temps.</div>\n";
  } else {
    b += "<div class=\"mini-list\">\n";
    size_t shown = 0;
    for (Wish* w : engagements) {
      if (shown >= 3) break;
      shown++;
      User* author = findUser(w->userId);
      User* maker = findUser(w->acceptedBy);
      b += "<a class=\"mini-card\" href=\"/exchanges\">\n<span class=\"m-title\">" + esc(w->title) + "</span><br/>\n";
      b += "<span class=\"chip\">Réalisé par " + esc(maker ? maker->displayName : "?") + "</span> ";
      b += "<span class=\"chip\">Pour " + esc(author ? author->displayName : "?") + "</span> ";
      if (w->hasRealizationReportedAt) b += "<span class=\"status status-accepted\">à confirmer</span>";
      b += "\n</a>\n";
    }
    b += "</div>\n<p class=\"hint\"><a href=\"/exchanges\">Voir nos échanges →</a></p>\n";
  }
  b += "</section>\n";

  // Derniers moments
  b += "<section class=\"section\">\n<h2 class=\"section-title\">Nos derniers moments</h2>\n";
  if (moments.empty()) {
    b += "<div class=\"empty\">Chaque envie réalisée rejoindra notre album des moments.</div>\n";
  } else {
    b += "<div class=\"mini-list\">\n";
    size_t shown = 0;
    for (Wish* w : moments) {
      if (shown >= 2) break;
      shown++;
      b += "<a class=\"mini-card\" href=\"/moments\">\n<span class=\"m-title\">" + esc(w->title) + "</span><br/>\n";
      if (w->hasCompletedAt) b += "<span class=\"chip\">" + esc(formatDateTimeFR(w->completedAt)) + "</span>";
      b += "\n</a>\n";
    }
    b += "</div>\n<p class=\"hint\"><a href=\"/moments\">Voir notre album →</a></p>\n";
  }
  b += "</section>\n";

  // Progression du couple + solde, affichés de manière discrète
  b += "<div class=\"footer-quiet\">\n";
  b += esc(g_levels[rel.levelIndex].icon) + " " + esc(g_levels[rel.levelIndex].name) +
       " · " + String(rel.completedCount) + (rel.completedCount > 1 ? " moments partagés" : " moment partagé");
  if (rel.nextIndex >= 0) {
    int remaining = g_levels[rel.nextIndex].threshold - rel.completedCount;
    b += " · encore " + String(remaining) + " pour être " + esc(g_levels[rel.nextIndex].name);
  }
  b += " · tes crédits : " + String(user.credits) + " ♥\n</div>\n";

  return renderPage("Accueil", "/dashboard", user.id, user.displayName,
                    user.credits, user.role == "admin", b);
}

// ---------------------------------------------------------------------------
// GET /my-wishes — rubrique « Mes envies »
// Sous-sections : disponibles / acceptées par le partenaire / réalisées
// ---------------------------------------------------------------------------
String pageMyWishes(User& user) {
  User* partner = findPartnerOf(user.id);

  String b;
  b.reserve(12000);
  b += "<section class=\"section\">\n";
  b += "<h1 class=\"section-title\">Mes envies</h1>\n";
  b += "<p class=\"subtitle\">Ce que tu aimerais vivre, recevoir ou partager.</p>\n";
  b += "<a class=\"btn-primary btn-full\" href=\"/wishes/new\">✨ Ajouter une envie</a>\n</section>\n";

  // --- Envies disponibles ---
  b += "<section class=\"section\">\n<h2 class=\"section-title\">Envies disponibles</h2>\n";
  bool anyOpen = false;
  for (auto& w : g_wishes) {
    if (w.userId != user.id || w.status != "open") continue;
    anyOpen = true;
    b += "<div class=\"card wish-card\">\n<h3 class=\"wish-title\">" + esc(w.title) + "</h3>\n<div class=\"wish-meta\">\n";
    b += chipCategory(w);
    b += creditBadge(w.credits);
    b += chipDate(w);
    b += "<span class=\"status status-open\">en attente</span>\n</div>\n";
    if (!w.description.isEmpty()) b += "<p class=\"wish-desc\">" + esc(w.description) + "</p>\n";
    b += "<div class=\"actions\">\n";
    b += "<a class=\"btn-secondary\" href=\"/wishes/" + w.id + "/edit\">Modifier</a>\n";
    b += "<form method=\"post\" action=\"/wishes/" + w.id + "/delete\" class=\"inline-form\" "
         "onsubmit=\"return confirm('Supprimer définitivement cette envie ?')\">"
         "<button type=\"submit\" class=\"btn-danger\">Supprimer</button></form>\n";
    b += "</div>\n</div>\n";
  }
  if (!anyOpen) b += "<div class=\"empty\">Aucune envie disponible — publie ta première envie !</div>\n";
  b += "</section>\n";

  // --- Envies acceptées par le partenaire ---
  b += "<section class=\"section\">\n<h2 class=\"section-title\">Envies acceptées par " +
       esc(partner ? partner->displayName : String("ton partenaire")) + "</h2>\n";
  bool anyAccepted = false;
  for (auto& w : g_wishes) {
    if (w.userId != user.id || w.status != "accepted") continue;
    anyAccepted = true;
    User* maker = findUser(w.acceptedBy);
    b += "<div class=\"card wish-card\">\n<h3 class=\"wish-title\">" + esc(w.title) + "</h3>\n<div class=\"wish-meta\">\n";
    b += chipCategory(w);
    b += creditBadge(w.credits);
    b += chipDate(w);
    b += "<span class=\"status status-accepted\">engage" + (maker && maker->id == "Alice" ? String("e") : String("")) + " : " +
         esc(maker ? maker->displayName : "?") + "</span>\n</div>\n";
    if (!w.description.isEmpty()) b += "<p class=\"wish-desc\">" + esc(w.description) + "</p>\n";
    if (w.hasRealizationReportedAt) {
      b += "<div class=\"success\">🎉 Réalisation signalée ! Confirme dans <a href=\"/exchanges\">Nos échanges</a>.</div>\n";
    }
    b += "<div class=\"actions\">\n";
    b += "<form method=\"post\" action=\"/wishes/" + w.id + "/cancel\" class=\"inline-form\" "
         "onsubmit=\"return confirm('Annuler cette envie ? Aucun crédit ne sera attribué.')\">"
         "<button type=\"submit\" class=\"btn-danger\">Annuler cette envie</button></form>\n";
    b += "</div>\n</div>\n";
  }
  if (!anyAccepted) b += "<div class=\"empty\">Aucune envie acceptée pour l'instant.</div>\n";
  b += "</section>\n";

  // --- Envies réalisées ---
  b += "<section class=\"section\">\n<h2 class=\"section-title\">Envies réalisées</h2>\n";
  bool anyCompleted = false;
  for (size_t i = g_wishes.size(); i > 0; i--) {
    Wish& w = g_wishes[i - 1];
    if (w.userId != user.id || w.status != "completed") continue;
    anyCompleted = true;
    User* maker = findUser(w.acceptedBy);
    b += "<div class=\"card wish-card\">\n<h3 class=\"wish-title\">" + esc(w.title) + "</h3>\n<div class=\"wish-meta\">\n";
    b += chipCategory(w);
    if (w.hasCompletedAt) b += "<span class=\"chip\">" + esc(formatDateTimeFR(w.completedAt)) + "</span>\n";
    if (w.credits > 0 && maker) {
      b += "<span class=\"credit-badge\">" + String(w.credits) + " ♥ gagnés par " + esc(maker->displayName) + "</span>\n";
    } else {
      b += creditBadge(w.credits);
    }
    b += "</div>\n";
    if (!w.memoryComment.isEmpty()) b += "<p class=\"wish-desc\">« " + esc(w.memoryComment) + " »</p>\n";
    b += "</div>\n";
  }
  if (!anyCompleted) b += "<div class=\"empty\">Tes envies réalisées apparaîtront ici — et dans <a href=\"/moments\">Nos moments</a>.</div>\n";
  b += "</section>\n";

  return renderPage("Mes envies", "/my-wishes", user.id, user.displayName,
                    user.credits, user.role == "admin", b);
}

// ---------------------------------------------------------------------------
// GET /wishes/new et GET /wishes/{}/edit — création / modification d'une envie
// (modification autorisée uniquement tant que l'envie est disponible)
// ---------------------------------------------------------------------------
String pageWishForm(User& user, Wish* wish) {
  bool isEdit = (wish != nullptr);
  String action = isEdit ? "/wishes/" + wish->id + "/edit" : String("/wishes");
  int credits = isEdit ? wish->credits : 2;
  const char* VALUES[5] = {"gratuite", "petite envie", "envie simple",
                           "envie importante", "grande envie"};

  String b;
  b.reserve(6000);
  b += "<section class=\"section\">\n";
  b += isEdit ? "<h1 class=\"section-title\">Modifier mon envie</h1>\n"
              : "<h1 class=\"section-title\">Nouvelle envie</h1>\n";
  b += "<p class=\"subtitle\">Quelque chose que tu aimerais vivre, recevoir ou partager.</p>\n";
  b += "<div class=\"card\">\n<form method=\"post\" action=\"" + action + "\" class=\"form\">\n";
  b += "<label>Titre *<input type=\"text\" name=\"title\" placeholder=\"Ex : une soirée romantique organisée par toi\" value=\"" +
       (isEdit ? esc(wish->title) : String("")) + "\" required /></label>\n";
  b += "<label>Description<textarea name=\"description\" rows=\"3\" "
       "placeholder=\"Décris ton envie comme tu la ressens\">" +
       (isEdit ? esc(wish->description) : String("")) + "</textarea></label>\n";
  b += "<label>Catégorie<select name=\"category\">" +
       categoryOptions(isEdit ? wish->category : (g_categories.empty() ? String("") : g_categories[0].name)) +
       "</select></label>\n";

  // Valeur en crédits : la récompense que gagnera le réalisateur (0 à 4)
  b += "<div class=\"form-block\"><span class=\"label-text\">Valeur en crédits *</span><div class=\"credit-choices\">\n";
  for (int i = 0; i <= 4; i++) {
    b += "<label><input type=\"radio\" name=\"credits\" value=\"" + String(i) + "\"" +
         (credits == i ? String(" checked") : String("")) + " /><span>♥ " + String(i) +
         " · " + VALUES[i] + "</span></label>\n";
  }
  b += "</div></div>\n";
  b += "<label>Date ou période souhaitée<input type=\"text\" name=\"desiredDate\" "
       "placeholder=\"Ex : dès que possible, le week-end du 12...\" value=\"" +
       (isEdit ? esc(wish->desiredDate) : String("")) + "\" /></label>\n";
  b += "<p class=\"hint\">La valeur en crédits est la récompense que gagnera ton/ta partenaire "
       "en réalisant cette envie. Une envie gratuite est aussi précieuse qu'une autre.</p>\n";
  b += "<button type=\"submit\" class=\"btn-primary btn-full\">" +
       (isEdit ? String("Enregistrer") : String("Publier mon envie")) + "</button>\n";
  b += "<a class=\"btn-secondary btn-full\" href=\"/my-wishes\">Annuler</a>\n";
  b += "</form>\n</div>\n</section>\n";

  return renderPage(isEdit ? "Modifier une envie" : "Nouvelle envie", "/my-wishes",
                    user.id, user.displayName, user.credits, user.role == "admin", b);
}

// ---------------------------------------------------------------------------
// 404
// ---------------------------------------------------------------------------
String page404() {
  String head = "<title>Page introuvable - Love Market</title>\n<link rel=\"stylesheet\" href=\"/style.css\"/>\n";
  String body = "<main class=\"page\"><section class=\"center-card\"><div class=\"card card-login\">"
                "<h1 class=\"title\">Page introuvable</h1>"
                "<p class=\"subtitle\">Le lien demandé n'existe pas sur ce serveur Love Market.</p>"
                "<a class=\"btn-primary\" href=\"/\">Retour à l'accueil</a></div></section></main>";
  return renderBarePage(head, body);
}
