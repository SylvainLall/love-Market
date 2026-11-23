#include "model.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <time.h>

// ---------------------------------------------------------------------------
// Store global
// ---------------------------------------------------------------------------
std::vector<User> g_users;
std::vector<Service> g_services;
std::vector<Offer> g_offers;
std::vector<Voucher> g_vouchers;
std::vector<SpecialEvent> g_events;
std::vector<RelLevel> g_levels;
int g_nextServiceId = 7;
int g_nextOfferId = 1;
int g_nextVoucherId = 1;
int g_nextEventId = 1;

static const char* DATA_FILE = "/data.json";

// ---------------------------------------------------------------------------
// Temps : heure de compilation du firmware + uptime (pas de NTP en mode AP)
// ---------------------------------------------------------------------------
static time_t parseBuildEpoch() {
  // __DATE__ = "Nov 22 2026"  __TIME__ = "14:30:00"
  const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  char mon[4] = {0}; int day = 1, year = 2026, hour = 0, minute = 0, sec = 0;
  sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
  sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &sec);
  int month = 1;
  for (int i = 0; i < 12; i++) if (strcmp(mon, months[i]) == 0) { month = i + 1; break; }
  struct tm t = {};
  t.tm_year = year - 1900; t.tm_mon = month - 1; t.tm_mday = day;
  t.tm_hour = hour; t.tm_min = minute; t.tm_sec = sec;
  return mktime(&t);
}

time_t nowEpoch() {
  static time_t bootEpoch = parseBuildEpoch();
  return bootEpoch + (time_t)(millis() / 1000);
}

String formatDateTimeFR(time_t t, bool withSeconds) {
  struct tm lt;
  localtime_r(&t, &lt);
  char buf[32];
  if (withSeconds) snprintf(buf, sizeof(buf), "%02d/%02d/%04d %02d:%02d:%02d",
                            lt.tm_mday, lt.tm_mon + 1, lt.tm_year + 1900,
                            lt.tm_hour, lt.tm_min, lt.tm_sec);
  else snprintf(buf, sizeof(buf), "%02d/%02d/%04d %02d:%02d",
                lt.tm_mday, lt.tm_mon + 1, lt.tm_year + 1900,
                lt.tm_hour, lt.tm_min);
  return String(buf);
}

String toISO8601(time_t t) {
  struct tm lt;
  gmtime_r(&t, &lt);
  char buf[40];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S.000Z", &lt);
  return String(buf);
}

time_t parseISO8601(const String& iso) {
  int y = 1970, mo = 1, d = 1, h = 0, mi = 0, s = 0;
  if (sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s) < 3) return 0;
  struct tm t = {};
  t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d;
  t.tm_hour = h; t.tm_min = mi; t.tm_sec = s;
  return mktime(&t);
}

// ---------------------------------------------------------------------------
// Données par défaut (identiques à defaultData du index.js Node)
// ---------------------------------------------------------------------------
static void loadDefaultStore() {
  g_users.clear();
  g_users.push_back({"Alice",   "Poupoune",  "user",  "Alice",   120});
  g_users.push_back({"Sylvain", "31051989", "admin", "Sylvain", 120});

  g_services.clear();
  g_services.push_back({"1", "Sylvain", "Nettoyage de la salle de bain",
    "Salle de bain propre, miroir et lavabo nickels, sol lavé.",
    "Maison", 25, "\xF0\x9F\x9B\x81"}); // 🛁
  g_services.push_back({"2", "Sylvain", "Petit déjeuner au lit",
    "Café ou thé, jus, viennoiserie et câlin du matin.",
    "Romantique", 30, "\xF0\x9F\xA5\x90"}); // 🥐
  g_services.push_back({"3", "Sylvain", "Soirée resto sans les enfants",
    "Je m'occupe de tout : réservation, garde des enfants, transport.",
    "Sortie", 60, "\xF0\x9F\x8D\xBD\xEF\xB8\x8F"}); // 🍽️
  g_services.push_back({"4", "Alice", "Soirée série et plaid",
    "Choix de la série, grignotage préparé, ambiance cosy sous le plaid.",
    "Détente", 35, "\xF0\x9F\x93\xBA"}); // 📺
  g_services.push_back({"5", "Alice", "Massage 30 minutes",
    "Massage dos / nuque avec musique douce.",
    "Détente", 40, "\xF0\x9F\x92\x86"}); // 💆
  g_services.push_back({"6", "Alice", "Garde des enfants pour ta soirée",
    "Je m'occupe des enfants pendant que tu profites de ta soirée.",
    "Maison", 50, "\xF0\x9F\x91\xB6"}); // 👶

  g_offers.clear();
  g_vouchers.clear();
  g_events.clear();

  g_levels.clear();
  g_levels.push_back({"Coloc",    0,    "\xF0\x9F\x8F\xA0"});    // 🏠
  g_levels.push_back({"Complice", 100,  "\xF0\x9F\xA4\x9D"});    // 🤝
  g_levels.push_back({"Équipe",   300,  "\xE2\x9A\xA1"});        // ⚡
  g_levels.push_back({"Connecté", 600,  "\xF0\x9F\x92\xAB"});    // 💫
  g_levels.push_back({"In Love",  1000, "\xF0\x9F\x92\x95"});    // 💕

  g_nextServiceId = 7; g_nextOfferId = 1; g_nextVoucherId = 1; g_nextEventId = 1;
}

