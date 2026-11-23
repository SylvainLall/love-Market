#include "pages.h"
#include "ui.h"
#include <algorithm>

// ---------------------------------------------------------------------------
// GET /negotiations — views/negotiations.ejs
// ---------------------------------------------------------------------------
String pageNegotiations(User& user) {
  int notif = countNotifications(user.id);

  // Offres liées à l'utilisateur
  std::vector<Offer*> related;
  for (auto& o : g_offers) {
    if (o.fromUserId == user.id || o.toUserId == user.id) related.push_back(&o);
  }
  // Tri par id décroissant (plus récent d'abord), comme l'original
  std::sort(related.begin(), related.end(), [](Offer* a, Offer* b) {
    return a->id.toInt() > b->id.toInt();
  });

  // Groupement par rootOfferId (ou id propre), dans l'ordre de rencontre
  std::vector<std::vector<Offer*>> groups;
  for (auto* o : related) {
    String gid = o->rootOfferId.isEmpty() ? o->id : o->rootOfferId;
    bool found = false;
    for (auto& g : groups) if (!g.empty() && ((g[0]->rootOfferId.isEmpty() ? g[0]->id : g[0]->rootOfferId) == gid)) {
      g.push_back(o); found = true; break;
    }
    if (!found) groups.push_back(std::vector<Offer*>{o});
  }

  String b;
  b.reserve(16000);
  b += "<section class=\"section\">\n<h1 class=\"section-title\">Négociations 💌</h1>\n";
  if (groups.empty()) b += "<p>Aucune négociation pour le moment.</p>\n";
  b += "<div class=\"cards\">\n";

  auto statusText = [](const String& st) -> String {
    if (st == "pending") return "en attente";
    if (st == "accepted") return "acceptée";
    if (st == "rejected") return "refusée";
    if (st == "countered") return "contre-proposition";
    if (st == "realized") return "réalisé";
    return st;
  };

  for (auto& group : groups) {
    // Ordre chronologique à l'intérieur du groupe
    std::sort(group.begin(), group.end(), [](Offer* a, Offer* b) {
      return a->id.toInt() < b->id.toInt();
    });
    Offer* first = group[0];

    bool isProvider = (first->toUserId == user.id && first->originalRequesterId != user.id);

    String borderStyle;
    if (first->type == "request") borderStyle = isProvider ? "#2196F3" : "#FF9800";
    else if (first->type == "purchase") borderStyle = "#4CAF50";

    b += "<div class=\"card negotiation-group\"" + (borderStyle.isEmpty() ? String("") : String(" style=\"border-left: 5px solid " + borderStyle + ";\"")) + ">\n";
    b += "<h2 class=\"card-title\">";
    if (first->type == "purchase") b += "🛍️ Demande d'achat #" + first->id;
    else if (first->type == "request") {
      if (isProvider) b += "📥 Demande de service #" + first->id + " <span style=\"color: #2196F3; font-size: 0.9rem;\">(on te demande)</span>";
      else b += "📤 Proposition de service #" + first->id + " <span style=\"color: #FF9800; font-size: 0.9rem;\">(tu proposes)</span>";
    } else b += "💬 Négociation #" + first->id;
    if (group.size() > 1) b += "<span class=\"badge\" style=\"background: #fff9c4; margin-left: 8px;\">" + String((int)group.size()) + " offres</span>";
    b += "</h2>\n";

    int index = 0;
    for (auto* o : group) {
      bool isCounter = index > 0;
      b += "<div class=\"offer-item" + String(isCounter ? " counter-offer-item" : "") + "\""
           + (isCounter ? String(" style=\"margin-top: 16px; padding-top: 16px; border-top: 2px dashed #e0e0e0;\"") : String(""))
           + ">\n";
      if (isCounter) b += "<p style=\"color: #ff6b9d; font-weight: 600; margin: 0 0 8px 0;\">🔄 Contre-proposition #" + o->id + "</p>\n";

      // Service concerné
      Service* svc = o->serviceId.isEmpty() ? nullptr : findService(o->serviceId);
      b += "<p>Service : <strong>";
      if (!o->serviceTitle.isEmpty()) b += esc(o->serviceIcon.isEmpty() ? String("💗") : o->serviceIcon) + " " + esc(o->serviceTitle);
      else if (svc) b += esc(svc->icon.isEmpty() ? String("💗") : svc->icon) + " " + esc(svc->title);
      else b += "#" + o->serviceId;
      b += "</strong></p>\n";
      if (!o->serviceCategory.isEmpty()) b += "<p class=\"category\">" + esc(o->serviceCategory) + "</p>\n";

      User* fromU = findUser(o->fromUserId);
      User* toU = findUser(o->toUserId);
      b += "<p>De <strong>" + esc(fromU ? fromU->displayName : String("?")) + "</strong> à <strong>"
           + esc(toU ? toU->displayName : String("?")) + "</strong></p>\n";
      b += "<p>Prix : <strong>" + String(o->offeredPrice) + " ❤️</strong></p>\n";
      if (o->type == "purchase" && index == 0) b += "<p class=\"info\">⚠️ Les crédits ont déjà été débités de l'acheteur.</p>\n";
      if (!o->comment.isEmpty()) b += "<p class=\"comment\">« " + esc(o->comment) + " »</p>\n";
      if (o->isSuperService) {
        b += "<p class=\"super-service-badge\">🌟 Service bonus : +" + String(o->superServiceBonus)
             + " ❤️ crédits supplémentaires pour le prestataire !</p>\n";
      }
      b += "<p>Statut : <span class=\"badge status-" + esc(o->status) + "\">" + esc(statusText(o->status)) + "</span></p>\n";

      // isBuyer (même logique que la vue EJS)
      bool isBuyer = false;
      if (o->type == "purchase") isBuyer = (o->fromUserId == user.id);
      else if (o->type == "request") {
        if (!o->originalRequesterId.isEmpty()) isBuyer = (o->originalRequesterId == user.id);
        else isBuyer = (o->fromUserId == user.id);
      } else isBuyer = (o->fromUserId == user.id);

      // Actions du destinataire d'une offre en attente
      if (o->toUserId == user.id && o->status == "pending") {
        b += "<form method=\"post\" action=\"/offers/" + o->id + "/respond\" class=\"inline-form\">\n";
        b += "<button type=\"submit\" name=\"action\" value=\"accept\" class=\"btn-primary\">Accepter</button>\n";
        b += "<button type=\"submit\" name=\"action\" value=\"reject\" class=\"btn-secondary\">";
        b += (o->type == "purchase") ? "Refuser (remboursement)" : "Refuser";
        b += "</button>\n</form>\n";
        if (o->type != "purchase") {
          b += R"CNTR(<details class="counter-offer-form">
<summary class="btn-secondary">Faire une contre-proposition</summary>
<form method="post" action="/offers/)CNTR" + o->id + R"CNTR(/respond" class="form" style="margin-top: 12px;">
<label>Nouveau prix proposé (❤️)
<input type="number" name="counterPrice" min="0" step="1" value=")CNTR" + String(o->offeredPrice) + R"CNTR(" required />
</label>
<label>Commentaire (optionnel)
<input type="text" name="counterComment" placeholder="Votre message..." />
</label>
<button type="submit" name="action" value="counter" class="btn-primary">Envoyer la contre-offre</button>
</form>
</details>
)CNTR";
        }
      }

      // Déclarer réalisé (acheteur, offre acceptée)
      if (o->status == "accepted" && isBuyer) {
        b += R"REAL(<details class="feedback-form" style="margin-top: 10px;">
<summary class="btn-secondary">Déclarer réalisé</summary>
<form method="post" action="/offers/)REAL" + o->id + R"REAL(/realize" class="form" style="margin-top: 12px;">
<p style="margin: 0 0 10px 0; font-weight: bold;">Comment s'est passé ce service ?</p>
<div style="display: flex; gap: 15px; justify-content: center; margin: 15px 0;">
<label style="cursor: pointer; text-align: center;">
<input type="radio" name="feedback" value="happy" required style="display: none;" />
<div class="feedback-option" data-value="happy" style="font-size: 3rem; padding: 10px; border: 3px solid transparent; border-radius: 10px; transition: all 0.3s;">😊</div>
<div style="font-size: 0.9rem; margin-top: 5px;">Très satisfait</div>
</label>
<label style="cursor: pointer; text-align: center;">
<input type="radio" name="feedback" value="neutral" required style="display: none;" />
<div class="feedback-option" data-value="neutral" style="font-size: 3rem; padding: 10px; border: 3px solid transparent; border-radius: 10px; transition: all 0.3s;">😐</div>
<div style="font-size: 0.9rem; margin-top: 5px;">Correct</div>
</label>
<label style="cursor: pointer; text-align: center;">
<input type="radio" name="feedback" value="sad" required style="display: none;" />
<div class="feedback-option" data-value="sad" style="font-size: 3rem; padding: 10px; border: 3px solid transparent; border-radius: 10px; transition: all 0.3s;">😞</div>
<div style="font-size: 0.9rem; margin-top: 5px;">Décevant</div>
</label>
</div>
<button type="submit" class="btn-primary">Valider et marquer comme réalisé</button>
</form>
</details>
)REAL";
      }

      // Évaluation affichée après réalisation
      if (o->status == "realized" && !o->feedback.isEmpty()) {
        String fb;
        if (o->feedback == "happy") fb = "😊 Très satisfait";
        else if (o->feedback == "neutral") fb = "😐 Correct";
        else if (o->feedback == "sad") fb = "😞 Décevant";
        b += "<div style=\"margin-top: 10px; padding: 10px; background: #f5f5f5; border-radius: 4px;\">\n";
        b += "<p style=\"margin: 0; font-weight: bold;\">Évaluation :</p>\n";
        b += "<p style=\"margin: 5px 0 0 0; font-size: 2rem;\">" + fb + "</p>\n</div>\n";
      }
      b += "</div>\n";
      index++;
    }
    b += "</div>\n";
  }

  b += R"NEGJS(</div>
