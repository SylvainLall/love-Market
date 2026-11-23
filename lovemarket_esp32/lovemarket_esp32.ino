// ===========================================================================
// Love Market V1 pour ESP32-S3 (N16R8) — application « envies »
//
// Philosophie : « Tes envies. Mes envies. Nos plaisirs. »
//   1. Chacun publie ses envies (récompense 0 à 4 crédits).
//   2. Le partenaire découvre et accepte librement (aucune obligation).
//   3. Le réalisateur signale sa réalisation.
//   4. Seul le bénéficiaire confirme : les crédits sont attribués une seule
//      fois, atomiquement, et journalisés.
//
// - Serveur HTTP embarqué (WebServer.h, bibliothèque standard ESP32)
// - Point d'accès WiFi autonome "PPmark" (aucune connexion internet requise)
// - Données persistées dans LittleFS (/data.json, écriture atomique + /data.bak)
// - Interfaces HTML embarquées en PROGMEM (style.css + favicon)
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

static Session* currentSession() {
  return sessionFind(getCookieToken());
}

static void redirect(const char* url) {
  server.sendHeader("Location", url, true);
  server.send(302, "text/plain", "");
}

static void takeFlash(String& err, String& ok) {
  Session* s = currentSession();
  if (s) { err = s->flashError; ok = s->flashSuccess; s->flashError = ""; s->flashSuccess = ""; }
  else   { err = ""; ok = ""; }
}

static void flashError(const String& msg) {
  Session* s = currentSession();
  if (s) s->flashError = msg;
}

static void flashSuccess(const String& msg) {
  Session* s = currentSession();
  if (s) s->flashSuccess = msg;
}

// Accès réservé à l'administration
static bool isAdmin(User* user) {
  return user && user->role == "admin";
}

// ---------------------------------------------------------------------------
// Helpers métier (cycle de vie des envies)
// ---------------------------------------------------------------------------
static bool isKnownCategory(const String& cat) {
  for (auto& c : g_categories) if (c.name == cat) return true;
  return false;
}

static String sanitizeCategory(const String& cat) {
  if (isKnownCategory(cat)) return cat;
  return g_categories.empty() ? String("") : g_categories[0].name;
}

static int clampCredits(const String& raw) {
  int v = raw.toInt();
  if (raw.isEmpty() || v < 0) v = 0;
  if (v > 4) v = 4;
  return v;
}

