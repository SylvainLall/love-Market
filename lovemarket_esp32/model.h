#pragma once
#include <Arduino.h>
#include <vector>

// ---------------------------------------------------------------------------
// Modèle de données V1 - Love Market « envies » (refonte)
//
// Cycle de vie d'une envie :
//   open -> accepted -> completed   (crédits attribués à la confirmation,
//                                    uniquement par le bénéficiaire)
//   accepted -> open                (abandon par le réalisateur)
//   accepted -> cancelled           (annulation par l'auteur, aucun crédit)
// ---------------------------------------------------------------------------

struct User {
  String id;
  String passwordHash;   // format "sel$sha256hex"
  String role;
  String displayName;
  int credits = 0;
};

struct Wish {
  String id;
  String userId;         // auteur = bénéficiaire de l'envie
  String title;
  String description;
  String category;
  int credits = 0;       // 0..4 : récompense du réalisateur
  String photo;          // réservé V1.1 (jamais rempli en V1)
  String desiredDate;    // texte libre (« dès que possible », « le 12 juin »...)
  String status;         // open | accepted | completed | cancelled
  String acceptedBy;     // réalisateur (vide si open)
  String memoryComment;  // commentaire facultatif sur le moment réalisé
  bool hasCreatedAt = false;             time_t createdAt = 0;
  bool hasAcceptedAt = false;            time_t acceptedAt = 0;
  bool hasRealizationReportedAt = false; time_t realizationReportedAt = 0;
  bool hasCompletedAt = false;           time_t completedAt = 0;
  bool hasCancelledAt = false;           time_t cancelledAt = 0;
  std::vector<String> readBy;            // partenaires ayant vu l'envie publiée
};

struct CreditTransaction {
  String id;
  String userId;         // bénéficiaire des crédits
  int amount = 0;
  String type;           // wish_completed | admin_adjustment
  String wishId;         // facultatif
  String description;
  bool hasCreatedAt = false; time_t createdAt = 0;
};

struct Notification {
  String id;
  String userId;
  String type;           // new_wish | wish_accepted | realization_reported |
                         // wish_abandoned | realization_confirmed | wish_cancelled
  String wishId;         // facultatif
  String message;
  bool hasCreatedAt = false; time_t createdAt = 0;
  bool hasReadAt = false;    time_t readAt = 0;
};

struct Category {
  String name;
  String icon;
};

struct RelLevel {
  String name;
  int threshold = 0;     // nombre d'envies confirmées réalisées (cumulatif)
  String icon;
};

// Progression du couple calculée
struct RelData {
  int levelIndex = 0;
  int completedCount = 0;
  int nextIndex = -1;    // -1 = niveau maximum atteint
  int nextThreshold = 0;
};

// Rubriques de navigation (badges de notifications)
#define RUBRIQUE_PARTNER   "partner"    // Ses envies (nouvelle envie du partenaire)
#define RUBRIQUE_EXCHANGES "exchanges"  // Nos échanges (acceptation, abandon, signalement, annulation)
#define RUBRIQUE_MOMENTS   "moments"    // Nos moments (réalisation confirmée)

// --- Store global (format data.json V1) ---
extern std::vector<User> g_users;
extern std::vector<Wish> g_wishes;
extern std::vector<CreditTransaction> g_transactions;
extern std::vector<Notification> g_notifications;
extern std::vector<Category> g_categories;
extern std::vector<RelLevel> g_levels;
extern int g_nextWishId;
extern int g_nextTransactionId;
extern int g_nextNotificationId;

// --- Chargement / sauvegarde (LittleFS /data.json, écriture atomique) ---
void loadStore();
void saveStore();
String exportStoreJSON();             // sérialisation compacte (export admin)
bool importStoreJSON(const String&);  // remplace tout le store V1, true si OK
bool hasLegacyArchive();              // /legacy.json présent (données V0 archivées)
String exportLegacyJSON();            // contenu de /legacy.json ("" si absent)

// --- Temps ---
time_t nowEpoch();                    // heure de compilation du firmware + uptime
String formatDateTimeFR(time_t, bool withSeconds = false); // jj/mm/aaaa hh:mm[:ss]
String toISO8601(time_t);
time_t parseISO8601(const String&);

// --- Recherches ---
User* findUser(const String& id);
User* findPartnerOf(const String& userId); // premier utilisateur d'id différent
Wish* findWish(const String& id);

// --- Mots de passe (SHA-256 + sel, mbedTLS) ---
String sha256Hex(const String& data);
String hashPassword(const String& salt, const String& password); // "salt$hex"
bool verifyPassword(User& u, const String& password);
void applyFixedPasswords(); // réapplique les mots de passe fixes après chargement/import

// --- Logique métier ---
void getRelationshipData(RelData& out);
int countCompletedWishes();
Wish* createWish(const String& userId, const String& title, const String& description,
                 const String& category, int credits, const String& desiredDate);
CreditTransaction* addTransaction(const String& userId, int amount, const String& type,
                                  const String& wishId, const String& description);
Notification* addNotification(const String& userId, const String& type,
                              const String& wishId, const String& message);
int countNotifications(const String& userId);                       // toutes non lues
int countUnreadForRubrique(const String& userId, const char* rubrique); // badge
void markRubriqueNotificationsRead(const String& userId, const char* rubrique);

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