</section>
</main>
<script>
document.addEventListener('DOMContentLoaded', function() {
  document.querySelectorAll('.feedback-option').forEach(function(option) {
    option.addEventListener('click', function() {
      var label = this.closest('label');
      var radio = label.querySelector('input[type="radio"]');
      var form = label.closest('form');
      form.querySelectorAll('.feedback-option').forEach(function(opt) {
        opt.style.borderColor = 'transparent';
        opt.style.transform = 'scale(1)';
      });
      this.style.borderColor = '#667eea';
      this.style.transform = 'scale(1.1)';
      radio.checked = true;
    });
  });
});
</script>
</body>
</html>)NEGJS";

  // Shell : head + topbar/nav (nav sans lien événements, comme negotiations.ejs)
  String html;
  html.reserve(b.length() + 2048);
  html += "<!DOCTYPE html>\n<html lang=\"fr\">\n<head>\n<meta charset=\"UTF-8\"/>\n<title>Négociations - Love Market</title>\n";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n";
  html += "<link rel=\"icon\" type=\"image/svg+xml\" href=\"/favicon.svg\"/>\n<link rel=\"stylesheet\" href=\"/style.css\"/>\n</head>\n<body>\n";
  html += "<header class=\"topbar\">\n<div class=\"logo\">💕 Love Market</div>\n<nav class=\"nav\">\n";
  html += "<a href=\"/dashboard\" title=\"Accueil\">🏠<span class=\"nav-text\"> Accueil</span></a>\n";
  html += "<a href=\"/shop\" title=\"Boutique\">🛍️<span class=\"nav-text\"> Boutique</span></a>\n";
  html += "<a href=\"/my-services\" title=\"Mes services\">🤝<span class=\"nav-text\"> Mes services</span></a>\n";
  html += "<a href=\"/negotiations\" title=\"Négociations\" class=\"nav-link-with-badge nav-active\">💬<span class=\"nav-text\"> Négociations</span>";
  if (notif > 0) html += "<span class=\"notification-badge\">" + String(notif) + "</span>";
  html += "</a>\n";
  if (user.role == "admin") html += "<a href=\"/admin\" class=\"admin-link\" title=\"Admin\">⚙️<span class=\"nav-text\"> Admin</span></a>\n";
  html += "</nav>\n<div class=\"user-info\">\n<span class=\"credits\">" + String(user.credits) + " ❤️</span>\n";
  html += "<span class=\"user-name\">" + esc(user.displayName) + "</span>\n";
  html += "<a href=\"/logout\" class=\"logout\">Déconnexion</a>\n</div>\n</header>\n<main class=\"page\">\n";
  html += b;
  return html;
}