// ---------------------------------------------------------------------------
// Lecture du data.json (LittleFS) ou données par défaut
// ---------------------------------------------------------------------------
static String strOr(const JsonVariant& v, const char* def = "") {
  if (v.isNull()) return String(def);
  return String(v.as<const char*>());
}

// ---------------------------------------------------------------------------
// Mots de passe fixés (demande utilisateur) : Alice = "Poupoune", Sylvain = "31051989"
// Réappliqués après chaque chargement/import pour écraser d'anciennes valeurs
// persistées dans data.json.
// ---------------------------------------------------------------------------
static const char* FIXED_PASS_ALICE = "Poupoune";
static const char* FIXED_PASS_SYLVAIN = "31051989";

static void applyFixedPasswords() {
  for (auto& u : g_users) {
    if (u.id == "Alice")   u.password = FIXED_PASS_ALICE;
    if (u.id == "Sylvain") u.password = FIXED_PASS_SYLVAIN;
  }
}

static void applyStoreFromJson(JsonDocument& doc) {
  std::vector<User> users;
  std::vector<Service> services;
  std::vector<Offer> offers;
  std::vector<Voucher> vouchers;
  std::vector<SpecialEvent> events;
  std::vector<RelLevel> levels;

  if (doc["users"].is<JsonArray>()) {
    for (JsonObject o : doc["users"].as<JsonArray>()) {
      User u;
      u.id = strOr(o["id"]); u.password = strOr(o["password"]);
      u.role = strOr(o["role"]); u.displayName = strOr(o["displayName"]);
      u.credits = o["credits"] | 0;
      users.push_back(u);
    }
  }
  if (doc["services"].is<JsonArray>()) {
    for (JsonObject o : doc["services"].as<JsonArray>()) {
      Service s;
      s.id = strOr(o["id"]); s.sellerId = strOr(o["sellerId"]);
      s.title = strOr(o["title"]); s.description = strOr(o["description"]);
      s.category = strOr(o["category"]); s.price = o["price"] | 0;
      s.icon = strOr(o["icon"]);
      services.push_back(s);
    }
  }
  if (doc["offers"].is<JsonArray>()) {
    for (JsonObject o : doc["offers"].as<JsonArray>()) {
      Offer f;
      f.id = strOr(o["id"]);
      f.serviceId = o["serviceId"].isNull() ? String() : strOr(o["serviceId"]);
      f.fromUserId = strOr(o["fromUserId"]); f.toUserId = strOr(o["toUserId"]);
      f.offeredPrice = o["offeredPrice"] | 0;
      f.comment = strOr(o["comment"]); f.status = strOr(o["status"]);
      f.type = strOr(o["type"]);
      f.serviceTitle = strOr(o["serviceTitle"]);
      f.serviceCategory = strOr(o["serviceCategory"]);
      f.serviceIcon = strOr(o["serviceIcon"]);
      f.originalRequesterId = strOr(o["originalRequesterId"]);
      f.parentOfferId = strOr(o["parentOfferId"]);
      f.rootOfferId = strOr(o["rootOfferId"]);
      f.isSuperService = o["isSuperService"] | false;
      f.superServiceBonus = o["superServiceBonus"] | 0;
      if (!o["debitedAmount"].isNull()) f.debitedAmount = o["debitedAmount"] | -1;
      f.eventId = strOr(o["eventId"]);
      if (!o["createdAt"].isNull()) {
        // Compatible ancien format ISO et nouveau format (epoch)
        JsonVariant cv = o["createdAt"];
        if (cv.is<const char*>()) { f.createdAt = parseISO8601(cv.as<String>()); f.hasCreatedAt = true; }
        else if (cv.is<int>())    { f.createdAt = (time_t)(cv.as<int>());     f.hasCreatedAt = true; }
        else if (cv.is<long>())   { f.createdAt = (time_t)(cv.as<long>());    f.hasCreatedAt = true; }
      }
      f.feedback = strOr(o["feedback"]);
      if (!o["feedbackAt"].isNull()) { f.feedbackAt = parseISO8601(o["feedbackAt"].as<String>()); f.hasFeedbackAt = true; }
      if (o["readBy"].is<JsonArray>()) {
        for (JsonVariant r : o["readBy"].as<JsonArray>()) f.readBy.push_back(r.as<String>());
      }
      offers.push_back(f);
    }
  }
  if (doc["giftVouchers"].is<JsonArray>()) {
    for (JsonObject o : doc["giftVouchers"].as<JsonArray>()) {
      Voucher v;
      v.id = strOr(o["id"]); v.code = strOr(o["code"]);
      v.credits = o["credits"] | 0; v.comment = strOr(o["comment"]);
      v.used = o["used"] | false; v.usedBy = strOr(o["usedBy"]);
      if (!o["createdAt"].isNull()) { v.createdAt = parseISO8601(o["createdAt"].as<String>()); v.hasCreatedAt = true; }
      if (!o["usedAt"].isNull())    { v.usedAt = parseISO8601(o["usedAt"].as<String>());       v.hasUsedAt = true; }
      vouchers.push_back(v);
    }
  }
  if (doc["specialEvents"].is<JsonArray>()) {
    for (JsonObject o : doc["specialEvents"].as<JsonArray>()) {
      SpecialEvent e;
      e.id = strOr(o["id"]); e.title = strOr(o["title"]);
      e.description = strOr(o["description"]);
      e.credits = o["credits"] | 0;
      e.targetUser = strOr(o["targetUser"], "both");
      e.status = strOr(o["status"], "active");
      if (!o["createdAt"].isNull()) { e.createdAt = parseISO8601(o["createdAt"].as<String>()); e.hasCreatedAt = true; }
      e.assignedTo = o["assignedTo"].isNull() ? String() : strOr(o["assignedTo"]);
      e.completedBy = o["completedBy"].isNull() ? String() : strOr(o["completedBy"]);
      if (!o["completedAt"].isNull()) { e.completedAt = parseISO8601(o["completedAt"].as<String>()); e.hasCompletedAt = true; }
      events.push_back(e);
    }
  }
  if (doc["relationshipLevels"].is<JsonArray>()) {
    for (JsonObject o : doc["relationshipLevels"].as<JsonArray>()) {
      RelLevel l;
      l.name = strOr(o["name"]); l.minCredits = o["minCredits"] | 0; l.icon = strOr(o["icon"]);
      levels.push_back(l);
    }
  }

  // Tout a été désérialisé sans erreur structurelle : on remplace le store
  if (!users.empty()) g_users = users;
  g_services = services;
  g_offers = offers;
  g_vouchers = vouchers;
  g_events = events;
  if (!levels.empty()) g_levels = levels;
  g_nextServiceId = doc["nextServiceId"] | (int)(g_services.size() + 1);
  g_nextOfferId   = doc["nextOfferId"]   | (int)(g_offers.size() + 1);
  g_nextVoucherId = doc["nextVoucherId"] | (int)(g_vouchers.size() + 1);
  g_nextEventId   = doc["nextEventId"]   | (int)(g_events.size() + 1);

  applyFixedPasswords();
}

