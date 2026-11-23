#pragma once
#include <Arduino.h>
#include <vector>

// ---------------------------------------------------------------------------
// Modèle de données - portage ESP32 de l'application Love Market (Node/Express)
// Le format data.json est identique à celui de la version Node.
// ---------------------------------------------------------------------------

struct User {
  String id;
  String password;
  String role;
  String displayName;
  int credits = 0;
};

struct Service {
  String id;
  String sellerId;
  String title;
  String description;
  String category;
  int price = 0;
  String icon;
};

struct Offer {
  String id;
  String serviceId;          // null -> vide
  String fromUserId;
  String toUserId;
  int offeredPrice = 0;
  String comment;
  String status;             // pending | accepted | rejected | countered | realized
  String type;               // purchase | negotiation | request | donation | special_event
  String serviceTitle;
  String serviceCategory;
  String serviceIcon;
  String originalRequesterId;
  String parentOfferId;
  String rootOfferId;
  bool isSuperService = false;
  int superServiceBonus = 0;
  int debitedAmount = -1;    // -1 = absent (seulement pour les achats)
  String eventId;
  bool hasCreatedAt = false;
  time_t createdAt = 0;
  String feedback;           // happy | neutral | sad
  bool hasFeedbackAt = false;
  time_t feedbackAt = 0;
  std::vector<String> readBy;
};

struct Voucher {
  String id;
  String code;
  int credits = 0;
  String comment;
  bool used = false;
  String usedBy;
  bool hasCreatedAt = false;
  time_t createdAt = 0;
  bool hasUsedAt = false;
  time_t usedAt = 0;
};

struct SpecialEvent {
  String id;
  String title;
  String description;
  int credits = 0;
  String targetUser;         // both | Alice | Sylvain
  String status;             // active | completed | cancelled
  bool hasCreatedAt = false;
  time_t createdAt = 0;
  String assignedTo;
  String completedBy;
  bool hasCompletedAt = false;
  time_t completedAt = 0;
};

struct RelLevel {
  String name;
  int minCredits = 0;
  String icon;
};

// Données de niveau de couple calculées
struct RelData {
  int levelIndex = 0;
  int totalCredits = 0;
  int nextIndex = -1;        // -1 = niveau maximum atteint
};

// --- Store global (mêmes clés que le data.json de la version Node) ---
extern std::vector<User> g_users;
extern std::vector<Service> g_services;
extern std::vector<Offer> g_offers;
extern std::vector<Voucher> g_vouchers;
extern std::vector<SpecialEvent> g_events;
extern std::vector<RelLevel> g_levels;
extern int g_nextServiceId;
extern int g_nextOfferId;
extern int g_nextVoucherId;
extern int g_nextEventId;

// --- Chargement / sauvegarde (LittleFS /data.json) ---
void loadStore();
void saveStore();
String exportStoreJSON();             // sérialisation compacte (export admin)
bool importStoreJSON(const String&);  // remplace tout le store, retourne true si OK

// --- Temps ---
time_t nowEpoch();                    // heure de compilation du firmware + uptime
String formatDateTimeFR(time_t, bool withSeconds = false); // jj/mm/aaaa hh:mm[:ss]
String toISO8601(time_t);
time_t parseISO8601(const String&);

// --- Recherches ---
User* findUser(const String& id);
User* findPartnerOf(const String& userId); // premier utilisateur d'id différent
Service* findService(const String& id);
Offer* findOffer(const String& id);
Voucher* findVoucherByCode(const String& code);
SpecialEvent* findEvent(const String& id);

// --- Logique métier ---
void getRelationshipData(RelData& out);
int countNotifications(const String& userId);      // badge de négociations non lues
int superServiceBonus(int basePrice);              // Math.round(p * 0.3)
Offer* createOffer(const String& fromUserId, const String& toUserId, int price,
                   const String& type, const String& comment); // id/createdAt auto

// --- Sessions (cookie lm_sid, stockage RAM, flash messages par session) ---
#define LM_SESSION_MAX 6
struct Session {
  bool active = false;
  String token;
  String userId;
  String flashError;    // consommé à l'affichage de la page suivante
  String flashSuccess;
};
Session* sessionCreate(const String& userId);
Session* sessionFind(const String& token);
void sessionDestroy(const String& token);
