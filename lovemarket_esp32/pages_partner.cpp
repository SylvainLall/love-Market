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

static String statusChip(const Wish& w) {
  if (w.status == "open") return "<span class=\"status status-open\">disponible</span>";
  if (w.status == "accepted") {
    return w.hasRealizationReportedAt
               ? String("<span class=\"status status-accepted\">réalisation signalée</span>")
               : String("<span class=\"status status-accepted\">à réaliser</span>");
  }
  if (w.status == "completed") return "<span class=\"status status-completed\">réalisée</span>";
  return "<span class=\"status status-cancelled\">annulée</span>";
}

// ---------------------------------------------------------------------------
// GET /partner-wishes — rubrique « Ses envies »
// Sous-sections : à découvrir / que j'ai acceptées
// ---------------------------------------------------------------------------
String pagePartnerWishes(User& user) {
  User* partner = findPartnerOf(user.id);
  String pName = partner ? partner->displayName : String("ton partenaire");

  String b;
  b.reserve(12000);
  b += "<section class=\"section\">\n";
  b += "<h1 class=\"section-title\">Les envies de " + esc(pName) + "</h1>\n";
  b += "<p class=\"subtitle\">Découvre ce qui lui ferait plaisir — sans aucune obligation.</p>\n</section>\n";

  // --- Envies à découvrir ---
  b += "<section class=\"section\">\n<h2 class=\"section-title\">Envies à découvrir</h2>\n";
  bool anyOpen = false;
  for (size_t i = g_wishes.size(); i > 0; i--) {
    Wish& w = g_wishes[i - 1];
    if (!partner || w.userId != partner->id || w.status != "open") continue;
    anyOpen = true;
    b += "<div class=\"card wish-card\">\n<h3 class=\"wish-title\">" + esc(w.title) + "</h3>\n<div class=\"wish-meta\">\n";
    b += chipCategory(w);
    b += creditBadge(w.credits);
    b += chipDate(w);
    b += "</div>\n";
    if (!w.description.isEmpty()) b += "<p class=\"wish-desc\">" + esc(w.description) + "</p>\n";
    b += "<div class=\"actions\">\n";
    b += "<form method=\"post\" action=\"/wishes/" + w.id + "/accept\" "
         "onsubmit=\"return confirm('Tu te portes volontaire pour réaliser cette envie ?\\nC\\'est un engagement libre, sans obligation de date.')\">"
         "<button type=\"submit\" class=\"btn-primary\">💗 J'aimerais réaliser cette envie</button></form>\n";
    b += "</div>\n</div>\n";
  }
  if (!anyOpen) b += "<div class=\"empty\">" + esc(pName) + " n'a publié aucune envie disponible pour le moment.</div>\n";
  b += "</section>\n";

  // --- Envies que j'ai acceptées (à réaliser) ---
  b += "<section class=\"section\">\n<h2 class=\"section-title\">Envies que tu as acceptées</h2>\n";
  bool anyMine = false;
  for (auto& w : g_wishes) {
    if (!partner || w.userId != partner->id || w.status != "accepted" || w.acceptedBy != user.id) continue;
    anyMine = true;
    b += "<div class=\"card wish-card\">\n<h3 class=\"wish-title\">" + esc(w.title) + "</h3>\n<div class=\"wish-meta\">\n";
    b += chipCategory(w);
    b += creditBadge(w.credits);
    b += chipDate(w);
    b += statusChip(w);
    b += "</div>\n";
    if (!w.description.isEmpty()) b += "<p class=\"wish-desc\">" + esc(w.description) + "</p>\n";
    b += "<p class=\"hint\">Gère la réalisation dans <a href=\"/exchanges\">Nos échanges</a>.</p>\n</div>\n";
  }
  if (!anyMine) b += "<div class=\"empty\">Tu n'as accepté aucune envie — prends le temps de découvrir les siennes.</div>\n";
  b += "</section>\n";

  return renderPage("Ses envies", "/partner-wishes", user.id, user.displayName,
                    user.credits, user.role == "admin", b);
}

