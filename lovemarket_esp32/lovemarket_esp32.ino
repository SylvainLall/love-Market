// ===========================================================================
// Love Market pour ESP32-S3 (N16R8) — portage de la version Node.js/Express
//
// - Serveur HTTP embarqué (WebServer.h, bibliothèque standard ESP32)
// - Point d'accès WiFi autonome "PPmark" (aucune connexion internet requise)
// - Données persistées dans LittleFS (/data.json, format identique à la version Node)
// - Interfaces HTML identiques aux vues EJS (style.css embarqué dans le firmware)
//
// Réglages IDE Arduino (carte "ESP32S3 Dev Module") :
//   Flash Size      : 16MB
//   Partition Scheme: Custom (fichier partitions.csv du sketch)
//   PSRAM           : OPI PSRAM                          [N16R8 = PSRAM octal]
//   CPU Frequency   : 240MHz
// Bibliothèque requise (Library Manager) : "ArduinoJson" de Benoit Blanchon (v7)
// ===========================================================================

#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "model.h"
#include "ui.h"
#include "pages.h"

// --- Configuration du point d'accès WiFi (personnalisable) ---
static const char* AP_SSID = "PPmark";
static const char* AP_PASS = "Poupoune"; // 8 à 63 caractères

// --- LED intégrée (WS2812 sur GPIO48) : notifications ---
// Respiration en bleu si notification pour Sylvain, en rose si pour Alice.
static const int LED_PIN = 48;

WebServer server(80);

// ---------------------------------------------------------------------------
// Session courante (par requête) + messages flash
// ---------------------------------------------------------------------------
static String getCookieToken() {
  String cookie = server.header("Cookie");
  if (cookie.isEmpty()) return "";
  int pos = cookie.indexOf("lm_sid=");
  if (pos < 0) return "";
  int start = pos + 7;
  int end = cookie.indexOf(';', start);
  if (end < 0) end = cookie.length();
  String token = cookie.substring(start, end);
  token.trim();
  return token;
}

static User* currentUser() {
  Session* s = sessionFind(getCookieToken());
  if (!s) return nullptr;
  return findUser(s->userId);
}

static void redirect(const char* url) {
  server.sendHeader("Location", url, true);
  server.send(302, "text/plain", "");
}

static void takeFlash(String& err, String& ok) {
  Session* s = sessionFind(getCookieToken());
  if (s) { err = s->flashError; ok = s->flashSuccess; s->flashError = ""; s->flashSuccess = ""; }
  else   { err = ""; ok = ""; }
}