// ---------------------------------------------------------------------------
// GET/POST /request-service — views/request-service.ejs
// ---------------------------------------------------------------------------
String pageRequestService(User& user) {
  User* partner = findPartnerOf(user.id);
  int notif = countNotifications(user.id);

  String b;
  b.reserve(6000);
  b += "<section class=\"section\">\n<h1 class=\"section-title\">Demander un service personnalisé ✨</h1>\n";
  b += "<div class=\"card\">\n<h2 class=\"card-title\">Nouvelle demande à " + esc(partner ? partner->displayName : String("?")) + "</h2>\n";
  b += "<p class=\"hint\">Demande un service qui n'est pas en vente. Ton/ta partenaire pourra accepter, refuser ou faire une contre-proposition.</p>\n";
  b += "<form method=\"post\" action=\"/request-service\" class=\"form\">\n";
  b += "<label>Titre du service souhaité<input type=\"text\" name=\"title\" placeholder=\"Ex: Soirée cinéma à la maison\" required /></label>\n";
  b += "<label>Description / commentaire<textarea name=\"description\" rows=\"3\" placeholder=\"Décris ce que tu aimerais...\"></textarea></label>\n";
  b += "<label>Catégorie<select name=\"category\">" + categoryOptions("Romantique") + "</select></label>\n";
  b += "<label>Icône<select name=\"icon\">" + iconOptions("💗") + "</select></label>\n";
  b += "<label>Prix proposé (en crédits ❤️)<input type=\"number\" name=\"offeredPrice\" min=\"0\" step=\"1\" value=\"30\" onchange=\"updateSuperServicePrice(this)\" required /></label>\n";
  b += R"SSVC(<label class="checkbox-label">
<input type="checkbox" name="isSuperService" value="true" onchange="updateSuperServicePrice(document.querySelector('input[name=offeredPrice]'))" />
<span class="checkbox-custom"></span>
Service bonus (+30% de crédits pour ton partenaire)
</label>
<div id="superServiceInfo" style="margin: 10px 0; padding: 10px; background: #f0f8ff; border-radius: 4px; display: none;">
<p style="margin: 0; color: #2c5aa0; font-weight: bold;" id="superServiceText"></p>
</div>
<p class="hint">Ton solde actuel : <strong>)SSVC" + String(user.credits) + R"SSVC( ❤️</strong><br>
Les crédits ne seront débités que si ton/ta partenaire accepte.
</p>
<button type="submit" class="btn-primary">Envoyer la demande</button>
<a href="/dashboard" class="btn-secondary">Annuler</a>
</form>
<script>
function updateSuperServicePrice(priceInput) {
  var checkbox = document.querySelector('input[name=isSuperService]');
  var infoDiv = document.getElementById('superServiceInfo');
  var superServiceText = document.getElementById('superServiceText');
  if (checkbox.checked) {
    var basePrice = parseInt(priceInput.value) || 0;
    var bonus = Math.round(basePrice * 0.3);
    var total = basePrice + bonus;
    superServiceText.textContent = 'Service bonus : Ton partenaire recevra ' + basePrice + ' ❤️ (prix) + ' + bonus + ' ❤️ (bonus) = ' + total + ' ❤️ au total !';
    infoDiv.style.display = 'block';
  } else {
    infoDiv.style.display = 'none';
  }
}
</script>
</div>
</section>)SSVC";

  return renderPage("Demander un service", "/request-service", false, b, user.displayName,
                    user.credits, user.role == "admin", notif);
}