// ---------------------------------------------------------------------------
// GET /exchanges — rubrique « Nos échanges » : les engagements en cours
// ---------------------------------------------------------------------------
String pageExchanges(User& user) {
  String b;
  b.reserve(12000);
  b += "<section class=\"section\">\n";
  b += "<h1 class=\"section-title\">Nos échanges</h1>\n";
  b += "<p class=\"subtitle\">Les envies acceptées, en cours de réalisation.</p>\n</section>\n";

  bool any = false;
  for (auto& w : g_wishes) {
    if (w.status != "accepted") continue;
    if (w.userId != user.id && w.acceptedBy != user.id) continue;
    any = true;
    User* author = findUser(w.userId);     // bénéficiaire
    User* maker = findUser(w.acceptedBy);  // réalisateur
    bool iAmMaker = (w.acceptedBy == user.id);
    bool iAmAuthor = (w.userId == user.id);

    b += "<div class=\"card wish-card\">\n<h3 class=\"wish-title\">" + esc(w.title) + "</h3>\n<div class=\"wish-meta\">\n";
    b += chipCategory(w);
    b += creditBadge(w.credits);
    b += chipDate(w);
    b += statusChip(w);
    b += "</div>\n";
    if (!w.description.isEmpty()) b += "<p class=\"wish-desc\">" + esc(w.description) + "</p>\n";
    b += "<p class=\"hint\">Bénéficiaire : <strong>" + esc(author ? author->displayName : "?") +
         "</strong> · Réalisateur/trice : <strong>" + esc(maker ? maker->displayName : "?") + "</strong></p>\n";

    b += "<div class=\"actions\">\n";
    if (iAmMaker && !w.hasRealizationReportedAt) {
      b += "<form method=\"post\" action=\"/wishes/" + w.id + "/report-done\" class=\"inline-form\">"
           "<button type=\"submit\" class=\"btn-primary\">✅ J'ai réalisé cette envie !</button></form>\n";
    }
    if (iAmAuthor && w.hasRealizationReportedAt) {
      b += "<form method=\"post\" action=\"/wishes/" + w.id + "/confirm\" class=\"inline-form\">"
           "<button type=\"submit\" class=\"btn-primary\">💗 Oui, c'est fait !</button></form>\n";
    }
    if (iAmMaker && w.hasRealizationReportedAt) {
      b += "<span class=\"hint\">En attente de la confirmation de " +
           esc(author ? author->displayName : "?") + ".</span>\n";
    }
    if (iAmAuthor && !w.hasRealizationReportedAt) {
      b += "<span class=\"hint\">En attente de la réalisation par " +
           esc(maker ? maker->displayName : "?") + ".</span>\n";
    }
    if (iAmMaker) {
      b += "<form method=\"post\" action=\"/wishes/" + w.id + "/abandon\" class=\"inline-form\" "
           "onsubmit=\"return confirm('Signaler que tu ne pourras pas réaliser cette envie ?\\nElle redeviendra disponible pour son auteur.')\">"
           "<button type=\"submit\" class=\"btn-danger\">Je ne pourrai pas la réaliser</button></form>\n";
    }
    if (iAmAuthor) {
      b += "<form method=\"post\" action=\"/wishes/" + w.id + "/cancel\" class=\"inline-form\" "
           "onsubmit=\"return confirm('Annuler cette envie ? Aucun crédit ne sera attribué.')\">"
           "<button type=\"submit\" class=\"btn-danger\">Annuler cette envie</button></form>\n";
    }
    b += "</div>\n</div>\n";
  }
  if (!any) b += "<div class=\"empty\">Aucun engagement en cours. Accepte une envie depuis <a href=\"/partner-wishes\">Ses envies</a>.</div>\n";

  // --- Conclues récemment (contexte, lecture seule) ---
  bool anyPast = false;
  String past;
  for (size_t i = g_wishes.size(); i > 0 && past.length() < 3000; i--) {
    Wish& w = g_wishes[i - 1];
    if (w.status != "completed" && w.status != "cancelled") continue;
    if (w.userId != user.id && w.acceptedBy != user.id) continue;
    anyPast = true;
    past += "<div class=\"card wish-card\">\n<h3 class=\"wish-title\">" + esc(w.title) + "</h3>\n<div class=\"wish-meta\">\n";
    past += statusChip(w);
    if (w.hasCompletedAt) past += "<span class=\"chip\">" + esc(formatDateTimeFR(w.completedAt)) + "</span>\n";
    if (w.hasCancelledAt) past += "<span class=\"chip\">annulée le " + esc(formatDateTimeFR(w.cancelledAt)) + "</span>\n";
    past += "</div>\n<p class=\"hint\">Consulter la suite dans <a href=\"/moments\">Nos moments</a>.</p>\n</div>\n";
    if (past.length() > 2500) break; // légèreté : 3-4 cartes suffisent
  }
  if (anyPast) {
    b += "<section class=\"section\">\n<h2 class=\"section-title\">Conclues récemment</h2>\n" + past + "</section>\n";
  }

  return renderPage("Nos échanges", "/exchanges", user.id, user.displayName,
                    user.credits, user.role == "admin", b);
}