// ---------------------------------------------------------------------------
// Enregistrement des routes (équivalent des routes Express de index.js)
// ---------------------------------------------------------------------------
void setupRoutes() {
  // --- Statique ---
  server.on("/style.css", HTTP_GET, []() {
    server.send_P(200, "text/css", LM_STYLE_CSS);
  });
  server.on("/favicon.svg", HTTP_GET, []() {
    server.send_P(200, "image/svg+xml", LM_FAVICON_SVG);
  });
  server.on("/favicon.ico", HTTP_GET, []() {
    server.send_P(200, "image/svg+xml", LM_FAVICON_SVG);
  });

  // --- Racine ---
  server.on("/", HTTP_GET, []() {
    if (!currentUser()) redirect("/login");
    else redirect("/dashboard");
  });

  // --- Connexion / déconnexion ---
  server.on("/login", HTTP_GET, []() {
    if (currentUser()) return redirect("/dashboard");
    server.send(200, "text/html", pageLogin(""));
  });
  server.on("/login", HTTP_POST, []() {
    String userId = server.arg("userId");
    String password = server.arg("password");
    User* u = nullptr;
    for (auto& x : g_users) if (x.id == userId && x.password == password) { u = &x; break; }
    if (!u) { server.send(200, "text/html", pageLogin("Identifiants incorrects")); return; }
    Session* s = sessionCreate(u->id);
    server.sendHeader("Set-Cookie", String("lm_sid=") + s->token + "; Path=/; HttpOnly; SameSite=Lax");
    redirect("/dashboard");
  });
  server.on("/logout", HTTP_GET, []() {
    sessionDestroy(getCookieToken());
    server.sendHeader("Set-Cookie", String("lm_sid=; Path=/; HttpOnly; Max-Age=0"));
    redirect("/login");
  });

  // --- Dashboard (consomme les messages flash de la session) ---
  server.on("/dashboard", HTTP_GET, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    String err, ok;
    takeFlash(err, ok);
    server.send(200, "text/html", pageDashboard(*user, err, ok));
  });

  // --- Boutique ---
  server.on("/shop", HTTP_GET, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    server.send(200, "text/html", pageShop(*user));
  });

  // --- Mes services ---
  server.on("/my-services", HTTP_GET, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    server.send(200, "text/html", pageMyServices(*user));
  });

  // --- Créer un service ---
  server.on("/services", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    String title = server.arg("title");
    int price = server.arg("price").toInt();
    if (title.isEmpty() || price == 0) return redirect("/my-services"); // !title || !price (comportement JS)
    Service s;
    s.id = String(g_nextServiceId++);
    s.sellerId = user->id;
    s.title = title;
    s.description = server.arg("description");
    s.category = server.arg("category").isEmpty() ? String("Romantique") : server.arg("category");
    s.price = price;
    s.icon = server.arg("icon").isEmpty() ? String("💗") : server.arg("icon");
    g_services.push_back(s);
    saveStore();
    redirect("/my-services");
  });

  // --- Édition d'un service ---
  server.on("/services/{}/edit", HTTP_GET, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    Service* s = findService(server.pathArg(0));
    if (!s || s->sellerId != user->id) return redirect("/my-services");
    server.send(200, "text/html", pageEditService(*user, *s));
  });
  server.on("/services/{}/edit", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    Service* s = findService(server.pathArg(0));
    if (!s || s->sellerId != user->id) return redirect("/my-services");
    s->title = server.arg("title").isEmpty() ? s->title : server.arg("title");
    s->description = server.arg("description");
    s->category = server.arg("category").isEmpty() ? s->category : server.arg("category");
    s->price = server.arg("price").toInt();
    s->icon = server.arg("icon").isEmpty() ? (s->icon.isEmpty() ? String("💗") : s->icon) : server.arg("icon");
    saveStore();
    redirect("/my-services");
  });

  // --- Suppression d'un service ---
  server.on("/services/{}/delete", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    for (size_t i = 0; i < g_services.size(); i++) {
      if (g_services[i].id == server.pathArg(0) && g_services[i].sellerId == user->id) {
        g_services.erase(g_services.begin() + i);
        break;
      }
    }
    saveStore();
    redirect("/my-services");
  });

  // --- Achat immédiat (débit de l'acheteur, demande au vendeur) ---
  server.on("/services/{}/buy", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    Service* s = findService(server.pathArg(0));
    if (!s) return redirect("/shop");
    User* partner = findUser(s->sellerId);
    if (!partner) return redirect("/shop");
    if (user->credits < s->price) return redirect("/shop");
    user->credits -= s->price;
    Offer* o = createOffer(user->id, partner->id, s->price, "purchase", "");
    o->serviceId = s->id;
    o->debitedAmount = s->price;
    saveStore();
    redirect("/dashboard");
  });

  // --- Négociation sur un service ---
  server.on("/services/{}/offer", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    Service* s = findService(server.pathArg(0));
    if (!s) return redirect("/shop");
    Offer* o = createOffer(user->id, s->sellerId, server.arg("offeredPrice").toInt(),
                           "negotiation", server.arg("comment"));
    o->serviceId = s->id;
    saveStore();
    redirect("/dashboard");
  });

  // --- Négociations (marque les offres comme lues, sans sauvegarde — comme l'original) ---
  server.on("/negotiations", HTTP_GET, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    for (auto& o : g_offers) {
      if (o.fromUserId != user->id && o.toUserId != user->id) continue;
      bool already = false;
      for (auto& r : o.readBy) if (r == user->id) { already = true; break; }
      if (!already) o.readBy.push_back(user->id);
    }
    server.send(200, "text/html", pageNegotiations(*user));
  });

  // --- Répondre à une offre : accept / counter / reject ---
  server.on("/offers/{}/respond", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    String action = server.arg("action");
    String offerId = server.pathArg(0);
    Offer* offer = findOffer(offerId);
    if (!offer || offer->toUserId != user->id || offer->status != "pending") return redirect("/negotiations");

    if (action == "accept") {
      Service* service = offer->serviceId.isEmpty() ? nullptr : findService(offer->serviceId);
      User* buyer = findUser(offer->fromUserId);
      User* seller = findUser(offer->toUserId);
      if (offer->type == "purchase") {
        // Achat : crédits déjà débités, on crédite le vendeur
        if (service && buyer && seller) {
          seller->credits += offer->offeredPrice;
          offer->status = "accepted";
          offer->readBy.clear();
          offer->readBy.push_back(user->id);
        }
      } else if (offer->type == "request") {
        // Demande personnalisée : on débite le demandeur initial
        User* originalRequester = findUser(offer->originalRequesterId.isEmpty() ? offer->fromUserId : offer->originalRequesterId);
        if (originalRequester) {
          User* serviceProvider = nullptr;
          for (auto& u : g_users) if (u.id != originalRequester->id) { serviceProvider = &u; break; }
          if (serviceProvider && originalRequester->credits >= offer->offeredPrice) {
            originalRequester->credits -= offer->offeredPrice;
            serviceProvider->credits += offer->offeredPrice;
            if (offer->isSuperService && offer->superServiceBonus > 0) {
              serviceProvider->credits += offer->superServiceBonus;
            }
            offer->status = "accepted";
            offer->readBy.clear();
            offer->readBy.push_back(user->id);
          }
        }
      } else {
        // Négociation classique
        if (service && buyer && seller && buyer->credits >= offer->offeredPrice) {
          buyer->credits -= offer->offeredPrice;
          seller->credits += offer->offeredPrice;
          if (offer->isSuperService && offer->superServiceBonus > 0) {
            seller->credits += offer->superServiceBonus;
          }
          offer->status = "accepted";
          offer->readBy.clear();
          offer->readBy.push_back(user->id);
        }
      }
    } else if (action == "counter") {
      User* buyer = findUser(offer->fromUserId);
      User* seller = findUser(offer->toUserId);
      String counterPrice = server.arg("counterPrice");
      String counterComment = server.arg("counterComment");
      if (!counterPrice.isEmpty() && seller && buyer) {
        String rootOfferId = offer->rootOfferId.isEmpty() ? offer->id : offer->rootOfferId;
        Offer* n = createOffer(seller->id, buyer->id, counterPrice.toInt(),
                               (offer->type == "request") ? "request" : "negotiation",
                               counterComment.isEmpty() ? String("Contre-proposition") : counterComment);
        n->serviceId = offer->serviceId;
        n->serviceTitle = offer->serviceTitle;
        n->serviceCategory = offer->serviceCategory;
        n->serviceIcon = offer->serviceIcon;
        n->parentOfferId = offer->id;
        n->rootOfferId = rootOfferId;
        n->originalRequesterId = offer->originalRequesterId.isEmpty() ? offer->fromUserId : offer->originalRequesterId;
        // createOffer peut avoir réalloué le vecteur : on ré-acquiert le pointeur
        offer = findOffer(offerId);
        offer->status = "countered";
        offer->readBy.clear();
        offer->readBy.push_back(user->id);
      }
    } else {
      // Refus : remboursement si achat
      if (offer->type == "purchase" && offer->debitedAmount >= 0) {
        User* buyer = findUser(offer->fromUserId);
        if (buyer) buyer->credits += offer->debitedAmount;
      }
      offer->status = "rejected";
      offer->readBy.clear();
      offer->readBy.push_back(user->id);
    }
    saveStore();
    redirect("/negotiations");
  });

  // --- Déclarer réalisé (par l'acheteur) ---
  server.on("/offers/{}/realize", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    Offer* offer = findOffer(server.pathArg(0));
    if (!offer || offer->status != "accepted") return redirect("/negotiations");
    String buyerId;
    if (offer->type == "purchase") buyerId = offer->fromUserId;
    else if (offer->type == "request") buyerId = offer->originalRequesterId.isEmpty() ? offer->fromUserId : offer->originalRequesterId;
    else buyerId = offer->fromUserId;
    if (user->id != buyerId) return redirect("/negotiations");
    offer->status = "realized";
    offer->readBy.clear();
    offer->readBy.push_back(user->id);
    String feedback = server.arg("feedback");
    if (!feedback.isEmpty()) {
      offer->feedback = feedback;
      offer->hasFeedbackAt = true;
      offer->feedbackAt = nowEpoch();
    }
    saveStore();
    redirect("/negotiations");
  });

  // --- Demander un service personnalisé ---
  server.on("/request-service", HTTP_GET, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    server.send(200, "text/html", pageRequestService(*user));
  });
  server.on("/request-service", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    User* partner = findPartnerOf(user->id);
    if (!partner) return redirect("/dashboard");
    String title = server.arg("title");
    int offeredPrice = server.arg("offeredPrice").toInt();
    if (title.isEmpty() || offeredPrice == 0) return redirect("/request-service");
    bool isSuper = (server.arg("isSuperService") == "true");
    Offer* o = createOffer(user->id, partner->id, offeredPrice, "request", server.arg("description"));
    o->serviceTitle = title;
    o->serviceCategory = server.arg("category").isEmpty() ? String("Romantique") : server.arg("category");
    o->serviceIcon = server.arg("icon").isEmpty() ? String("💗") : server.arg("icon");
    o->originalRequesterId = user->id;
    o->isSuperService = isSuper;
    o->superServiceBonus = isSuper ? superServiceBonus(offeredPrice) : 0;
    saveStore();
    redirect("/negotiations");
  });

  // --- Proposer un service personnalisé ---
  server.on("/offer-service", HTTP_GET, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    server.send(200, "text/html", pageOfferService(*user));
  });
  server.on("/offer-service", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    User* partner = findPartnerOf(user->id);
    if (!partner) return redirect("/dashboard");
    String title = server.arg("title");
    int offeredPrice = server.arg("offeredPrice").toInt();
    if (title.isEmpty() || offeredPrice == 0) return redirect("/offer-service");
    bool isSuper = (server.arg("isSuperService") == "true");
    // Le partenaire paiera s'il accepte : type request, payeur = partenaire
    Offer* o = createOffer(user->id, partner->id, offeredPrice, "request", server.arg("description"));
    o->serviceTitle = title;
    o->serviceCategory = server.arg("category").isEmpty() ? String("Romantique") : server.arg("category");
    o->serviceIcon = server.arg("icon").isEmpty() ? String("💗") : server.arg("icon");
    o->originalRequesterId = partner->id;
    o->isSuperService = isSuper;
    o->superServiceBonus = isSuper ? superServiceBonus(offeredPrice) : 0;
    saveStore();
    redirect("/negotiations");
  });

  // --- Don de crédits ---
  server.on("/donate-credits", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    User* partner = findPartnerOf(user->id);
    if (!partner) return redirect("/dashboard");
    int amountNum = server.arg("amount").toInt();
    if (amountNum <= 0 || amountNum > user->credits) {
      Session* s = sessionFind(getCookieToken());
      if (s) s->flashError = "Montant invalide ou insuffisant";
      return redirect("/dashboard");
    }
    user->credits -= amountNum;
    partner->credits += amountNum;
    Offer* d = createOffer(user->id, partner->id, amountNum, "donation",
                           server.arg("message").isEmpty() ? String("Don de crédits") : server.arg("message"));
    d->status = "accepted";
    d->serviceTitle = "Don de crédits";
    d->serviceCategory = "Cadeau";
    d->serviceIcon = "💝";
    d->originalRequesterId = user->id;
    saveStore();
    Session* s = sessionFind(getCookieToken());
    if (s) s->flashSuccess = "Don de " + String(amountNum) + " ❤️ crédits effectué à " + partner->displayName + " !";
    redirect("/dashboard");
  });

  // --- Utiliser un bon cadeau ---
  server.on("/redeem-voucher", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    Voucher* v = findVoucherByCode(server.arg("code"));
    if (!v || v->used) {
      Session* s = sessionFind(getCookieToken());
      if (s) s->flashError = "Code invalide ou déjà utilisé";
      return redirect("/dashboard");
    }
    user->credits += v->credits;
    v->used = true;
    v->usedBy = user->id;
    v->hasUsedAt = true;
    v->usedAt = nowEpoch();
    saveStore();
    Session* s = sessionFind(getCookieToken());
    if (s) s->flashSuccess = "Félicitations ! Vous avez reçu " + String(v->credits) + " ❤️ crédits !";
    redirect("/dashboard");
  });

  // --- Historique de couple ---
  server.on("/relationship-history", HTTP_GET, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    server.send(200, "text/html", pageRelationshipHistory(*user));
  });

  // --- Événements spéciaux ---
  server.on("/special-events", HTTP_GET, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    String err, ok;
    takeFlash(err, ok);
    server.send(200, "text/html", pageSpecialEvents(*user, err, ok));
  });
  server.on("/special-events/{}/accept", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    SpecialEvent* event = findEvent(server.pathArg(0));
    if (!event || event->status != "active") return redirect("/special-events");
    if (event->targetUser != "both" && event->targetUser != user->id) return redirect("/special-events");
    Session* s = sessionFind(getCookieToken());
    if (!event->assignedTo.isEmpty()) {
      if (s) s->flashError = "Cet événement est déjà attribué à quelqu'un d'autre.";
      return redirect("/special-events");
    }
    event->assignedTo = user->id;
    Offer* o = createOffer(user->id, user->id, event->credits, "special_event",
                           "Événement spécial: " + event->title);
    o->status = "accepted";
    o->serviceTitle = event->title;
    o->serviceCategory = "Événement spécial";
    o->serviceIcon = "🎯";
    o->originalRequesterId = user->id;
    o->eventId = event->id;
    saveStore();
    if (s) s->flashSuccess = "Vous avez accepté l'événement: " + event->title;
    redirect("/special-events");
  });
  server.on("/special-events/{}/complete", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    String eventId = server.pathArg(0);
    SpecialEvent* event = findEvent(eventId);
    Offer* offer = nullptr;
    for (auto& o : g_offers) if (o.eventId == eventId && o.fromUserId == user->id) { offer = &o; break; }
    if (!event || event->status != "active" || !offer || event->assignedTo != user->id) return redirect("/special-events");
    user->credits += event->credits;
    event->completedBy = user->id;
    event->hasCompletedAt = true;
    event->completedAt = nowEpoch();
    event->status = "completed";
    offer->status = "realized";
    saveStore();
    Session* s = sessionFind(getCookieToken());
    if (s) s->flashSuccess = "Félicitations ! Vous avez reçu " + String(event->credits) + " ❤️ crédits pour avoir complété l'événement: " + event->title;
    redirect("/special-events");
  });

  // --- Administration ---
  server.on("/admin", HTTP_GET, []() {
    User* user = currentUser();
    if (!user || user->role != "admin") {
      server.send(403, "text/plain", "Forbidden");
      return;
    }
    String importStatus = server.hasArg("import") ? server.arg("import") : String("");
    server.send(200, "text/html", pageAdmin(*user, importStatus));
  });
  server.on("/admin/credits", HTTP_POST, []() {
    User* user = currentUser();
    if (!user || user->role != "admin") { server.send(403, "text/plain", "Forbidden"); return; }
    User* target = findUser(server.arg("targetId"));
    if (target) {
      target->credits = server.arg("credits").toInt();
      saveStore();
    }
    redirect("/admin");
  });
  server.on("/admin/relationship-levels", HTTP_POST, []() {
    User* user = currentUser();
    if (!user || user->role != "admin") { server.send(403, "text/plain", "Forbidden"); return; }
    int index = server.arg("levelIndex").toInt();
    int credits = server.arg("minCredits").toInt();
    if (index >= 0 && index < (int)g_levels.size() && credits >= 0) {
      g_levels[index].minCredits = credits;
      saveStore();
    }
    redirect("/admin");
  });
  server.on("/admin/special-event", HTTP_POST, []() {
    User* user = currentUser();
    if (!user || user->role != "admin") { server.send(403, "text/plain", "Forbidden"); return; }
    String title = server.arg("title");
    String description = server.arg("description");
    int creditsNum = server.arg("credits").toInt();
    if (title.isEmpty() || description.isEmpty() || creditsNum <= 0) return redirect("/admin");
    SpecialEvent e;
    e.id = String(g_nextEventId++);
    e.title = title;
    e.description = description;
    e.credits = creditsNum;
    e.targetUser = server.arg("targetUser").isEmpty() ? String("both") : server.arg("targetUser");
    e.status = "active";
    e.hasCreatedAt = true;
    e.createdAt = nowEpoch();
    g_events.push_back(e);
    saveStore();
    redirect("/admin");
  });
  server.on("/admin/gift-voucher", HTTP_POST, []() {
    User* user = currentUser();
    if (!user || user->role != "admin") { server.send(403, "text/plain", "Forbidden"); return; }
    int creditsNum = server.arg("credits").toInt();
    if (creditsNum <= 0) return redirect("/admin");
    // Code à 4 chiffres unique (spécification bons cadeaux)
    String code;
    do {
      code = String(1000 + (int)random(9000));
    } while (findVoucherByCode(code) != nullptr);
    Voucher v;
    v.id = String(g_nextVoucherId++);
    v.code = code;
    v.credits = creditsNum;
    v.comment = server.arg("comment");
    v.used = false;
    v.hasCreatedAt = true;
    v.createdAt = nowEpoch();
    g_vouchers.push_back(v);
    saveStore();
    redirect("/admin");
  });
  server.on("/admin/export", HTTP_GET, []() {
    User* user = currentUser();
    if (!user || user->role != "admin") { server.send(403, "text/plain", "Forbidden"); return; }
    server.sendHeader("Content-Disposition", "attachment; filename=data.json");
    server.send(200, "application/json", exportStoreJSON());
  });
  server.on("/admin/import", HTTP_POST, []() {
    User* user = currentUser();
    if (!user || user->role != "admin") { server.send(403, "text/plain", "Forbidden"); return; }
    String data = server.arg("data");
    if (data.isEmpty()) data = server.arg("plain");
    bool ok = !data.isEmpty() && importStoreJSON(data);
    redirect(ok ? "/admin?import=ok" : "/admin?import=failed");
  });

  // --- Bon cadeau imprimable (route publique, comme l'original) ---
  server.on("/gift-voucher/{}", HTTP_GET, []() {
    Voucher* v = findVoucherByCode(server.pathArg(0));
    if (!v || v->used) {
      server.send(404, "text/plain", "Voucher not found or already used");
      return;
    }
    server.send(200, "text/html", pageGiftVoucher(*v));
  });

  // --- 404 ---
  server.onNotFound([]() {
    server.send(404, "text/html", page404());
  });
}