void loadStore() {
  loadDefaultStore();
  File f = LittleFS.open(DATA_FILE, "r");
  if (!f) { Serial.println("[DATA] data.json absent : donnees par defaut"); return; }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) { Serial.printf("[DATA] Erreur JSON (%s) : donnees par defaut\n", err.c_str()); return; }
  applyStoreFromJson(doc);
  Serial.println("[DATA] data.json charge depuis LittleFS");
}

// ---------------------------------------------------------------------------
// Sérialisation du store (utilisée pour la sauvegarde et l'export admin)
// ---------------------------------------------------------------------------
void saveStoreTo(JsonDocument& doc);

String exportStoreJSON() {
  JsonDocument doc;
  saveStoreTo(doc);
  String out;
  serializeJson(doc, out);
  return out;
}

bool importStoreJSON(const String& json) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return false;
  if (!doc["users"].is<JsonArray>()) return false;
  applyStoreFromJson(doc);
  saveStore();
  return true;
}

// ---------------------------------------------------------------------------
// Sauvegarde (format identique à la version Node : JSON indenté)
// ---------------------------------------------------------------------------
static void addOptStr(JsonObject& o, const char* key, const String& v) {
  if (!v.isEmpty()) o[key] = v;
}

void saveStoreTo(JsonDocument& doc) {
  JsonObject root = doc.to<JsonObject>();

  JsonArray users = root["users"].to<JsonArray>();
  for (auto& u : g_users) {
    JsonObject o = users.add<JsonObject>();
    o["id"] = u.id; o["password"] = u.password; o["role"] = u.role;
    o["displayName"] = u.displayName; o["credits"] = u.credits;
  }

  JsonArray services = root["services"].to<JsonArray>();
  for (auto& s : g_services) {
    JsonObject o = services.add<JsonObject>();
    o["id"] = s.id; o["sellerId"] = s.sellerId; o["title"] = s.title;
    o["description"] = s.description; o["category"] = s.category;
    o["price"] = s.price; o["icon"] = s.icon;
  }

  JsonArray offers = root["offers"].to<JsonArray>();
  for (auto& f : g_offers) {
    JsonObject o = offers.add<JsonObject>();
    o["id"] = f.id;
    if (f.serviceId.isEmpty()) o["serviceId"] = nullptr; else o["serviceId"] = f.serviceId;
    o["fromUserId"] = f.fromUserId; o["toUserId"] = f.toUserId;
    o["offeredPrice"] = f.offeredPrice; o["comment"] = f.comment;
    o["status"] = f.status; o["type"] = f.type;
    addOptStr(o, "serviceTitle", f.serviceTitle);
    addOptStr(o, "serviceCategory", f.serviceCategory);
    addOptStr(o, "serviceIcon", f.serviceIcon);
    addOptStr(o, "originalRequesterId", f.originalRequesterId);
    addOptStr(o, "parentOfferId", f.parentOfferId);
    addOptStr(o, "rootOfferId", f.rootOfferId);
    if (f.isSuperService) { o["isSuperService"] = true; o["superServiceBonus"] = f.superServiceBonus; }
    if (f.debitedAmount >= 0) o["debitedAmount"] = f.debitedAmount;
    addOptStr(o, "eventId", f.eventId);
    if (f.hasCreatedAt) o["createdAt"] = toISO8601(f.createdAt);
    addOptStr(o, "feedback", f.feedback);
    if (f.hasFeedbackAt) o["feedbackAt"] = toISO8601(f.feedbackAt);
    if (!f.readBy.empty()) {
      JsonArray rb = o["readBy"].to<JsonArray>();
      for (auto& r : f.readBy) rb.add(r);
    }
  }

  JsonArray vouchers = root["giftVouchers"].to<JsonArray>();
  for (auto& v : g_vouchers) {
    JsonObject o = vouchers.add<JsonObject>();
    o["id"] = v.id; o["code"] = v.code; o["credits"] = v.credits;
    o["comment"] = v.comment; o["used"] = v.used;
    if (!v.usedBy.isEmpty()) o["usedBy"] = v.usedBy;
    if (v.hasCreatedAt) o["createdAt"] = toISO8601(v.createdAt);
    if (v.hasUsedAt)    o["usedAt"] = toISO8601(v.usedAt);
  }

  JsonArray events = root["specialEvents"].to<JsonArray>();
  for (auto& e : g_events) {
    JsonObject o = events.add<JsonObject>();
    o["id"] = e.id; o["title"] = e.title; o["description"] = e.description;
    o["credits"] = e.credits; o["targetUser"] = e.targetUser; o["status"] = e.status;
    if (e.hasCreatedAt) o["createdAt"] = toISO8601(e.createdAt);
    if (e.assignedTo.isEmpty()) o["assignedTo"] = nullptr; else o["assignedTo"] = e.assignedTo;
    if (e.completedBy.isEmpty()) o["completedBy"] = nullptr; else o["completedBy"] = e.completedBy;
    if (e.hasCompletedAt) o["completedAt"] = toISO8601(e.completedAt);
  }

  JsonArray levels = root["relationshipLevels"].to<JsonArray>();
  for (auto& l : g_levels) {
    JsonObject o = levels.add<JsonObject>();
    o["name"] = l.name; o["minCredits"] = l.minCredits; o["icon"] = l.icon;
  }

  root["nextServiceId"] = g_nextServiceId;
  root["nextOfferId"] = g_nextOfferId;
  root["nextVoucherId"] = g_nextVoucherId;
  root["nextEventId"] = g_nextEventId;
}

