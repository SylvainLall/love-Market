#include "model.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <time.h>
#include "mbedtls/sha256.h"

// ---------------------------------------------------------------------------
// Store global
// ---------------------------------------------------------------------------
std::vector<User> g_users;
std::vector<Wish> g_wishes;
std::vector<CreditTransaction> g_transactions;
std::vector<Notification> g_notifications;
std::vector<Category> g_categories;
std::vector<RelLevel> g_levels;
int g_nextWishId = 1;
int g_nextTransactionId = 1;
int g_nextNotificationId = 1;

static const char* DATA_FILE = "/data.json";
static const char* DATA_TMP = "/data.tmp";
static const char* DATA_BAK = "/data.bak";
static const char* LEGACY_FILE = "/legacy.json";
static const char* V0_BACKUP_FILE = "/data_v0_backup.json";

// ---------------------------------------------------------------------------
// Temps : heure de compilation du firmware + uptime (pas de NTP en mode AP)
// ---------------------------------------------------------------------------
static time_t parseBuildEpoch() {
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
// Mots de passe : SHA-256 + sel (jamais de mot de passe en clair dans data.json)
// ---------------------------------------------------------------------------
String sha256Hex(const String& data) {
  unsigned char out[32];
  mbedtls_sha256((const unsigned char*)data.c_str(), data.length(), out, 0);
  static const char hex[] = "0123456789abcdef";
  String s;
  s.reserve(64);
  for (int i = 0; i < 32; i++) {
    s += hex[(out[i] >> 4) & 0xF];
    s += hex[out[i] & 0xF];
  }
  return s;
}

String hashPassword(const String& salt, const String& password) {
  return salt + "$" + sha256Hex(salt + password);
}

static String randomSalt() {
  static const char hex[] = "0123456789abcdef";
  String s;
  s.reserve(32);
  for (int i = 0; i < 16; i++) {
    s += hex[(int)random(16)];
    s += hex[(int)random(16)];
  }
  return s;
}

bool verifyPassword(User& u, const String& password) {
  int dollar = u.passwordHash.indexOf('$');
  if (dollar <= 0) return false; // format invalide -> refus
  String salt = u.passwordHash.substring(0, dollar);
  return hashPassword(salt, password) == u.passwordHash;
}

// Mots de passe fixés (choix utilisateur) : Alice = "Poupoune", Sylvain = "31051989"
// Réappliqués après chaque chargement/import : le sel existant est conservé,
// le hash est recalculé (écrase d'anciennes valeurs persistées).
static const char* FIXED_PASS_ALICE = "Poupoune";
static const char* FIXED_PASS_SYLVAIN = "31051989";

void applyFixedPasswords() {
  for (auto& u : g_users) {
    const char* pass = nullptr;
    if (u.id == "Alice")   pass = FIXED_PASS_ALICE;
    if (u.id == "Sylvain") pass = FIXED_PASS_SYLVAIN;
    if (!pass) continue;
    String salt;
    int dollar = u.passwordHash.indexOf('$');
    if (dollar > 0) salt = u.passwordHash.substring(0, dollar);
    else            salt = randomSalt();
    u.passwordHash = hashPassword(salt, String(pass));
  }
}

// ---------------------------------------------------------------------------
// Données par défaut (store V1 vierge)
// ---------------------------------------------------------------------------
static void loadDefaultStore() {
  g_users.clear();
  g_users.push_back({"Alice",   "", "user",  "Alice",   120});
  g_users.push_back({"Sylvain", "", "admin", "Sylvain", 120});

  g_wishes.clear();
  g_transactions.clear();
  g_notifications.clear();

  g_categories.clear();
  g_categories.push_back({"Romantisme",           "💞"});
  g_categories.push_back({"Moments à deux",       "🥂"});
  g_categories.push_back({"Surprises et sorties", "🎁"});
  g_categories.push_back({"Détente et évasion",   "🌿"});
  g_categories.push_back({"Intimité et complicité", "🔥"});
  g_categories.push_back({"Loisirs et passions",  "🎨"});

  g_levels.clear();
  g_levels.push_back({"Coloc",    0,  "🏠"});
  g_levels.push_back({"Complice", 5,  "🤝"});
  g_levels.push_back({"Équipe",   12, "⚡"});
  g_levels.push_back({"Connecté", 20, "💫"});
  g_levels.push_back({"In Love",  35, "💕"});

  g_nextWishId = 1;
  g_nextTransactionId = 1;
  g_nextNotificationId = 1;
}

// ---------------------------------------------------------------------------
// Lecture du JSON (V1)
// ---------------------------------------------------------------------------
static String strOr(const JsonVariant& v, const char* def = "") {
  if (v.isNull()) return String(def);
  return String(v.as<const char*>());
}

// Timestamp : compatible ISO (ancien format) et epoch (entier)
static bool jsonTime(const JsonVariant& v, time_t& out) {
  if (v.isNull()) return false;
  if (v.is<const char*>()) { out = parseISO8601(v.as<String>()); return true; }
  if (v.is<int>())  { out = (time_t)v.as<int>();  return true; }
  if (v.is<long>()) { out = (time_t)v.as<long>(); return true; }
  return false;
}

static void applyStoreFromJson(JsonDocument& doc) {
  std::vector<User> users;
  std::vector<Wish> wishes;
  std::vector<CreditTransaction> transactions;
  std::vector<Notification> notifications;
  std::vector<Category> categories;
  std::vector<RelLevel> levels;

  if (doc["users"].is<JsonArray>()) {
    for (JsonObject o : doc["users"].as<JsonArray>()) {
      User u;
      u.id = strOr(o["id"]);
      u.passwordHash = strOr(o["passwordHash"]);
      u.role = strOr(o["role"]);
      u.displayName = strOr(o["displayName"]);
      u.credits = o["credits"] | 0;
      if (u.credits < 0) u.credits = 0;
      users.push_back(u);
    }
  }
  if (doc["wishes"].is<JsonArray>()) {
    for (JsonObject o : doc["wishes"].as<JsonArray>()) {
      Wish w;
      w.id = strOr(o["id"]);
      w.userId = strOr(o["userId"]);
      w.title = strOr(o["title"]);
      w.description = strOr(o["description"]);
      w.category = strOr(o["category"]);
      w.credits = o["credits"] | 0;
      if (w.credits < 0) w.credits = 0;
      if (w.credits > 4) w.credits = 4;
      w.photo = strOr(o["photo"]);
      w.desiredDate = strOr(o["desiredDate"]);
      w.status = strOr(o["status"], "open");
      w.acceptedBy = strOr(o["acceptedBy"]);
      w.memoryComment = strOr(o["memoryComment"]);
      w.hasCreatedAt = jsonTime(o["createdAt"], w.createdAt);
      w.hasAcceptedAt = jsonTime(o["acceptedAt"], w.acceptedAt);
      w.hasRealizationReportedAt = jsonTime(o["realizationReportedAt"], w.realizationReportedAt);
      w.hasCompletedAt = jsonTime(o["completedAt"], w.completedAt);
      w.hasCancelledAt = jsonTime(o["cancelledAt"], w.cancelledAt);
      if (o["readBy"].is<JsonArray>()) {
        for (JsonVariant r : o["readBy"].as<JsonArray>()) w.readBy.push_back(r.as<String>());
      }
      wishes.push_back(w);
    }
  }
  if (doc["creditTransactions"].is<JsonArray>()) {
    for (JsonObject o : doc["creditTransactions"].as<JsonArray>()) {
      CreditTransaction t;
      t.id = strOr(o["id"]);
      t.userId = strOr(o["userId"]);
      t.amount = o["amount"] | 0;
      t.type = strOr(o["type"]);
      t.wishId = strOr(o["wishId"]);
      t.description = strOr(o["description"]);
      t.hasCreatedAt = jsonTime(o["createdAt"], t.createdAt);
      transactions.push_back(t);
    }
  }
  if (doc["notifications"].is<JsonArray>()) {
    for (JsonObject o : doc["notifications"].as<JsonArray>()) {
      Notification n;
      n.id = strOr(o["id"]);
      n.userId = strOr(o["userId"]);
      n.type = strOr(o["type"]);
      n.wishId = strOr(o["wishId"]);
      n.message = strOr(o["message"]);
      n.hasCreatedAt = jsonTime(o["createdAt"], n.createdAt);
      n.hasReadAt = jsonTime(o["readAt"], n.readAt);
      notifications.push_back(n);
    }
  }
  if (doc["categories"].is<JsonArray>()) {
    for (JsonObject o : doc["categories"].as<JsonArray>()) {
      Category c;
      c.name = strOr(o["name"]);
      c.icon = strOr(o["icon"]);
      if (!c.name.isEmpty()) categories.push_back(c);
    }
  }
  if (doc["relationshipLevels"].is<JsonArray>()) {
    for (JsonObject o : doc["relationshipLevels"].as<JsonArray>()) {
      RelLevel l;
      l.name = strOr(o["name"]);
      l.threshold = o["threshold"] | 0;
      l.icon = strOr(o["icon"]);
      if (l.threshold < 0) l.threshold = 0;
      levels.push_back(l);
    }
  }

  if (!users.empty()) g_users = users;
  g_wishes = wishes;
  g_transactions = transactions;
  g_notifications = notifications;
  if (!categories.empty()) g_categories = categories;
  if (!levels.empty()) g_levels = levels;
  g_nextWishId = doc["nextWishId"] | (int)(g_wishes.size() + 1);
  g_nextTransactionId = doc["nextTransactionId"] | (int)(g_transactions.size() + 1);
  g_nextNotificationId = doc["nextNotificationId"] | (int)(g_notifications.size() + 1);

  if (g_users.empty()) { // garde : jamais sans utilisateur
    g_users.push_back({"Alice",   "", "user",  "Alice",   120});
    g_users.push_back({"Sylvain", "", "admin", "Sylvain", 120});
  }
  applyFixedPasswords();
}

// ---------------------------------------------------------------------------
// Migration V0 (boutique/négociations) -> V1 (envies)
// - Sauvegarde brute intégrale : /data_v0_backup.json
// - Archivage des données commerciales : /legacy.json (exportable via l'admin)
// - Conservation : utilisateurs, rôles, soldes de crédits
// ---------------------------------------------------------------------------
static void migrateV0(const String& raw, JsonDocument& doc) {
  Serial.println("[DATA] Format V0 detecte : migration vers V1");

  File b = LittleFS.open(V0_BACKUP_FILE, "w");
  if (b) { b.print(raw); b.close(); Serial.println(String("[DATA] Sauvegarde brute : ") + V0_BACKUP_FILE); }

  JsonDocument legacy;
  JsonObject lo = legacy.to<JsonObject>();
  lo["migratedAt"] = toISO8601(nowEpoch());
  if (doc["services"].is<JsonArray>())          lo["services"].set(doc["services"]);
  if (doc["offers"].is<JsonArray>())            lo["offers"].set(doc["offers"]);
  if (doc["giftVouchers"].is<JsonArray>())      lo["giftVouchers"].set(doc["giftVouchers"]);
  if (doc["specialEvents"].is<JsonArray>())     lo["specialEvents"].set(doc["specialEvents"]);
  if (doc["relationshipLevels"].is<JsonArray>()) lo["relationshipLevels"].set(doc["relationshipLevels"]);
  File lf = LittleFS.open(LEGACY_FILE, "w");
  if (lf) { serializeJson(legacy, lf); lf.close(); Serial.println(String("[DATA] Archive commerciale : ") + LEGACY_FILE); }

  // Store par défaut V1, puis conservation des utilisateurs/soldes
  loadDefaultStore();
  if (doc["users"].is<JsonArray>()) {
    std::vector<User> users;
    for (JsonObject o : doc["users"].as<JsonArray>()) {
      User u;
      u.id = strOr(o["id"]);
      u.displayName = strOr(o["displayName"], u.id.c_str());
      u.role = strOr(o["role"], "user");
      u.credits = o["credits"] | 0;
      if (u.credits < 0) u.credits = 0;
      users.push_back(u);
    }
    if (!users.empty()) g_users = users;
  }
  applyFixedPasswords();
  saveStore();
  Serial.println("[DATA] Migration V0 -> V1 terminee");
}

// ---------------------------------------------------------------------------
// Restauration depuis /data.bak (coupure pendant une sauvegarde, corruption)
// ---------------------------------------------------------------------------
static bool restoreFromBak() {
  if (!LittleFS.exists(DATA_BAK)) return false;
  Serial.println("[DATA] Restauration depuis /data.bak");
  LittleFS.remove(DATA_TMP);
  if (!LittleFS.rename(DATA_BAK, DATA_TMP)) return false;
  return LittleFS.rename(DATA_TMP, DATA_FILE);
}

void loadStore() {
  loadDefaultStore();
  applyFixedPasswords(); // garantit des hash valides même sans data.json
  for (int attempt = 0; attempt < 2; attempt++) {
    if (!LittleFS.exists(DATA_FILE)) {
      if (attempt == 0 && restoreFromBak()) continue;
      Serial.println("[DATA] data.json absent : donnees par defaut");
      return;
    }
    File f = LittleFS.open(DATA_FILE, "r");
    if (!f) { Serial.println("[DATA] ERREUR : ouverture data.json impossible"); return; }
    String raw = f.readString();
    f.close();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, raw);
    if (err) {
      Serial.printf("[DATA] Erreur JSON (%s)\n", err.c_str());
      if (attempt == 0 && restoreFromBak()) continue;
      Serial.println("[DATA] data.json illisible : donnees par defaut");
      return;
    }
    // Format V0 = boutique (services/offers) sans envies
    bool isV0 = (doc["services"].is<JsonArray>() || doc["offers"].is<JsonArray>())
                && !doc["wishes"].is<JsonArray>();
    if (isV0) {
      migrateV0(raw, doc);
      return;
    }
    applyStoreFromJson(doc);
    Serial.println("[DATA] data.json V1 charge");
    return;
  }
}