// ---------------------------------------------------------------------------
// Initialisation
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000); // laisse le temps au port USB-CDC de s'attacher
  Serial.println("[BOOT] setup : debut");
  Serial.flush();

  if (!LittleFS.begin(true)) {  // true : formatage automatique au premier démarrage
    Serial.println("[FS] Montage impossible : formatage explicite...");
    Serial.flush();
    if (LittleFS.format() && LittleFS.begin(true)) {
      Serial.println("[FS] LittleFS formatte et monte");
    } else {
      Serial.println("[FS] ERREUR : montage LittleFS impossible !");
    }
  }
  Serial.println("[BOOT] LittleFS pret");
  Serial.flush();

  loadStore();
  Serial.println("[BOOT] donnees chargees");
  Serial.flush();

  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  if (!WiFi.softAP(AP_SSID, AP_PASS)) {
    Serial.println("[WIFI] ERREUR : demarrage du point d'acces impossible");
  } else {
    Serial.println("[WIFI] Point d'acces actif");
    Serial.print("       SSID     : ");
    Serial.println(AP_SSID);
    Serial.print("       Cle WiFi : ");
    Serial.println(AP_PASS);
    Serial.print("       Adresse  : http://");
    Serial.println(WiFi.softAPIP());
  }

  // LED intégrée éteinte au démarrage (allumée uniquement sur notification)
  neopixelWrite(LED_PIN, 0, 0, 0);

  setupRoutes();
  const char* headers[] = { "Cookie" };
  server.collectHeaders(headers, 1);
  server.begin();
  Serial.println("[HTTP] Serveur pret sur http://192.168.4.1");
  Serial.flush();
}