void saveStore() {
  JsonDocument doc;
  saveStoreTo(doc);
  File f = LittleFS.open(DATA_FILE, "w");
  if (!f) { Serial.println("[DATA] ERREUR ouverture data.json en ecriture"); return; }
  serializeJsonPretty(doc, f);
  f.close();
  Serial.println("[DATA] Donnees sauvegardees");
}

// ---------------------------------------------------------------------------
// Recherches
// ---------------------------------------------------------------------------
User* findUser(const String& id) {
  for (auto& u : g_users) if (u.id == id) return &u;
  return nullptr;
}

User* findPartnerOf(const String& userId) {
  for (auto& u : g_users) if (u.id != userId) return &u;
  return nullptr;
}

Service* findService(const String& id) {
  for (auto& s : g_services) if (s.id == id) return &s;
  return nullptr;
}

Offer* findOffer(const String& id) {
  for (auto& o : g_offers) if (o.id == id) return &o;
  return nullptr;
}

Voucher* findVoucherByCode(const String& code) {
  for (auto& v : g_vouchers) if (v.code == code) return &v;
  return nullptr;
}

SpecialEvent* findEvent(const String& id) {
  for (auto& e : g_events) if (e.id == id) return &e;
  return nullptr;
}

// ---------------------------------------------------------------------------
// Logique métier
// ---------------------------------------------------------------------------
void getRelationshipData(RelData& out) {
  time_t limit = nowEpoch() - 30 * 24 * 3600;
  int total = 0;
  for (auto& o : g_offers) {
    if (!o.hasCreatedAt) continue;
    if (o.createdAt < limit) continue;
    if (o.status != "accepted" && o.status != "realized") continue;
    int credits = o.offeredPrice;
    if (o.isSuperService) credits += o.superServiceBonus;
    total += credits;
  }
  out.totalCredits = total;
  out.levelIndex = 0;
  for (int i = (int)g_levels.size() - 1; i >= 0; i--) {
    if (total >= g_levels[i].minCredits) { out.levelIndex = i; break; }
  }
  out.nextIndex = (out.levelIndex + 1 < (int)g_levels.size()) ? out.levelIndex + 1 : -1;
}