// ---------------------------------------------------------------------------
// GET/POST /offer-service — views/offer-service.ejs
// ---------------------------------------------------------------------------
String pageOfferService(User& user) {
  User* partner = findPartnerOf(user.id);
  int notif = countNotifications(user.id);

  String b;
  b.reserve(6000);
  b += "<section class=\"section\">\n<h1 class=\"section-title\">Proposer un service ✨</h1>\n";
  b += "<div class=\"card\">\n<h2 class=\"card-title\">Nouvelle proposition pour " + esc(partner ? partner->displayName : String("?")) + "</h2>\n";
  b += "<p class=\"hint\">Propose un service que tu peux offrir à ton/ta partenaire, contre des crédits. S'il/elle accepte, ses crédits seront débités et tu seras crédité(e).</p>\n";
  b += "<form method=\"post\" action=\"/offer-service\" class=\"form\">\n";
  b += "<label>Titre du service proposé<input type=\"text\" name=\"title\" placeholder=\"Ex: Massage relaxant 30 min\" required /></label>\n";
  b += "<label>Description / détails<textarea name=\"description\" rows=\"3\" placeholder=\"Décris ce que tu proposes...\"></textarea></label>\n";
  b += "<label>Catégorie<select name=\"category\">" + categoryOptions("Romantique") + "</select></label>\n";
  b += "<label>Icône<select name=\"icon\">" + iconOptions("💗") + "</select></label>\n";
  b += "<label>Prix demandé (en crédits ❤️)<input type=\"number\" name=\"offeredPrice\" min=\"0\" step=\"1\" value=\"30\" onchange=\"updateSuperServicePrice(this)\" required /></label>\n";
  b += R"OSVC(<label class="checkbox-label">
<input type="checkbox" name="isSuperService" value="true" onchange="updateSuperServicePrice(document.querySelector('input[name=offeredPrice]'))" />
<span class="checkbox-custom"></span>
Service bonus (+30% de crédits pour toi)
</label>
<div id="superServiceInfo" style="margin: 10px 0; padding: 10px; background: #f0f8ff; border-radius: 4px; display: none;">
<p style="margin: 0; color: #2c5aa0; font-weight: bold;" id="superServiceText"></p>
</div>
<p class="hint">Ton/ta partenaire paiera seulement s'il/elle accepte la proposition.</p>
<button type="submit" class="btn-primary">Envoyer la proposition</button>
<a href="/dashboard" class="btn-secondary">Annuler</a>
</form>
<script>
function updateSuperServicePrice(priceInput) {
  var checkbox = document.querySelector('input[name=isSuperService]');
  var infoDiv = document.getElementById('superServiceInfo');
  var superServiceText = document.getElementById('superServiceText');
  if (checkbox.checked) {
    var basePrice = parseInt(priceInput.value) || 0;
    var bonus = Math.round(basePrice * 0.3);
    var total = basePrice + bonus;
    superServiceText.textContent = 'Service bonus : Si accepté, tu recevras ' + basePrice + ' ❤️ (prix) + ' + bonus + ' ❤️ (bonus) = ' + total + ' ❤️ au total !';
    infoDiv.style.display = 'block';
  } else {
    infoDiv.style.display = 'none';
  }
}
</script>
</div>
</section>)OSVC";

  return renderPage("Proposer un service", "/offer-service", false, b, user.displayName,
                    user.credits, user.role == "admin", notif);
}