void loop() {
  server.handleClient();

  // --- LED intégrée : respiration selon les notifications non lues ---
  // Bleu si notification pour Sylvain, rose si pour Alice, éteinte sinon.
  // Si les deux ont des notifications, la couleur alterne à chaque cycle.
  static uint32_t lastCheck = 0;   // recalcul des notifications (1 s)
  static uint32_t lastStep = 0;    // pas d'animation (25 ms)
  static float phase = 0.0f;
  static int lastR = -1, lastG = -1, lastB = -1;
  static bool notifSylvain = false, notifAlice = false;

  uint32_t now = millis();
  if (now - lastCheck >= 1000) {
    lastCheck = now;
    notifSylvain = countNotifications("Sylvain") > 0;
    notifAlice   = countNotifications("Alice") > 0;
  }
  if (now - lastStep >= 25) {
    lastStep = now;
    int r = 0, g = 0, b = 0;
    if (notifSylvain || notifAlice) {
      phase += 0.0628f;                    // période ≈ 2,5 s
      if (phase >= 6.2832f) phase -= 6.2832f;
      float v = sinf(phase);               // respiration : montée puis repos
      if (v < 0) v = 0;
      if (notifSylvain && (!notifAlice || (((int)(phase / 6.2832f)) % 2 == 0))) {
        r = 0; g = 0; b = (int)(v * 45);   // bleu
      } else {
        r = (int)(v * 45); g = (int)(v * 18); b = (int)(v * 32); // rose
      }
    }
    if (r != lastR || g != lastG || b != lastB) {
      neopixelWrite(LED_PIN, r, g, b);
      lastR = r; lastG = g; lastB = b;
    }
  }

  // Battement de cœur : prouve la vie du serveur sur le port série
  static uint32_t lastBeat = 0;
  if (millis() - lastBeat > 10000) {
    lastBeat = millis();
    Serial.print("[LOOP] actif, uptime=");
    Serial.print(lastBeat / 1000);
    Serial.println(" s");
  }
}