// ---------------------------------------------------------------------------
// Sérialisation V1 (sauvegarde + export admin)
// ---------------------------------------------------------------------------
static void addOptStr(JsonObject& o, const char* key, const String& v) {
  if (!v.isEmpty()) o[key] = v;
}

void saveStoreTo(JsonDocument& doc) {
  JsonObject root = doc.to<JsonObject>();

  JsonArray users = root["users"].to<JsonArray>();
  for (auto& u : g_users) {
    JsonObject o = users.add<JsonObject>();
    o["id"] = u.id;
    o["passwordHash"] = u.passwordHash;
    o["role"] = u.role;
    o["displayName"] = u.displayName;
    o["credits"] = u.credits;
  }

  JsonArray wishes = root["wishes"].to<JsonArray>();
  for (auto& w : g_wishes) {
    JsonObject o = wishes.add<JsonObject>();
    o["id"] = w.id;
    o["userId"] = w.userId;
    o["title"] = w.title;
    addOptStr(o, "description", w.description);
    addOptStr(o, "category", w.category);
    o["credits"] = w.credits;
    addOptStr(o, "photo", w.photo);
    addOptStr(o, "desiredDate", w.desiredDate);
    o["status"] = w.status;
    addOptStr(o, "acceptedBy", w.acceptedBy);
    addOptStr(o, "memoryComment", w.memoryComment);
    if (w.hasCreatedAt) o["createdAt"] = toISO8601(w.createdAt);
    if (w.hasAcceptedAt) o["acceptedAt"] = toISO8601(w.acceptedAt);
    if (w.hasRealizationReportedAt) o["realizationReportedAt"] = toISO8601(w.realizationReportedAt);
    if (w.hasCompletedAt) o["completedAt"] = toISO8601(w.completedAt);
    if (w.hasCancelledAt) o["cancelledAt"] = toISO8601(w.cancelledAt);
    if (!w.readBy.empty()) {
      JsonArray rb = o["readBy"].to<JsonArray>();
      for (auto& r : w.readBy) rb.add(r);
    }
  }

  JsonArray tx = root["creditTransactions"].to<JsonArray>();
  for (auto& t : g_transactions) {
    JsonObject o = tx.add<JsonObject>();
    o["id"] = t.id;
    o["userId"] = t.userId;
    o["amount"] = t.amount;
    o["type"] = t.type;
    addOptStr(o, "wishId", t.wishId);
    o["description"] = t.description;
    if (t.hasCreatedAt) o["createdAt"] = toISO8601(t.createdAt);
  }

  JsonArray notifs = root["notifications"].to<JsonArray>();
  for (auto& n : g_notifications) {
    JsonObject o = notifs.add<JsonObject>();
    o["id"] = n.id;
    o["userId"] = n.userId;
    o["type"] = n.type;
    addOptStr(o, "wishId", n.wishId);
    o["message"] = n.message;
    if (n.hasCreatedAt) o["createdAt"] = toISO8601(n.createdAt);
    if (n.hasReadAt) o["readAt"] = toISO8601(n.readAt);
  }

  JsonArray cats = root["categories"].to<JsonArray>();
  for (auto& c : g_categories) {
    JsonObject o = cats.add<JsonObject>();
    o["name"] = c.name;
    o["icon"] = c.icon;
  }

  JsonArray levels = root["relationshipLevels"].to<JsonArray>();
  for (auto& l : g_levels) {
    JsonObject o = levels.add<JsonObject>();
    o["name"] = l.name;
    o["threshold"] = l.threshold;
    o["icon"] = l.icon;
  }

  root["nextWishId"] = g_nextWishId;
  root["nextTransactionId"] = g_nextTransactionId;
  root["nextNotificationId"] = g_nextNotificationId;
  root["schemaVersion"] = 1;
}