// ---------------------------------------------------------------------------
// GET /relationship-history — views/relationship-history.ejs
// ---------------------------------------------------------------------------
String pageRelationshipHistory(User& user) {
  int notif = countNotifications(user.id);
  RelData rel;
  getRelationshipData(rel);

  // Transactions acceptées/réalisées des 30 derniers jours, triées par date décroissante
  time_t limit = nowEpoch() - 30 * 24 * 3600;
  std::vector<Offer*> history;
  for (auto& o : g_offers) {
    if (!o.hasCreatedAt) continue;
    if (o.createdAt < limit) continue;
    if (o.status != "accepted" && o.status != "realized") continue;
    history.push_back(&o);
  }
  std::sort(history.begin(), history.end(), [](Offer* a, Offer* b) {
    return a->createdAt > b->createdAt;
  });

  String b;
  b.reserve(8000);

  // Carte niveau de couple
  b += "<section class=\"grid\">\n";
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
  b += "</div>\n</section>\n";

  // Historique des transactions
  b += "<section class=\"section\">\n<div class=\"card\">\n";
  b += "<h2 class=\"card-title\">📜 Historique des transactions</h2>\n";
  if (history.empty()) {
    b += "<p>Aucune transaction récente dans les 30 derniers jours.</p>\n";
  } else {
    b += "<div class=\"transaction-list\">\n";
    for (auto* o : history) {
      int totalCredits = o->offeredPrice + (o->isSuperService ? o->superServiceBonus : 0);
      Service* svc = o->serviceId.isEmpty() ? nullptr : findService(o->serviceId);
      User* fromU = findUser(o->fromUserId);
      User* toU = findUser(o->toUserId);

      b += "<div class=\"transaction-item\">\n";
      b += "<div class=\"transaction-header\">\n";
      b += "<div class=\"transaction-date\">" + formatDateTimeFR(o->createdAt) + "</div>\n";
      b += "<div class=\"transaction-credits\">" + String(totalCredits) + " ❤️</div>\n";
      b += "</div>\n";
      b += "<div class=\"transaction-details\">\n";
      if (svc) {
        b += "<div class=\"transaction-service\"><span class=\"service-icon\">" + esc(svc->icon.isEmpty() ? String("💝") : svc->icon)
             + "</span><span class=\"service-title\">" + esc(svc->title) + "</span></div>\n";
      } else if (!o->serviceTitle.isEmpty()) {
        b += "<div class=\"transaction-service\"><span class=\"service-icon\">" + esc(o->serviceIcon.isEmpty() ? String("💝") : o->serviceIcon)
             + "</span><span class=\"service-title\">" + esc(o->serviceTitle) + "</span></div>\n";
      }
      b += "<div class=\"transaction-users\"><span class=\"from-user\">" + esc(fromU ? fromU->displayName : String("?"))
           + "</span><span class=\"transaction-arrow\">→</span><span class=\"to-user\">"
           + esc(toU ? toU->displayName : String("?")) + "</span></div>\n";
      String typeBadge;
      String typeClass;
      if (o->type == "purchase") { typeBadge = "Achat"; typeClass = "type-purchase"; }
      else if (o->type == "request") { typeBadge = "Demande"; typeClass = "type-request"; }
      else if (o->type == "donation") { typeBadge = "Don"; typeClass = "type-donation"; }
      else if (o->type == "special_event") { typeBadge = "Événement"; typeClass = "type-special"; }
      else { typeBadge = "Négociation"; typeClass = "type-negotiation"; }
      b += "<div class=\"transaction-type\"><span class=\"type-badge " + typeClass + "\">" + typeBadge + "</span>";
      if (o->status == "realized") b += "<span class=\"status-badge status-realized\">✅ Réalisé</span>";
      else if (o->status == "accepted") b += "<span class=\"status-badge status-accepted\">✅ Accepté</span>";
      b += "</div>\n";
      if (!o->comment.isEmpty()) b += "<div class=\"transaction-comment\">\"" + esc(o->comment) + "\"</div>\n";
      b += "</div>\n</div>\n";
    }
    b += "</div>\n";
  }
  b += "</div>\n</section>\n";

  // Styles spécifiques à cette page (comme le <style> de relationship-history.ejs)
  b += R"RELSTYLE(<style>
.transaction-list { display: flex; flex-direction: column; gap: 15px; }
.transaction-item { border: 1px solid #e0e0e0; border-radius: 8px; padding: 15px; background: #fafafa; }
.transaction-header {
  display: flex; justify-content: space-between; align-items: center;
  margin-bottom: 10px; padding-bottom: 8px; border-bottom: 1px solid #eee;
}
.transaction-date { font-size: 0.9rem; color: #666; }
.transaction-credits { font-weight: bold; color: #e91e63; }
.transaction-details { display: flex; flex-direction: column; gap: 8px; }
.transaction-service { display: flex; align-items: center; gap: 8px; }
.transaction-users { display: flex; align-items: center; gap: 5px; font-size: 0.9rem; }
.transaction-type { display: flex; gap: 8px; flex-wrap: wrap; }
.type-badge, .status-badge { padding: 3px 8px; border-radius: 12px; font-size: 0.8rem; font-weight: bold; }
.type-purchase { background: #e3f2fd; color: #1976d2; }
.type-request { background: #f3e5f5; color: #7b1fa2; }
.type-donation { background: #e8f5e9; color: #388e3c; }
.type-special { background: #fff3e0; color: #f57c00; }
.type-negotiation { background: #fff8e1; color: #f57c00; }
.status-realized { background: #e8f5e9; color: #388e3c; }
.status-accepted { background: #e3f2fd; color: #1976d2; }
.transaction-comment {
  font-style: italic; color: #666; padding: 8px;
  background: #f5f5f5; border-radius: 4px; margin-top: 5px;
}
</style>
</body>
</html>)RELSTYLE";

  // Shell : nav avec lien Événements (comme la vue originale)
  String html;
  html.reserve(b.length() + 2048);
  html += "<!DOCTYPE html>\n<html lang=\"fr\">\n<head>\n<meta charset=\"UTF-8\"/>\n<title>Historique de couple - Love Market</title>\n";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n";
  html += "<link rel=\"icon\" type=\"image/svg+xml\" href=\"/favicon.svg\"/>\n<link rel=\"stylesheet\" href=\"/style.css\"/>\n</head>\n<body>\n";
  html += "<header class=\"topbar\">\n<div class=\"logo\">💕 Love Market</div>\n<nav class=\"nav\">\n";
  html += "<a href=\"/dashboard\" title=\"Accueil\">🏠<span class=\"nav-text\"> Accueil</span></a>\n";
  html += "<a href=\"/shop\" title=\"Boutique\">🛍️<span class=\"nav-text\"> Boutique</span></a>\n";
  html += "<a href=\"/my-services\" title=\"Mes services\">🤝<span class=\"nav-text\"> Mes services</span></a>\n";
  html += "<a href=\"/special-events\" title=\"Événements spéciaux\" class=\"nav-active\">🎯<span class=\"nav-text\"> Événements</span></a>\n";
  html += "<a href=\"/negotiations\" title=\"Négociations\" class=\"nav-link-with-badge\">💬<span class=\"nav-text\"> Négociations</span>";
  if (notif > 0) html += "<span class=\"notification-badge\">" + String(notif) + "</span>";
  html += "</a>\n";
  if (user.role == "admin") html += "<a href=\"/admin\" class=\"admin-link\" title=\"Admin\">⚙️<span class=\"nav-text\"> Admin</span></a>\n";
  html += "</nav>\n<div class=\"user-info\">\n<span class=\"credits\">" + String(user.credits) + " ❤️</span>\n";
  html += "<span class=\"user-name\">" + esc(user.displayName) + "</span>\n";
  html += "<a href=\"/logout\" class=\"logout\">Déconnexion</a>\n</div>\n</header>\n<main class=\"page\">\n";
  html += b;
  return html;
}