// ---------------------------------------------------------------------------
// Enregistrement des routes
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
    User* u = findUser(userId);
    if (!u || !verifyPassword(*u, password)) {
      server.send(200, "text/html", pageLogin("Identifiants incorrects"));
      return;
    }
    Session* s = sessionCreate(u->id);
    server.sendHeader("Set-Cookie", String("lm_sid=") + s->token + "; Path=/; HttpOnly; SameSite=Lax");
    redirect("/dashboard");
  });
  server.on("/logout", HTTP_GET, []() {
    sessionDestroy(getCookieToken());
    server.sendHeader("Set-Cookie", String("lm_sid=; Path=/; HttpOnly; Max-Age=0"));
    redirect("/login");
  });

  // --- Accueil (consomme les messages flash de la session) ---
  server.on("/dashboard", HTTP_GET, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    String err, ok;
    takeFlash(err, ok);
    server.send(200, "text/html", pageHome(*user, err, ok));
  });

  // --- Rubrique « Mes envies » ---
  server.on("/my-wishes", HTTP_GET, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    server.send(200, "text/html", pageMyWishes(*user));
  });

  // --- Créer une envie ---
  server.on("/wishes/new", HTTP_GET, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    server.send(200, "text/html", pageWishForm(*user, nullptr));
  });
  server.on("/wishes", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    User* partner = findPartnerOf(user->id);
    String title = server.arg("title");
    if (title.isEmpty()) {
      flashError("Le titre est obligatoire.");
      return redirect("/wishes/new");
    }
    Wish* w = createWish(user->id, title, server.arg("description"),
                         sanitizeCategory(server.arg("category")),
                         clampCredits(server.arg("credits")), server.arg("desiredDate"));
    if (partner) {
      addNotification(partner->id, "new_wish", w->id,
                      user->displayName + " a publié une nouvelle envie : « " + w->title + " »");
    }
    saveStore();
    flashSuccess("Ton envie a été publiée. Elle attend d'être découverte !");
    redirect("/my-wishes");
  });

  // --- Modifier une envie (tant qu'elle est disponible) ---
  server.on("/wishes/{}/edit", HTTP_GET, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    Wish* w = findWish(server.pathArg(0));
    if (!w || w->userId != user->id || w->status != "open") return redirect("/my-wishes");
    server.send(200, "text/html", pageWishForm(*user, w));
  });
  server.on("/wishes/{}/edit", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    Wish* w = findWish(server.pathArg(0));
    if (!w || w->userId != user->id || w->status != "open") return redirect("/my-wishes");
    String title = server.arg("title");
    if (!title.isEmpty()) w->title = title;
    w->description = server.arg("description");
    w->category = sanitizeCategory(server.arg("category"));
    if (!server.arg("credits").isEmpty()) w->credits = clampCredits(server.arg("credits"));
    w->desiredDate = server.arg("desiredDate");
    saveStore();
    flashSuccess("Ton envie a été modifiée.");
    redirect("/my-wishes");
  });

  // --- Supprimer une envie (uniquement si disponible) ---
  server.on("/wishes/{}/delete", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    Wish* w = findWish(server.pathArg(0));
    if (w && w->userId == user->id && w->status == "open") {
      for (size_t i = 0; i < g_wishes.size(); i++) {
        if (g_wishes[i].id == w->id) { g_wishes.erase(g_wishes.begin() + i); break; }
      }
      saveStore();
      flashSuccess("L'envie a été supprimée.");
    }
    redirect("/my-wishes");
  });

  // --- Rubrique « Ses envies » (marque les notifications et les lectures) ---
  server.on("/partner-wishes", HTTP_GET, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    bool changed = false;
    for (auto& w : g_wishes) {
      if (w.status != "open" || w.userId == user->id) continue;
      bool already = false;
      for (auto& r : w.readBy) if (r == user->id) { already = true; break; }
      if (!already) { w.readBy.push_back(user->id); changed = true; }
    }
    if (changed) saveStore();
    markRubriqueNotificationsRead(user->id, RUBRIQUE_PARTNER);
    server.send(200, "text/html", pagePartnerWishes(*user));
  });

  // --- Accepter une envie (engagement volontaire, une seule personne) ---
  server.on("/wishes/{}/accept", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    Wish* w = findWish(server.pathArg(0));
    if (!w || w->status != "open" || w->userId == user->id) {
      flashError("Cette envie n'est plus disponible.");
      return redirect("/partner-wishes");
    }
    w->status = "accepted";
    w->acceptedBy = user->id;
    w->hasAcceptedAt = true;
    w->acceptedAt = nowEpoch();
    w->hasRealizationReportedAt = false;
    w->realizationReportedAt = 0;
    addNotification(w->userId, "wish_accepted", w->id,
                    user->displayName + " se porte volontaire pour réaliser « " + w->title + " »");
    saveStore();
    flashSuccess("Tu as accepté cette envie — c'est un engagement libre, à ton rythme.");
    redirect("/exchanges");
  });

  // --- Rubrique « Nos échanges » (marque les notifications comme lues) ---
  server.on("/exchanges", HTTP_GET, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    markRubriqueNotificationsRead(user->id, RUBRIQUE_EXCHANGES);
    server.send(200, "text/html", pageExchanges(*user));
  });

  // --- Signaler la réalisation (par le réalisateur uniquement) ---
  server.on("/wishes/{}/report-done", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    Wish* w = findWish(server.pathArg(0));
    if (!w || w->status != "accepted" || w->acceptedBy != user->id || w->hasRealizationReportedAt) {
      return redirect("/exchanges");
    }
    w->hasRealizationReportedAt = true;
    w->realizationReportedAt = nowEpoch();
    User* author = findUser(w->userId);
    if (author) {
      addNotification(author->id, "realization_reported", w->id,
                      user->displayName + " a réalisé « " + w->title +
                      " » — confirme que c'est fait pour lui offrir ses " + String(w->credits) + " ♥");
    }
    saveStore();
    flashSuccess("Réalisation signalée ! " + (author ? author->displayName : String("")) + " va confirmer.");
    redirect("/exchanges");
  });

  // --- Confirmer la réalisation (bénéficiaire uniquement, crédits attribués ICI) ---
  server.on("/wishes/{}/confirm", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    Wish* w = findWish(server.pathArg(0));
    // Garde forte : statut accepted + appelant = bénéficiaire (auteur de l'envie)
    if (!w || w->status != "accepted" || w->acceptedBy.isEmpty() || w->userId != user->id) {
      flashError("Seul le bénéficiaire de l'envie peut confirmer sa réalisation.");
      return redirect("/exchanges");
    }
    // Attribution des crédits : une seule fois, atomique avec le changement de statut
    w->status = "completed";
    w->hasCompletedAt = true;
    w->completedAt = nowEpoch();
    User* maker = findUser(w->acceptedBy);
    if (w->credits > 0 && maker) {
      maker->credits += w->credits;
      addTransaction(maker->id, w->credits, "wish_completed", w->id,
                     "Envie réalisée : " + w->title);
    }
    if (maker) {
      addNotification(maker->id, "realization_confirmed", w->id,
                      user->displayName + " a confirmé la réalisation de « " + w->title +
                      " »" + (w->credits > 0 ? " — +" + String(w->credits) + " ♥ pour toi !" : " — merci !"));
    }
    saveStore();
    flashSuccess("Merci d'avoir confirmé ! Ce moment rejoint Nos moments.");
    redirect("/moments");
  });

  // --- Abandon (par le réalisateur) : l'envie redevient disponible ---
  server.on("/wishes/{}/abandon", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    Wish* w = findWish(server.pathArg(0));
    if (!w || w->status != "accepted" || w->acceptedBy != user->id) return redirect("/exchanges");
    w->status = "open";
    w->acceptedBy = "";
    w->hasAcceptedAt = false;
    w->acceptedAt = 0;
    w->hasRealizationReportedAt = false;
    w->realizationReportedAt = 0;
    addNotification(w->userId, "wish_abandoned", w->id,
                    user->displayName + " ne pourra pas réaliser « " + w->title +
                    " ». L'envie est de nouveau disponible.");
    saveStore();
    flashSuccess("L'envie est de nouveau disponible. Aucun crédit n'a été attribué.");
    redirect("/exchanges");
  });

  // --- Annulation (par le bénéficiaire/auteur) : aucun crédit ---
  server.on("/wishes/{}/cancel", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    Wish* w = findWish(server.pathArg(0));
    if (!w || w->status != "accepted" || w->userId != user->id) return redirect("/exchanges");
    w->status = "cancelled";
    w->hasCancelledAt = true;
    w->cancelledAt = nowEpoch();
    if (!w->acceptedBy.isEmpty()) {
      addNotification(w->acceptedBy, "wish_cancelled", w->id,
                      user->displayName + " a annulé l'envie « " + w->title +
                      " ». Aucun crédit n'est attribué, et c'est tout à fait ok.");
    }
    saveStore();
    flashSuccess("L'envie a été annulée. Aucun crédit n'a été attribué.");
    redirect("/my-wishes");
  });

  // --- Rubrique « Nos moments » (marque les notifications comme lues) ---
  server.on("/moments", HTTP_GET, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    markRubriqueNotificationsRead(user->id, RUBRIQUE_MOMENTS);
    server.send(200, "text/html", pageMoments(*user));
  });

  // --- Commentaire sur un moment réalisé (les deux partenaires) ---
  server.on("/moments/{}/comment", HTTP_POST, []() {
    User* user = currentUser();
    if (!user) return redirect("/login");
    Wish* w = findWish(server.pathArg(0));
    if (w && w->status == "completed" && (w->userId == user->id || w->acceptedBy == user->id)) {
      w->memoryComment = server.arg("comment");
      w->memoryComment.trim();
      saveStore();
      flashSuccess("Commentaire enregistré.");
    }
    redirect("/moments");
  });

  // --- Administration (invisible pour Alice, contrôlée côté serveur) ---
  server.on("/admin", HTTP_GET, []() {
    User* user = currentUser();
    if (!isAdmin(user)) { server.send(403, "text/plain", "Forbidden"); return; }
    String importStatus = server.hasArg("import") ? server.arg("import") : String("");
    server.send(200, "text/html", pageAdmin(*user, importStatus));
  });

  server.on("/admin/credits", HTTP_POST, []() {
    User* user = currentUser();
    if (!isAdmin(user)) { server.send(403, "text/plain", "Forbidden"); return; }
    User* target = findUser(server.arg("targetId"));
    int amount = server.arg("amount").toInt();
    String motif = server.arg("motif");
    motif.trim();
    if (!target || amount == 0 || motif.isEmpty()) {
      flashError("Ajustement refusé : utilisateur, montant et motif sont obligatoires.");
      return redirect("/admin");
    }
    int newCredits = target->credits + amount;
    if (newCredits < 0) {
      flashError("Ajustement refusé : les crédits ne peuvent pas devenir négatifs.");
      return redirect("/admin");
    }
    target->credits = newCredits;
    addTransaction(target->id, amount, "admin_adjustment", "",
                   "Ajustement par " + user->displayName + " : " + motif);
    saveStore();
    flashSuccess("Crédits de " + target->displayName + " ajustés (" + String(amount) + " ♥) et journalisés.");
    redirect("/admin");
  });

  server.on("/admin/levels", HTTP_POST, []() {
    User* user = currentUser();
    if (!isAdmin(user)) { server.send(403, "text/plain", "Forbidden"); return; }
    int index = server.arg("levelIndex").toInt();
    int threshold = server.arg("threshold").toInt();
    if (index >= 0 && index < (int)g_levels.size() && threshold >= 0) {
      g_levels[index].threshold = threshold;
      saveStore();
    }
    redirect("/admin");
  });

  server.on("/admin/categories", HTTP_POST, []() {
    User* user = currentUser();
    if (!isAdmin(user)) { server.send(403, "text/plain", "Forbidden"); return; }
    String action = server.arg("action");
    if (action == "add") {
      String name = server.arg("name");
      name.trim();
      if (!name.isEmpty() && !isKnownCategory(name)) {
        Category c;
        c.name = name;
        c.icon = server.arg("icon").isEmpty() ? String("✨") : server.arg("icon");
        g_categories.push_back(c);
        saveStore();
      }
    } else if (action == "delete") {
      int index = server.arg("index").toInt();
      if (index >= 0 && index < (int)g_categories.size() && g_categories.size() > 1) {
        g_categories.erase(g_categories.begin() + index);
        saveStore();
      }
    }
    redirect("/admin");
  });

  server.on("/admin/export", HTTP_GET, []() {
    User* user = currentUser();
    if (!isAdmin(user)) { server.send(403, "text/plain", "Forbidden"); return; }
    server.sendHeader("Content-Disposition", "attachment; filename=data.json");
    server.send(200, "application/json", exportStoreJSON());
  });

  server.on("/admin/legacy", HTTP_GET, []() {
    User* user = currentUser();
    if (!isAdmin(user)) { server.send(403, "text/plain", "Forbidden"); return; }
    String legacy = exportLegacyJSON();
    if (legacy.isEmpty()) { server.send(404, "text/plain", "No legacy archive"); return; }
    server.sendHeader("Content-Disposition", "attachment; filename=legacy.json");
    server.send(200, "application/json", legacy);
  });

  server.on("/admin/import", HTTP_POST, []() {
    User* user = currentUser();
    if (!isAdmin(user)) { server.send(403, "text/plain", "Forbidden"); return; }
    String data = server.arg("data");
    if (data.isEmpty()) data = server.arg("plain");
    bool ok = !data.isEmpty() && importStoreJSON(data);
    redirect(ok ? "/admin?import=ok" : "/admin?import=failed");
  });

  server.on("/admin/reset", HTTP_POST, []() {
    User* user = currentUser();
    if (!isAdmin(user)) { server.send(403, "text/plain", "Forbidden"); return; }
    String scope = server.arg("scope");
    String confirm = server.arg("confirm");
    if (confirm != "yes") {
      flashError("Réinitialisation annulée : confirmation explicite requise.");
      return redirect("/admin");
    }
    if (scope == "notifications") {
      g_notifications.clear();
      g_nextNotificationId = 1;
      flashSuccess("Notifications réinitialisées.");
    } else if (scope == "wishes") {
      for (size_t i = g_wishes.size(); i > 0; i--) {
        if (g_wishes[i - 1].status == "open") g_wishes.erase(g_wishes.begin() + (i - 1));
      }
      flashSuccess("Envies disponibles supprimées — l'historique est conservé.");
    } else if (scope == "all") {
      g_wishes.clear();
      g_transactions.clear();
      g_notifications.clear();
      g_nextWishId = 1;
      g_nextTransactionId = 1;
      g_nextNotificationId = 1;
      flashSuccess("Données d'activité réinitialisées (utilisateurs et configuration conservés).");
    } else {
      flashError("Portée de réinitialisation inconnue.");
      return redirect("/admin");
    }
    saveStore();
    redirect("/admin");
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