// ---------------------------------------------------------------------------
// Sauvegarde atomique : /data.tmp -> vérification -> remplacement de /data.json
// (la version précédente est conservée dans /data.bak)
// ---------------------------------------------------------------------------
void saveStore() {
  JsonDocument doc;
  saveStoreTo(doc);
  File f = LittleFS.open(DATA_TMP, "w");
  if (!f) { Serial.println("[DATA] ERREUR : ouverture /data.tmp impossible"); return; }
  serializeJsonPretty(doc, f);
  f.close();

  // Vérification : le fichier temporaire doit se relire sans erreur
  File v = LittleFS.open(DATA_TMP, "r");
  JsonDocument check;
  DeserializationError err = deserializeJson(check, v);
  v.close();
  if (err) {
    Serial.printf("[DATA] ERREUR : /data.tmp invalide (%s), sauvegarde annulee\n", err.c_str());
    LittleFS.remove(DATA_TMP);
    return;
  }

  LittleFS.remove(DATA_BAK);
  if (LittleFS.exists(DATA_FILE)) LittleFS.rename(DATA_FILE, DATA_BAK);
  if (!LittleFS.rename(DATA_TMP, DATA_FILE)) {
    Serial.println("[DATA] ERREUR : remplacement impossible, restauration de /data.bak");
    if (!LittleFS.exists(DATA_FILE) && LittleFS.exists(DATA_BAK)) LittleFS.rename(DATA_BAK, DATA_FILE);
    return;
  }
  Serial.println("[DATA] Donnees sauvegardees (atomique)");
}

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
  // Import réservé au format V1 (le format V0 reste dans /legacy.json)
  if (!doc["users"].is<JsonArray>() || !doc["wishes"].is<JsonArray>()) return false;
  applyStoreFromJson(doc);
  saveStore();
  return true;
}