int countNotifications(const String& userId) {
  int n = 0;
  for (auto& o : g_offers) {
    bool involved = (o.fromUserId == userId || o.toUserId == userId);
    bool hasNotRead = true;
    for (auto& r : o.readBy) if (r == userId) { hasNotRead = false; break; }
    bool statusChange = (o.status == "accepted" || o.status == "rejected" ||
                         o.status == "countered" || o.status == "realized");
    bool pendingForMe = (o.status == "pending" && o.toUserId == userId);
    if (involved && hasNotRead && (statusChange || pendingForMe)) n++;
  }
  return n;
}

int superServiceBonus(int basePrice) {
  return (basePrice * 3 + 5) / 10; // arrondi entier de basePrice * 0.3
}

Offer* createOffer(const String& fromUserId, const String& toUserId, int price,
                   const String& type, const String& comment) {
  Offer o;
  o.id = String(g_nextOfferId++);
  o.serviceId = "";
  o.fromUserId = fromUserId;
  o.toUserId = toUserId;
  o.offeredPrice = price;
  o.comment = comment;
  o.status = "pending";
  o.type = type;
  o.hasCreatedAt = true;
  o.createdAt = nowEpoch();
  g_offers.push_back(o);
  return &g_offers.back();
}

// ---------------------------------------------------------------------------
// Sessions
// ---------------------------------------------------------------------------
static Session g_sessions[LM_SESSION_MAX];
static int g_sessionCursor = 0;

static String randomToken() {
  // random() utilise esp_random() sur ESP32 (source matérielle)
  static const char hex[] = "0123456789abcdef";
  char buf[33];
  for (int i = 0; i < 16; i++) {
    int r = (int)random(16);
    buf[i * 2]     = hex[r];
    buf[i * 2 + 1] = hex[(int)random(16)];
  }
  buf[32] = 0;
  return String(buf);
}

Session* sessionCreate(const String& userId) {
  Session* s = &g_sessions[g_sessionCursor];
  g_sessionCursor = (g_sessionCursor + 1) % LM_SESSION_MAX;
  s->active = true;
  s->token = randomToken();
  s->userId = userId;
  s->flashError = "";
  s->flashSuccess = "";
  return s;
}

Session* sessionFind(const String& token) {
  if (token.isEmpty()) return nullptr;
  for (auto& s : g_sessions) if (s.active && s.token == token) return &s;
  return nullptr;
}

void sessionDestroy(const String& token) {
  for (auto& s : g_sessions) if (s.active && s.token == token) { s.active = false; s.token = ""; }
}