// ---------------------------------------------------------------------------
// GET /moments — rubrique « Nos moments » : l'album des envies réalisées
// ---------------------------------------------------------------------------
String pageMoments(User& user) {
  String b;
  b.reserve(12000);
  b += "<section class=\"section\">\n";
  b += "<h1 class=\"section-title\">Nos moments</h1>\n";
  b += "<p class=\"subtitle\">L'album de nos envies réalisées, à deux.</p>\n</section>\n";

  bool any = false;
  for (size_t i = g_wishes.size(); i > 0; i--) {
    Wish& w = g_wishes[i - 1];
    if (w.status != "completed") continue;
    any = true;
    User* author = findUser(w.userId);
    User* maker = findUser(w.acceptedBy);

    b += "<div class=\"card wish-card\">\n<h3 class=\"wish-title\">" + esc(w.title) + "</h3>\n<div class=\"wish-meta\">\n";
    b += chipCategory(w);
    if (w.hasCompletedAt) b += "<span class=\"chip\">le " + esc(formatDateTimeFR(w.completedAt)) + "</span>\n";
    b += "<span class=\"chip\">Réalisé par " + esc(maker ? maker->displayName : "?") + "</span>\n";
    b += "<span class=\"chip\">Pour " + esc(author ? author->displayName : "?") + "</span>\n";
    if (w.credits > 0 && maker) {
      b += "<span class=\"credit-badge\">" + String(w.credits) + " ♥ pour " + esc(maker->displayName) + "</span>\n";
    } else {
      b += creditBadge(w.credits);
    }
    b += "</div>\n";
    if (!w.description.isEmpty()) b += "<p class=\"wish-desc\">" + esc(w.description) + "</p>\n";
    if (!w.memoryComment.isEmpty()) b += "<p class=\"wish-desc\">💬 « " + esc(w.memoryComment) + " »</p>\n";

    // Commentaire facultatif : les deux partenaires peuvent l'étoffer
    b += "<form method=\"post\" action=\"/moments/" + w.id + "/comment\" class=\"form\">\n";
    b += "<label>Commentaire<input type=\"text\" name=\"comment\" maxlength=\"200\" "
         "placeholder=\"Un mot sur ce moment...\" value=\"" + esc(w.memoryComment) + "\" /></label>\n";
    b += "<button type=\"submit\" class=\"btn-secondary\">Enregistrer le commentaire</button>\n";
    b += "</form>\n</div>\n";
  }
  if (!any) {
    b += "<div class=\"empty\">Aucun moment partagé pour l'instant — chaque envie confirmée rejoindra cet album.</div>\n";
  }

  return renderPage("Nos moments", "/moments", user.id, user.displayName,
                    user.credits, user.role == "admin", b);
}