bool hasLegacyArchive() {
  return LittleFS.exists(LEGACY_FILE);
}

String exportLegacyJSON() {
  if (!hasLegacyArchive()) return "";
  File f = LittleFS.open(LEGACY_FILE, "r");
  if (!f) return "";
  String out = f.readString();
  f.close();
  return out;
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

Wish* findWish(const String& id) {
  for (auto& w : g_wishes) if (w.id == id) return &w;
  return nullptr;
}

// ---------------------------------------------------------------------------
// Logique métier
// ---------------------------------------------------------------------------
int countCompletedWishes() {
  int n = 0;
  for (auto& w : g_wishes) if (w.status == "completed") n++;
  return n;
}

void getRelationshipData(RelData& out) {
  out.completedCount = countCompletedWishes();
  out.levelIndex = 0;
  for (int i = (int)g_levels.size() - 1; i >= 0; i--) {
    if (out.completedCount >= g_levels[i].threshold) { out.levelIndex = i; break; }
  }
  out.nextIndex = (out.levelIndex + 1 < (int)g_levels.size()) ? out.levelIndex + 1 : -1;
  out.nextThreshold = (out.nextIndex >= 0) ? g_levels[out.nextIndex].threshold : 0;
}

// Attention : push_back peut réallouer le vecteur -> ré-acquérir le pointeur
Wish* createWish(const String& userId, const String& title, const String& description,
                 const String& category, int credits, const String& desiredDate) {
  Wish w;
  w.id = String(g_nextWishId++);
  w.userId = userId;
  w.title = title;
  w.description = description;
  w.category = category;
  w.credits = credits;
  if (w.credits < 0) w.credits = 0;
  if (w.credits > 4) w.credits = 4;
  w.desiredDate = desiredDate;
  w.status = "open";
  w.hasCreatedAt = true;
  w.createdAt = nowEpoch();
  g_wishes.push_back(w);
  return &g_wishes.back();
}

CreditTransaction* addTransaction(const String& userId, int amount, const String& type,
                                  const String& wishId, const String& description) {
  CreditTransaction t;
  t.id = String(g_nextTransactionId++);
  t.userId = userId;
  t.amount = amount;
  t.type = type;
  t.wishId = wishId;
  t.description = description;
  t.hasCreatedAt = true;
  t.createdAt = nowEpoch();
  g_transactions.push_back(t);
  return &g_transactions.back();
}

Notification* addNotification(const String& userId, const String& type,
                              const String& wishId, const String& message) {
  Notification n;
  n.id = String(g_nextNotificationId++);
  n.userId = userId;
  n.type = type;
  n.wishId = wishId;
  n.message = message;
  n.hasCreatedAt = true;
  n.createdAt = nowEpoch();
  g_notifications.push_back(n);
  return &g_notifications.back();
}

// Rubrique de navigation concernée par chaque type de notification
static const char* rubriqueForType(const String& type) {
  if (type == "new_wish") return RUBRIQUE_PARTNER;
  if (type == "wish_accepted" || type == "realization_reported" ||
      type == "wish_abandoned" || type == "wish_cancelled") return RUBRIQUE_EXCHANGES;
  if (type == "realization_confirmed") return RUBRIQUE_MOMENTS;
  return "";
}

int countNotifications(const String& userId) {
  int n = 0;
  for (auto& notif : g_notifications) {
    if (notif.userId == userId && !notif.hasReadAt) n++;
  }
  return n;
}

int countUnreadForRubrique(const String& userId, const char* rubrique) {
  int n = 0;
  for (auto& notif : g_notifications) {
    if (notif.userId != userId || notif.hasReadAt) continue;
    if (rubriqueForType(notif.type) == rubrique) n++;
  }
  return n;
}

void markRubriqueNotificationsRead(const String& userId, const char* rubrique) {
  bool changed = false;
  for (auto& notif : g_notifications) {
    if (notif.userId != userId || notif.hasReadAt) continue;
    if (rubriqueForType(notif.type) == rubrique) {
      notif.hasReadAt = true;
      notif.readAt = nowEpoch();
      changed = true;
    }
  }
  if (changed) saveStore();
}

// ---------------------------------------------------------------------------
// Sessions
// ---------------------------------------------------------------------------
static Session g_sessions[LM_SESSION_MAX];
static int g_sessionCursor = 0;

static String randomToken() {
  static const char hex[] = "0123456789abcdef";
  char buf[33];
  for (int i = 0; i < 16; i++) {
    buf[i * 2]     = hex[(int)random(16)];
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
